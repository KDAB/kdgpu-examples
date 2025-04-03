#pragma once

#include <tinygltf_helper/camera.h>

#include <texture_target/texture_target.h>
#include <GltfHolder/render_permutation/gltf_render_permutations.h>

#include <pass/gltf_other_channel/uniform_buffer/camera.h>
#include <pass/gltf_other_channel/uniform_buffer/configuration.h>
#include <uniform/uniform_buffer_object.h>

#include <GltfHolder/material/rendering/gltf_materials_for_a_pass.h>

#include <texture/texture_set.h>
#include <shader/shader_vertex_fragment.h>

namespace pass::gltf_other_channel {
class GltfOtherChannelPass
{
public:
    void addGltfHolder(kdgpu_ext::gltf_holder::GltfHolder &holder);
    void initializeShader();

    void initialize(TextureTarget& depthTexture, Extent2D swapchainExtent);
    void resize(Extent2D swapChainExtent);
    void deinitialize();

    void setIntensity(float intensity);
    void updateViewProjectionMatricesFromCamera(TinyGltfHelper::Camera& camera);

    void render(CommandRecorder& commandRecorder);


    TextureTarget& resultColorTextureTarget() { return m_colorTextureTarget; }

    kdgpu_ext::graphics::texture::TextureUniformSet* m_textureSet = nullptr;

private:
    // shader
    kdgpu_ext::graphics::shader::ShaderVertexFragment m_shader;

    // material
    kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels m_shaderTextureChannels;

    // object render permutations
    kdgpu_ext::gltf_holder::GltfRenderPermutations m_gltfRenderPermutations;

    // uniform buffer for vertex shader
    UniformBufferObject<geometry::uniform_buffer::camera, ShaderStageFlagBits::VertexBit> m_cameraUniformBufferObject = {};
    UniformBufferObject<geometry::uniform_buffer::configuration, ShaderStageFlagBits::VertexBit> m_configurationUniformBufferObject = {};

    // depth texture from previous pass
    TextureTarget* m_depthTextureTarget = nullptr;

    // results - textures
    TextureTarget m_colorTextureTarget;

    // result - render target
    RenderTarget m_regularRenderTarget;

};

}
