/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "gltf_area_light_pass.h"

#include "shader/gltf_area_light.h"

#include <KDUtils/dir.h>

namespace pass::gltf_area_light {
void GltfAreaLightPass::addGltfHolder(kdgpu_ext::gltf_holder::GltfHolder &holder)
{
    m_gltfRenderPermutations.add_permutation(
            holder,
            m_shaderTextureChannels,
            shader::gltf_area_light::vertexUniformGltfNodeTransformSet);
}

void GltfAreaLightPass::initializeShader()
{
    // init the shader material
    m_shaderTextureChannels = shader::gltf_area_light::create_shader_material();
    m_shaderTextureChannels.initializeBindGroupLayout();

    m_shader.setBaseDirectory("gltf/gltf_render_pbr/shader");
    m_shader.loadVertexShader("gltf_area_light.vert.glsl.spv");
    m_shader.loadFragmentShader("gltf_area_light.frag.glsl.spv");
}

void GltfAreaLightPass::initialize(TextureTarget& depth_texture, Extent2D swapchainExtent)
{
    m_colorTextureTarget.initialize(swapchainExtent, Format::R8G8B8A8_UNORM, TextureUsageFlagBits::ColorAttachmentBit);
    m_depthTextureTarget = &depth_texture;

    m_cameraUniformBufferObject.init(shader::gltf_area_light::vertexUniformPassCameraBinding);
    m_configurationUniformBufferObject.init(shader::gltf_area_light::fragmentUniformPassConfigurationBinding);

    // setup render targets
    m_renderTarget.setColorTarget(&m_colorTextureTarget);
    m_renderTarget.setDepthTarget(m_depthTextureTarget);

    // only render when depth buffer is matched with the depth from previous pass
    {
        m_renderTarget.depthTestEnabled = true;
        m_renderTarget.depthWriteEnabled = false;
        m_renderTarget.depthCompareOperation = CompareOperation::Equal;
    }

    // connect each custom/per-pass uniform buffer set with its layout
    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        shader::gltf_area_light::vertexUniformPassCameraSet,
        &m_cameraUniformBufferObject.bindGroupLayout());

    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        shader::gltf_area_light::fragmentUniformPassLtcTexturesSet,
        &m_ltcTextureSet->bindGroupLayout());

    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        shader::gltf_area_light::fragmentUniformPassConfigurationSet,
        &m_configurationUniformBufferObject.bindGroupLayout());

    // initialize pipeline layout for internal gltf and pass-specific uniform buffers
    m_gltfRenderPermutations.initialize_pipeline_layout(m_shaderTextureChannels, {});
    m_gltfRenderPermutations.create_pipelines(m_renderTarget, m_shader.shaderStages(), shader::gltf_area_light::create_shader_vertex_input());
}

void GltfAreaLightPass::resize(Extent2D swapChainExtent)
{
    m_colorTextureTarget.resize(swapChainExtent);
    m_renderTarget.setColorTarget(&m_colorTextureTarget);
}

void GltfAreaLightPass::deinitialize()
{
    m_gltfRenderPermutations.deinitialize();
    m_shaderTextureChannels.deinitialize();
    m_shader.deinitialize();
    m_cameraUniformBufferObject = {};
    m_configurationUniformBufferObject = {};
    m_colorTextureTarget.deinitialize();
    m_depthTextureTarget = nullptr;
}

void GltfAreaLightPass::setQuadVertices(std::vector<glm::vec3> &vertices)
{
    m_configurationUniformBufferObject.data.lightQuadPoints[0] = glm::vec4(vertices[0], 1.0);
    m_configurationUniformBufferObject.data.lightQuadPoints[1] = glm::vec4(vertices[1], 1.0);
    m_configurationUniformBufferObject.data.lightQuadPoints[2] = glm::vec4(vertices[2], 1.0);
    m_configurationUniformBufferObject.data.lightQuadPoints[3] = glm::vec4(vertices[3], 1.0);
}

void GltfAreaLightPass::setLightIntensity(float genericIntensity, float specularIntensity)
{
    m_configurationUniformBufferObject.data.lightConfiguration.x = genericIntensity;
    m_configurationUniformBufferObject.data.lightConfiguration.z = specularIntensity;
}

void GltfAreaLightPass::updateViewProjectionMatricesFromCamera(TinyGltfHelper::Camera &camera)
{
    m_cameraUniformBufferObject.data.view = camera.viewMatrix;
    m_cameraUniformBufferObject.data.projection = camera.lens().projectionMatrix;
    m_cameraUniformBufferObject.upload();

    auto eye_position = camera.eyePosition();
    m_configurationUniformBufferObject.data.eyePosition.x = eye_position.x;
    m_configurationUniformBufferObject.data.eyePosition.y = eye_position.y;
    m_configurationUniformBufferObject.data.eyePosition.z = eye_position.z;
    m_configurationUniformBufferObject.data.eyePosition.w = 1.0f;
    m_configurationUniformBufferObject.upload();
}

void GltfAreaLightPass::render(CommandRecorder &commandRecorder)
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
            .view = m_depthTextureTarget->textureView(),
            .depthLoadOperation = AttachmentLoadOperation::DontCare // don't clear the depth buffer
            }
        }
    );
    // clang-format on

    // set global bind groups (descriptor set)
    render_pass.setBindGroup(shader::gltf_area_light::vertexUniformPassCameraSet, m_cameraUniformBufferObject.bindGroup(), m_gltfRenderPermutations.pipelineLayout);
    render_pass.setBindGroup(shader::gltf_area_light::fragmentUniformPassLtcTexturesSet, m_ltcTextureSet->bindGroup(), m_gltfRenderPermutations.pipelineLayout);
    render_pass.setBindGroup(shader::gltf_area_light::fragmentUniformPassConfigurationSet, m_configurationUniformBufferObject.bindGroup(), m_gltfRenderPermutations.pipelineLayout);

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
