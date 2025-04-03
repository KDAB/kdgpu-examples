#include "gltf_other_channel_pass.h"

#include "shader/regular_mesh.h"

#include <KDUtils/dir.h>

namespace pass::gltf_other_channel {
void GltfOtherChannelPass::addGltfHolder(kdgpu_ext::gltf_holder::GltfHolder &holder)
{
    m_gltfRenderPermutations.add_permutation(
            holder,
            m_shaderTextureChannels,
            pass::gltf_other_channel::shader::gltf_other_channel::vertexUniformGltfNodeTransformSet);
}

void GltfOtherChannelPass::initializeShader()
{
    // init the shader material
    m_shaderTextureChannels = pass::gltf_other_channel::shader::gltf_other_channel::createShaderMaterial();
    m_shaderTextureChannels.initializeBindGroupLayout();

    m_shader.setBaseDirectory("gltf/gltf_render_pbr/shader");
    m_shader.loadVertexShader("gltf_other_channel.vert.glsl.spv");
    m_shader.loadFragmentShader("gltf_other_channel.frag.glsl.spv");
}

void GltfOtherChannelPass::initialize(TextureTarget& depthTexture, Extent2D swapchainExtent)
{
    m_colorTextureTarget.initialize(swapchainExtent, Format::R8G8B8A8_UNORM, TextureUsageFlagBits::ColorAttachmentBit);
    m_depthTextureTarget = &depthTexture;

    m_cameraUniformBufferObject.init(shader::gltf_other_channel::vertexUniformPassCameraBinding);
    m_configurationUniformBufferObject.init(shader::gltf_other_channel::vertexUniformPassConfigurationBinding);

    // setup render targets
    m_regularRenderTarget.setColorTarget(&m_colorTextureTarget);
    m_regularRenderTarget.setDepthTarget(m_depthTextureTarget);

    // only render when depth buffer is matched with the depth from previous pass
    {
        m_regularRenderTarget.depthTestEnabled = true;
        m_regularRenderTarget.depthWriteEnabled = false;
        m_regularRenderTarget.depthCompareOperation = CompareOperation::Equal;
    }

    // connect each custom/per-pass uniform buffer set with its layout
    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        pass::gltf_other_channel::shader::gltf_other_channel::vertexUniformPassCameraSet,
        &m_cameraUniformBufferObject.bindGroupLayout());

    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
    pass::gltf_other_channel::shader::gltf_other_channel::vertexUniformPassConfigurationSet,
    &m_configurationUniformBufferObject.bindGroupLayout());

    m_shaderTextureChannels.setBindGroupLayoutForBindGroup(
        pass::gltf_other_channel::shader::gltf_other_channel::fragmentUniformPassOtherChannelTextureSet,
        &m_textureSet->bindGroupLayout());

    // initialize pipeline layout for internal gltf and pass-specific uniform buffers
    m_gltfRenderPermutations.initialize_pipeline_layout(m_shaderTextureChannels, {});
    m_gltfRenderPermutations.create_pipelines(m_regularRenderTarget, m_shader.shaderStages(), shader::gltf_other_channel::createShaderVertexInput());
}

void GltfOtherChannelPass::resize(Extent2D swapChainExtent)
{
    m_colorTextureTarget.resize(swapChainExtent);
    m_regularRenderTarget.setColorTarget(&m_colorTextureTarget);
}

void GltfOtherChannelPass::deinitialize()
{
    m_gltfRenderPermutations.deinitialize();
    m_shaderTextureChannels.deinitialize();
    m_shader.deinitialize();
    m_configurationUniformBufferObject = {};
    m_cameraUniformBufferObject = {};
    m_colorTextureTarget.deinitialize();
    m_depthTextureTarget = nullptr;
}

void GltfOtherChannelPass::setIntensity(float intensity)
{
    m_configurationUniformBufferObject.data.intensity = intensity;
    m_configurationUniformBufferObject.upload();
}

void GltfOtherChannelPass::updateViewProjectionMatricesFromCamera(TinyGltfHelper::Camera &camera)
{
    m_cameraUniformBufferObject.data.view = camera.viewMatrix;
    m_cameraUniformBufferObject.data.projection = camera.lens().projectionMatrix;
    m_cameraUniformBufferObject.upload();
}

void GltfOtherChannelPass::render(CommandRecorder &commandRecorder)
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
            .view = m_depthTextureTarget->textureView(),
            .depthLoadOperation = AttachmentLoadOperation::DontCare // don't clear the depth buffer
            }
        }
    );
    // clang-format on

    // set global bind groups (descriptor set)
    render_pass.setBindGroup(pass::gltf_other_channel::shader::gltf_other_channel::vertexUniformPassCameraSet, m_cameraUniformBufferObject.bindGroup(), m_gltfRenderPermutations.pipelineLayout);
    render_pass.setBindGroup(pass::gltf_other_channel::shader::gltf_other_channel::vertexUniformPassConfigurationSet, m_configurationUniformBufferObject.bindGroup(), m_gltfRenderPermutations.pipelineLayout);
    render_pass.setBindGroup(pass::gltf_other_channel::shader::gltf_other_channel::fragmentUniformPassOtherChannelTextureSet, m_textureSet->bindGroup(), m_gltfRenderPermutations.pipelineLayout);

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

} // namespace pass::gltf_other_channel
