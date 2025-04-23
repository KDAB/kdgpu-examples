/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "basic_geometry_pass.h"

#include <KDUtils/dir.h>

#include <pass/basic_geometry/shader/basic_geometry.h>

namespace pass::basic_geometry {

void BasicGeometryPass::initialize(TextureTarget &depthTexture, const Extent2D swapchainExtent, const kdgpu_ext::graphics::camera::Camera &camera)
{
    auto &device = kdgpu_ext::graphics::GlobalResources::instance().graphicsDevice();

    m_shader.setBaseDirectory("gltf/gltf_render_pbr/shader");
    m_shader.loadVertexShader("basic_geometry.vert.glsl.spv");
    m_shader.loadFragmentShader("basic_geometry.frag.glsl.spv");

    // initialize quad data
    {
        float quad_size_x = 0.8f;
        float quad_size_y = 0.5f;
        float quad_offset_y = 0.4f;
        float z_pos = 0.7f;
        m_quad.topology = PrimitiveTopology::TriangleStrip;
        m_quad.vertices = {
            { -quad_size_x, quad_size_y + quad_offset_y, z_pos },
            { quad_size_x, quad_size_y + quad_offset_y, z_pos },
            { -quad_size_x, -quad_size_y + quad_offset_y, z_pos },
            { quad_size_x, -quad_size_y + quad_offset_y, z_pos }
        };
        m_quad.textureCoordinates = {
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 0.0f, 1.0f },
            { 1.0f, 1.0f }
        };
        m_quad.initializeFromData();
    }

    // Create a sampler to sample from the color texture in the input pass
    m_colorOutputSampler = device.createSampler();

    m_colorTextureTarget.initialize(swapchainExtent, Format::R8G8B8A8_UNORM, TextureUsageFlagBits::ColorAttachmentBit);
    m_depthTextureTarget = &depthTexture;

    // setup render targets
    m_renderTarget.setColorTarget(&m_colorTextureTarget);
    m_renderTarget.setDepthTarget(m_depthTextureTarget);

    // render and write to depth buffer for other passes to react to
    {
        m_renderTarget.depthTestEnabled = true;
        m_renderTarget.depthWriteEnabled = true;
        m_renderTarget.depthCompareOperation = CompareOperation::Less;
    }

    // Create a pipeline layout (array of bind group layouts)
    const PipelineLayoutOptions pipelineLayoutOptions = {
        .bindGroupLayouts = {
                m_textureSet->bindGroupLayout(),
                camera.bindGroupLayout(),
        },
    };
    m_pipelineLayout = device.createPipelineLayout(pipelineLayoutOptions);

    // Create a pipeline
    // clang-format off
    GraphicsPipelineOptions pipelineOptions = {
        .shaderStages = m_shader.shaderStages(),
        .layout = m_pipelineLayout,

        .renderTargets = {
            { .format = m_colorTextureTarget.format() }
        },
        .depthStencil = {
            .format = m_depthTextureTarget->format(),
            .depthTestEnabled = true,
            .depthWritesEnabled = true,
            .depthCompareOperation = CompareOperation::Less
        },
        .primitive = {
            .cullMode = CullModeFlagBits::None,
        },
    };
    // clang-format on

    // add quad to pipeline options
    m_quad.addToPipelineOptions(pipelineOptions, shader::basic_geometry::vertexInVertexPositionLocation, shader::basic_geometry::vertexInVertexTextureCoordinateLocation);

    // create graphics pipeline
    m_graphicsPipeline = device.createGraphicsPipeline(pipelineOptions);
}

void BasicGeometryPass::deinitialize()
{
    m_shader.deinitialize();
    m_quad.deinitialize();
    m_colorOutputSampler = {};
    m_colorTextureTarget.deinitialize();
    m_depthTextureTarget = nullptr;
    m_renderTarget = {};
    m_bindGroup = {};
    m_bindGroupLayout = {};
    m_pipelineLayout = {};
    m_graphicsPipeline = {};
}

void BasicGeometryPass::render(CommandRecorder &commandRecorder, const kdgpu_ext::graphics::camera::Camera &camera)
{
    auto render_pass = commandRecorder.beginRenderPass(
        {
        .colorAttachments =
            {
                {
                    .view = m_renderTarget.colorTarget()->textureView(), // render to the texture
                    .clearValue =
                    {
                    0.0f,
                    0.0f,
                    0.0f,
                    1.0f
                    },
                }
            },
        .depthStencilAttachment =
            {
            .view = m_depthTextureTarget->textureView(),
            .depthLoadOperation = AttachmentLoadOperation::DontCare // don't clear the depth buffer
            }
        }
    );
    render_pass.setPipeline(m_graphicsPipeline);
    render_pass.setBindGroup(pass::basic_geometry::shader::basic_geometry::fragmentUniformPassColorTextureSet, m_textureSet->bindGroup(), m_pipelineLayout);
    render_pass.setBindGroup(pass::basic_geometry::shader::basic_geometry::vertexUniformPassCameraSet, camera.bindGroup(), m_pipelineLayout);

    m_quad.render(render_pass);
    render_pass.end();

    // transition color texture to shader read optimal
    commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
            .srcStages = PipelineStageFlagBit::ColorAttachmentOutputBit,
            .srcMask = AccessFlagBit::ColorAttachmentWriteBit,
            .dstStages = PipelineStageFlags(PipelineStageFlagBit::FragmentShaderBit),
            .dstMask = AccessFlags(AccessFlagBit::ShaderReadBit),
            .oldLayout = TextureLayout::ColorAttachmentOptimal,
            .newLayout = TextureLayout::ShaderReadOnlyOptimal,
            .texture = m_colorTextureTarget.texture(),
            .range = {
                    .aspectMask = TextureAspectFlagBits::ColorBit,
                    .levelCount = 1,
            },
    });
}

} // namespace pass::basic_geometry
