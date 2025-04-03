#pragma once

#include <global_resources.h>

#include <KDGpu/bind_group_options.h>

#include <GltfHolder/shader_specification/gltf_shader_texture_channels.h>
#include <GltfHolder/texture/gltf_texture.h>

namespace gltf_holder::material::rendering {
/**
 * Holds the glue between what texture channels a shader has specified
 * and the actual shared texture resources beloning to the loaded gltf model.
 */
struct GltfRenderPassTextureBindings {

    KDGpu::BindGroup &bindGroup()
    {
        return m_bindGroup;
    }

    bool areAllTexturesValidForUse()
    {
        if (baseColorTexture)
            if (!baseColorTexture->isValid())
                return false;
        if (normalMapTexture)
            if (!normalMapTexture->isValid())
                return false;
        if (metallicRoughnessTexture)
            if (!metallicRoughnessTexture->isValid())
                return false;
        if (occlusionTexture)
            if (!occlusionTexture->isValid())
                return false;
        if (emissionTexture)
            if (!emissionTexture->isValid())
                return false;
        return true;
    }

    // does not own the textures, those are owned by gltf_holder
    GltfTexture *baseColorTexture = nullptr;
    GltfTexture *normalMapTexture = nullptr;
    GltfTexture *metallicRoughnessTexture = nullptr;
    GltfTexture *occlusionTexture = nullptr;
    GltfTexture *emissionTexture = nullptr;

    void initialize(const kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels &textureChannels)
    {
        KDGpu::BindGroupOptions bindGroupOptions;
        // the layout is re-used
        bindGroupOptions.layout = textureChannels.materialBindGroupLayout;

        if (textureChannels.baseColorFragmentShaderBinding.has_value() && baseColorTexture != nullptr) {
            bindGroupOptions.resources.push_back(
                    // KDGpu::BindGroupEntry
                    {
                            // this binding matches in the layout, see gltf_shader_texture_channels
                            .binding = static_cast<uint32_t>(textureChannels.baseColorFragmentShaderBinding.value()),
                            .resource = KDGpu::TextureViewSamplerBinding{
                                    .textureView = baseColorTexture->textureViewHandle(),
                                    .sampler = baseColorTexture->samplerHandle() } });
        }
        if (textureChannels.normalMapFragmentShaderBinding.has_value() && normalMapTexture != nullptr) {
            bindGroupOptions.resources.push_back(
                    // KDGpu::BindGroupEntry
                    {
                            // this binding matches in the layout, see gltf_shader_texture_channels
                            .binding = static_cast<uint32_t>(textureChannels.normalMapFragmentShaderBinding.value()),
                            .resource = KDGpu::TextureViewSamplerBinding{
                                    .textureView = normalMapTexture->textureViewHandle(),
                                    .sampler = normalMapTexture->samplerHandle() } });
        }
        if (textureChannels.metallicRoughnessFragmentShaderBinding.has_value() && metallicRoughnessTexture != nullptr) {
            bindGroupOptions.resources.push_back(
                    // KDGpu::BindGroupEntry
                    {
                            // this binding matches in the layout, see gltf_shader_texture_channels
                            .binding = static_cast<uint32_t>(textureChannels.metallicRoughnessFragmentShaderBinding.value()),
                            .resource = KDGpu::TextureViewSamplerBinding{
                                    .textureView = metallicRoughnessTexture->textureViewHandle(),
                                    .sampler = metallicRoughnessTexture->samplerHandle() } });
        }
        if (textureChannels.occlusionFragmentShaderBinding.has_value() && occlusionTexture != nullptr) {
            bindGroupOptions.resources.push_back(
                    // KDGpu::BindGroupEntry
                    {
                            // this binding matches in the layout, see gltf_shader_texture_channels
                            .binding = static_cast<uint32_t>(textureChannels.occlusionFragmentShaderBinding.value()),
                            .resource = KDGpu::TextureViewSamplerBinding{
                                    .textureView = occlusionTexture->textureViewHandle(),
                                    .sampler = occlusionTexture->samplerHandle() } });
        }
        if (textureChannels.emissionFragmentShaderBinding.has_value() && emissionTexture != nullptr) {
            bindGroupOptions.resources.push_back(
                    // KDGpu::BindGroupEntry
                    {
                            // this binding matches in the layout, see gltf_shader_texture_channels
                            .binding = static_cast<uint32_t>(textureChannels.emissionFragmentShaderBinding.value()),
                            .resource = KDGpu::TextureViewSamplerBinding{
                                    .textureView = emissionTexture->textureViewHandle(),
                                    .sampler = emissionTexture->samplerHandle() } });
        }

        m_bindGroup = kdgpu_ext::graphics::GlobalResources::instance().graphicsDevice().createBindGroup(bindGroupOptions);
    }

    void deinitialize()
    {
        m_bindGroup = {};
    }

    KDGpu::BindGroup m_bindGroup{};
};
} // namespace gltf_holder::material::rendering
