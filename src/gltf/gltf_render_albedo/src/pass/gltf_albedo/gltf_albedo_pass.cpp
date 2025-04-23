/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "gltf_albedo_pass.h"

#include "shader/regular_mesh.h"

#include <KDUtils/dir.h>

namespace pass::gltf_albedo {
void GltfAlbedoPass::addGltfHolder(kdgpu_ext::gltf_holder::GltfHolder &holder)
{
    m_gltfRenderPermutations.add_permutation(
            holder,
            m_shaderTextureChannels,
            pass::gltf_albedo::shader::gltf_albedo::vertexUniformGltfNodeTransformSet);
}

void GltfAlbedoPass::initializeShader()
{
    // init the shader material
    m_shaderTextureChannels = pass::gltf_albedo::shader::gltf_albedo::createShaderMaterial();
    m_shaderTextureChannels.initializeBindGroupLayout();

    m_shader.setBaseDirectory("gltf/gltf_render_albedo/shader/");
    m_shader.loadVertexShader("gltf_albedo.vert.glsl.spv");
    m_shader.loadFragmentShader("gltf_albedo.frag.glsl.spv");
}

void GltfAlbedoPass::initialize(Format depthFormat, Extent2D swapchainExtent, const kdgpu_ext::graphics::camera::Camera &camera)
{
    m_colorTextureTarget.initialize(swapchainExtent, Format::R8G8B8A8_UNORM, TextureUsageFlagBits::ColorAttachmentBit);
    m_depthTextureTarget.initialize(swapchainExtent, depthFormat, TextureUsageFlagBits::DepthStencilAttachmentBit);

    // setup render targets
    m_regularRenderTarget.setColorTarget(&m_colorTextureTarget);
    m_regularRenderTarget.setDepthTarget(&m_depthTextureTarget);

    // connect each custom/per-pass uniform buffer set with its layout
    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        pass::gltf_albedo::shader::gltf_albedo::vertexUniformPassCameraSet,
        &camera.bindGroupLayout());
    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        pass::gltf_albedo::shader::gltf_albedo::fragmentUniformPassLutTexturesSet,
        &m_textureSet->bindGroupLayout());

    // initialize pipeline layout for internal gltf and pass-specific uniform buffers
    m_gltfRenderPermutations.initialize_pipeline_layout(m_shaderTextureChannels, {});
    m_gltfRenderPermutations.create_pipelines(m_regularRenderTarget, m_shader.shaderStages(), shader::gltf_albedo::createShaderVertexInput());
}

void GltfAlbedoPass::resize(Extent2D swapChainExtent)
{
    m_colorTextureTarget.resize(swapChainExtent);
    m_regularRenderTarget.setColorTarget(&m_colorTextureTarget);
}

void GltfAlbedoPass::deinitialize()
{
    m_gltfRenderPermutations.deinitialize();
    m_shaderTextureChannels.deinitialize();
    m_shader.deinitialize();
    m_colorTextureTarget.deinitialize();
    m_depthTextureTarget.deinitialize();
}

void GltfAlbedoPass::render(CommandRecorder &commandRecorder, const kdgpu_ext::graphics::camera::Camera &camera)
{
    // clang-format off
    auto render_pass = commandRecorder.beginRenderPass(
        {
        .colorAttachments =
            {
                {
                    .view = m_regularRenderTarget.colorTarget()->textureView(), // render to the texture
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
            .view = m_depthTextureTarget.textureView()
            }
        }
    );
    // clang-format on

    // set global bind groups (descriptor set)
    render_pass.setBindGroup(pass::gltf_albedo::shader::gltf_albedo::vertexUniformPassCameraSet, camera.bindGroup(), m_gltfRenderPermutations.pipelineLayout);
    render_pass.setBindGroup(pass::gltf_albedo::shader::gltf_albedo::fragmentUniformPassLutTexturesSet, m_textureSet->bindGroup(), m_gltfRenderPermutations.pipelineLayout);

    // render
    m_gltfRenderPermutations.render(render_pass);

    // finish render pass
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
}
