/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "gltf_pbr_pass.h"

#include "shader/gltf_pbr.h"

#include <KDUtils/dir.h>

namespace pass::gltf_pbr {
void gltfPbrPass::addGltfHolder(kdgpu_ext::gltf_holder::GltfHolder &holder)
{
    m_gltfRenderPermutations.add_permutation(
            holder,
            m_shaderTextureChannels,
            shader::gltf_pbr::vertexUniformGltfNodeTransformSet);
}

void gltfPbrPass::initializeShader()
{
    // init the shader material
    m_shaderTextureChannels = shader::gltf_pbr::createShaderMaterial();
    m_shaderTextureChannels.initializeBindGroupLayout();

    m_shader.setBaseDirectory("gltf/gltf_render_pbr/shader");
    m_shader.loadVertexShader("gltf_pbr.vert.glsl.spv");
    m_shader.loadFragmentShader("gltf_pbr.frag.glsl.spv");
}

void gltfPbrPass::initialize(Format depthFormat, Extent2D swapchainExtent)
{
    m_colorTextureTarget.initialize(swapchainExtent, Format::R8G8B8A8_UNORM, TextureUsageFlagBits::ColorAttachmentBit);
    m_depthTextureTarget.initialize(swapchainExtent, depthFormat, TextureUsageFlagBits::DepthStencilAttachmentBit);

    m_cameraUniformBufferObject.init(shader::gltf_pbr::vertexUniformPassCameraBinding);
    m_materialUniformBufferObject.init(shader::gltf_pbr::fragmentUniformPassMaterialBinding);

    // setup render targets
    m_renderTarget.setColorTarget(&m_colorTextureTarget);
    m_renderTarget.setDepthTarget(&m_depthTextureTarget);

    // connect each custom/per-pass uniform buffer set with its layout
    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        shader::gltf_pbr::vertexUniformPassCameraSet,
        &m_cameraUniformBufferObject.bindGroupLayout());

    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        shader::gltf_pbr::fragmentUniformPassConfigurationSet,
        &m_materialUniformBufferObject.bindGroupLayout());

    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        shader::gltf_pbr::fragmentUniformPassLutTexturesSet,
        &m_pbrTextureSet->bindGroupLayout());

    // initialize pipeline layout for internal gltf and pass-specific uniform buffers
    m_gltfRenderPermutations.initialize_pipeline_layout(m_shaderTextureChannels, {});
    m_gltfRenderPermutations.create_pipelines(m_renderTarget, m_shader.shaderStages(), shader::gltf_pbr::createShaderVertexInput());
}

void gltfPbrPass::resize(Extent2D swapChainExtent)
{
    m_colorTextureTarget.resize(swapChainExtent);
    m_renderTarget.setColorTarget(&m_colorTextureTarget);
}


void gltfPbrPass::deinitialize()
{
    m_gltfRenderPermutations.deinitialize();
    m_shaderTextureChannels.deinitialize();
    m_shader.deinitialize();
    m_cameraUniformBufferObject = {};
    m_materialUniformBufferObject = {};
    m_colorTextureTarget.deinitialize();
    m_depthTextureTarget.deinitialize();
}

void gltfPbrPass::setDirectionalLightIntensity(float intensity)
{
    m_materialUniformBufferObject.data.directionalLightIntensity = intensity;
}

void gltfPbrPass::setIblIntensity(float intensity)
{
    m_materialUniformBufferObject.data.iblIntensity = intensity;
}

void gltfPbrPass::updateViewProjectionMatricesFromCamera(TinyGltfHelper::Camera &camera)
{
    m_cameraUniformBufferObject.data.view = camera.viewMatrix;
    m_cameraUniformBufferObject.data.projection = camera.lens().projectionMatrix;
    m_cameraUniformBufferObject.upload();

    // here, the base color multiplier can be set every frame
    m_materialUniformBufferObject.data.baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    m_materialUniformBufferObject.upload();
}

void gltfPbrPass::render(CommandRecorder &commandRecorder)
{
    // clang-format off
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
            .view = m_depthTextureTarget.textureView()
            }
        }
    );
    // clang-format on

    // set global bind groups (descriptor set)
    render_pass.setBindGroup(shader::gltf_pbr::vertexUniformPassCameraSet, m_cameraUniformBufferObject.bindGroup(), m_gltfRenderPermutations.pipelineLayout);
    render_pass.setBindGroup(shader::gltf_pbr::fragmentUniformPassLutTexturesSet, m_pbrTextureSet->bindGroup(), m_gltfRenderPermutations.pipelineLayout);
    render_pass.setBindGroup(shader::gltf_pbr::fragmentUniformPassConfigurationSet, m_materialUniformBufferObject.bindGroup(), m_gltfRenderPermutations.pipelineLayout);

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
