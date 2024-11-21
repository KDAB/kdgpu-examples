/*
This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "render_texture_to_screen_render_pass.h"

#include <KDGpu/buffer_options.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpuExample/kdgpuexample.h>
#include <KDUtils/dir.h>

void RenderTextureToScreenRenderPass::initialize(
    Device &device,
    Format depthFormat,
    Format swapchainFormat
)
{
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

    // Create a sampler to sample from the color texture in the final pass
    m_colorOutputSampler = device.createSampler();

    // Load vertex shader and fragment shader (spir-v only for now)
    m_shader.setBaseDirectory("compositing/gaussian_blur/shader");
    m_shader.loadVertexShader("desaturate.vert.glsl.spv");
    m_shader.loadFragmentShader("desaturate.frag.glsl.spv");

    // Set up filter push constant range
    m_filterPosPushConstantRange = {
        .offset = 0,
        .size = sizeof(float),
        .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
    };

    // Create bind group layout consisting of a single binding holding the texture the 1st pass rendered to
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
        .pushConstantRanges = { m_filterPosPushConstantRange }
    };
    // clang-format on
    m_pipelineLayout = device.createPipelineLayout(pipelineLayoutOptions);

    // Create a pipeline
    // clang-format off
    const GraphicsPipelineOptions pipelineOptions = {
        .shaderStages = m_shader.shaderStages(),
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
            { .format = swapchainFormat }
        },
        .depthStencil = {
            .format = depthFormat,
            .depthWritesEnabled = true,
            .depthCompareOperation = CompareOperation::Less
        },
        .primitive = {
            .topology = PrimitiveTopology::TriangleStrip
        }
    };
    // clang-format on
    m_graphicsPipeline = device.createGraphicsPipeline(pipelineOptions);

    // allocate room for the parameters to be sent to shader
    m_filterPosData.resize(sizeof(float));
}

void RenderTextureToScreenRenderPass::deinitialize()
{
    m_shader = {};
    m_fullScreenQuad = {};
    m_bindGroup = {};
    m_bindGroupLayout = {};
    m_colorOutputSampler = {};
    m_graphicsPipeline = {};
    m_pipelineLayout = {};
}

void RenderTextureToScreenRenderPass::update(float time)
{
    m_filterPos = 0.5f * (std::sin(time) + 1.0f);
    std::memcpy(m_filterPosData.data(), &m_filterPos, sizeof(float));
}

void RenderTextureToScreenRenderPass::setSourceTexture(Device &device, TextureTarget &sourceTexture)
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

void RenderTextureToScreenRenderPass::render(CommandRecorder& commandRecorder, TextureView& depthTextureView, TextureView& swapchainView)
{
    auto renderPass = commandRecorder.beginRenderPass({
        .colorAttachments = {
            {
                .view = swapchainView,
                .clearValue = { 0.3f, 0.3f, 0.3f, 1.0f },
                .finalLayout = TextureLayout::PresentSrc
            }
        },
        .depthStencilAttachment = {
            .view = depthTextureView
        }
    });
    renderPass.setPipeline(m_graphicsPipeline);
    renderPass.setVertexBuffer(0, m_fullScreenQuad);
    renderPass.setBindGroup(0, m_bindGroup);
    renderPass.pushConstant(m_filterPosPushConstantRange, m_filterPosData.data());
    renderPass.draw(DrawCommand{ .vertexCount = 4 });
    renderPass.end();
}
