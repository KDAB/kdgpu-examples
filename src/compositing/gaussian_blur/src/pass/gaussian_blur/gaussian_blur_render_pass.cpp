/*
This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "gaussian_blur_render_pass.h"

#include <KDGpu/buffer_options.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpuExample/kdgpuexample.h>
#include <KDUtils/dir.h>

void GaussianBlurRenderPass::initialize(
    Device &device,
    TextureTarget *targetTexture,
    GaussianBlurDirection direction
)
{
    m_targetTexture = targetTexture;

    // Create a buffer to hold a full screen quad. This will be drawn as a triangle-strip (see pipeline creation below).
    {
        BufferOptions bufferOptions = {
            .size = 4 * (3 + 2) * sizeof(float),
            .usage = BufferUsageFlagBits::VertexBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu // allow mapping to CPU address space
        };
        m_fullScreenQuad = device.createBuffer(bufferOptions);

        std::array<float, 20> vertexData = {
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f, -1.0f, 0.0f, 1.0f, 0.0f
        };

        auto bufferData = m_fullScreenQuad.map();
        std::memcpy(bufferData, vertexData.data(), vertexData.size() * sizeof(float));
        m_fullScreenQuad.unmap();
    }

    // Create a sampler to sample from the color texture
    m_colorOutputSampler = device.createSampler();

    // Load vertex shader and fragment shader (spir-v only for now)
    m_shader.setBaseDirectory("compositing/gaussian_blur/shader");
    m_shader.loadVertexShader("gaussian_blur.vert.glsl.spv");
    m_shader.loadFragmentShader("gaussian_blur.frag.glsl.spv");

    // Set up push constants
    m_pushConstantRange = {
        .offset = 0,
        .size = sizeof(float) * 2,
        .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
    };

    // Create bind group layout consisting of a single binding holding the source texture
    {
        // clang-format off
        const BindGroupLayoutOptions bindGroupLayoutOptions = {
            .bindings = {{
                .binding = 0,
                .resourceType = ResourceBindingType::CombinedImageSampler,
                .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
            }}
        };
        // clang-format on
        m_bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutOptions);
    }

    // Create a pipeline layout (array of bind group layouts)
    // clang-format off
    const PipelineLayoutOptions pipelineLayoutOptions = {
        .bindGroupLayouts = { m_bindGroupLayout },
        .pushConstantRanges = {
            m_pushConstantRange
        }
    };
    // clang-format on
    m_pipelineLayout = device.createPipelineLayout(pipelineLayoutOptions);

    auto shaderStages = m_shader.shaderStages();
    shaderStages[1].specializationConstants = {
        {
            0,
            static_cast<int>(direction)
        }
    };
    // Create a pipeline
    // clang-format off
    const GraphicsPipelineOptions pipelineOptions = {
        .shaderStages = shaderStages,
        .layout = m_pipelineLayout,
        .vertex = {
            .buffers = {
                { .binding = 0, .stride = (3 + 2) * sizeof(float) }
            },
            .attributes = {
                // Position
                {
                    .location = 0,
                    .binding = 0,
                    .format = Format::R32G32B32_SFLOAT
                },

                // Texture coordinates
                {
                    .location = 1,
                    .binding = 0,
                    .format = Format::R32G32_SFLOAT,
                    .offset = 3 * sizeof(float)
                }
            }
        },
        .renderTargets = {
            { .format = targetTexture->format() }
        },
        .primitive = {
            .topology = PrimitiveTopology::TriangleStrip
        }
    };
    // clang-format on
    m_graphicsPipeline = device.createGraphicsPipeline(pipelineOptions);
}

void GaussianBlurRenderPass::deinitialize()
{
    m_shader = {};
    m_fullScreenQuad = {};
    m_bindGroup = {};
    m_bindGroupLayout = {};
    m_colorOutputSampler = {};
    m_graphicsPipeline = {};
    m_pipelineLayout = {};
}

void GaussianBlurRenderPass::render(CommandRecorder& commandRecorder)
{
    // clang-format off
    auto renderPass = commandRecorder.beginRenderPass({
        .colorAttachments = {
            {
                .view = m_targetTexture->textureView(),
                .clearValue = {
                    0.0f,
                    0.0f,
                    0.0f,
                    1.0f
                },
            }
        }
    });
    // clang-format on
    renderPass.setPipeline(m_graphicsPipeline);
    renderPass.setVertexBuffer(0, m_fullScreenQuad);
    renderPass.setBindGroup(0, m_bindGroup);
    renderPass.pushConstant(m_pushConstantRange, &m_pushConstantsData);
    renderPass.draw(DrawCommand{ .vertexCount = 4 });
    renderPass.end();

    // Wait for pass writes to offscreen texture to have been completed
    // Transition it to a shader read only layout
    commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
            .srcStages = PipelineStageFlagBit::ColorAttachmentOutputBit,
            .srcMask = AccessFlagBit::ColorAttachmentWriteBit,
            .dstStages = PipelineStageFlagBit::FragmentShaderBit,
            .dstMask = AccessFlagBit::ShaderReadBit,
            .oldLayout = TextureLayout::ColorAttachmentOptimal,
            .newLayout = TextureLayout::ShaderReadOnlyOptimal,
            .texture = m_targetTexture->texture(),
            .range = {
                    .aspectMask = TextureAspectFlagBits::ColorBit,
                    .levelCount = 1,
            },
    });
}

void GaussianBlurRenderPass::setSourceTexture(Device &device, TextureTarget &sourceTexture)
{
    // Create a bindGroup for the source texture
    // clang-format off
    m_bindGroup = device.createBindGroup({
        .layout = m_bindGroupLayout,
        .resources =
        {
            {
                .binding = 0,
                .resource = TextureViewSamplerBinding{
                    .textureView = sourceTexture.textureView(),
                    .sampler = m_colorOutputSampler
                }
            }
        }
    });
    // clang-format on
}
