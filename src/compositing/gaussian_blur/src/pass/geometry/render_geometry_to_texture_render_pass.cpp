/*
This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "render_geometry_to_texture_render_pass.h"

#include <KDGpu/buffer_options.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpuExample/kdgpuexample.h>
#include <KDUtils/dir.h>

#include <glm/gtx/transform.hpp>

#include <pass/geometry/shader/rotating_triangle.h>

void RenderGeometryToTextureRenderPass::initialize(Device &device, Format depthFormat, TextureTarget *targetTexture )
{
    m_targetTexture = targetTexture;

    struct Vertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    // Create a buffer to hold triangle vertex data
    {
        const BufferOptions bufferOptions = {
            .size = 3 * sizeof(Vertex), // 3 vertices * 2 attributes * 3 float components
            .usage = BufferUsageFlagBits::VertexBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
        };
        m_meshVertexBuffer = device.createBuffer(bufferOptions);

        const float r = 0.8f;
        std::array<Vertex, 3> vertexData = {
            { //
              // Bottom-left, red
              glm::vec3{ r * std::cos(7.0f * M_PI / 6.0f), -r * std::sin(7.0f * M_PI / 6.0f), 0.0f },
              glm::vec3{ 1.0f, 0.0, 0.0f },
              // Bottom-right, green
              glm::vec3{ r * std::cos(11.0f * M_PI / 6.0f), -r * std::sin(11.0f * M_PI / 6.0f), 0.0f },
              glm::vec3{ 0.0f, 1.0, 0.0f },
              // Top, blue
              glm::vec3{ 0.0f, -r, 0.0f },
              glm::vec3{ 0.0f, 0.0, 1.0f } }
        };

        auto bufferData = m_meshVertexBuffer.map();
        std::memcpy(bufferData, vertexData.data(), vertexData.size() * sizeof(Vertex));
        m_meshVertexBuffer.unmap();
    }

    // Create a buffer to hold the geometry index data
    {
        const BufferOptions bufferOptions = {
            .size = 3 * sizeof(uint32_t),
            .usage = BufferUsageFlagBits::IndexBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu
        };
        m_meshIndexBuffer = device.createBuffer(bufferOptions);
        std::vector<uint32_t> indexData = { 0, 1, 2 };
        auto bufferData = m_meshIndexBuffer.map();
        std::memcpy(bufferData, indexData.data(), indexData.size() * sizeof(uint32_t));
        m_meshIndexBuffer.unmap();
    }

    // Create a buffer to hold the transformation matrix
    m_modelTransformObject.data.modelMatrix = glm::mat4(1.0f);
    m_modelTransformObject.init(pass::geometry::shader::rotating_triangle::vertexUniformModelTransformBinding);

    // Load vertex shader and fragment shader (spir-v only for now)
    m_shader.setBaseDirectory("compositing/gaussian_blur/shader");
    m_shader.loadVertexShader("rotating_triangle.vert.glsl.spv");
    m_shader.loadFragmentShader("rotating_triangle.frag.glsl.spv");

    // Create a pipeline layout (array of bind group layouts)
    const PipelineLayoutOptions pipelineLayoutOptions = {
        .bindGroupLayouts = { m_modelTransformObject.bindGroupLayout() }
    };
    m_pipelineLayout = device.createPipelineLayout(pipelineLayoutOptions);

    // Create a pipeline
    // clang-format off
    const GraphicsPipelineOptions pipelineOptions = {
        .shaderStages = m_shader.shaderStages(),
        .layout = m_pipelineLayout,
        .vertex = {
            .buffers = {
                { .binding = 0, .stride = sizeof(Vertex) }
            },
            .attributes = {
                { .location = 0, .binding = 0, .format = Format::R32G32B32_SFLOAT }, // Position
                { .location = 1, .binding = 0, .format = Format::R32G32B32_SFLOAT, .offset = sizeof(glm::vec3) } // Color
            }
        },
        .renderTargets = {
            { .format = targetTexture->format() }
        },
        .depthStencil = {
            .format = depthFormat,
            .depthWritesEnabled = true,
            .depthCompareOperation = CompareOperation::Less
        }
    };
    // clang-format on
    m_regularGraphicsPipeline = device.createGraphicsPipeline(pipelineOptions);
}

void RenderGeometryToTextureRenderPass::resize(TextureTarget *targetTexture)
{
    m_targetTexture = targetTexture;
}

void RenderGeometryToTextureRenderPass::deinitialize()
{
    m_shader = {};
    m_regularGraphicsPipeline = {};
    m_pipelineLayout = {};
    m_meshVertexBuffer = {};
    m_meshIndexBuffer = {};
    m_modelTransformObject = {};
}

void RenderGeometryToTextureRenderPass::update()
{
    // Each frame we want to rotate the triangle a little
    static float angle = 0.0f;
    angle += 0.01f;
    if (angle > 360.0f)
        angle -= 360.0f;

    // update model transform matrix
    m_modelTransformObject.data.modelMatrix = glm::mat4(1.0f);
    m_modelTransformObject.data.modelMatrix = glm::rotate(m_modelTransformObject.data.modelMatrix, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
    m_modelTransformObject.upload();
}

void RenderGeometryToTextureRenderPass::render(CommandRecorder &commandRecorder, TextureView& depthTextureView)
{
    auto renderPass = commandRecorder.beginRenderPass({
        .colorAttachments = {
            {
                .view = m_targetTexture->textureView(), // render to the texture
                .clearValue = { 0.0f, 0.0f, 0.0f, 1.0f },
            }
        },
        .depthStencilAttachment = {
            .view = depthTextureView
        }
    });

    renderPass.setPipeline(m_regularGraphicsPipeline);
    renderPass.setVertexBuffer(0, m_meshVertexBuffer);
    renderPass.setIndexBuffer(m_meshIndexBuffer);

    // bind the uniform buffer
    renderPass.setBindGroup(pass::geometry::shader::rotating_triangle::vertexUniformModelTransformSet, m_modelTransformObject.bindGroup());
    renderPass.drawIndexed(DrawIndexedCommand{ .indexCount = 3 });
    renderPass.end();

    // Wait for writes to offscreen texture to have been completed
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
