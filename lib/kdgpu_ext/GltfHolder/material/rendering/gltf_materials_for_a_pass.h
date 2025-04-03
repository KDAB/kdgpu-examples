#pragma once

#include <GltfHolder/shader_specification/gltf_shader_texture_channels.h>
#include <GltfHolder/material/rendering/gltf_render_pass_texture_bindings.h>

namespace gltf_holder::material::rendering {
class GltfMaterialsForAPass
{
public:
    // read all materials from gltf holder (immutable source)
    // combine with shader material demands and textures

    void initialize(
            tinygltf::Model &model,
            std::vector<GltfTexture> &textures,
            const kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels &shaderMaterial)
    {
        if (!shaderMaterial.anyTextureChannelsFromGltfEnabled)
            return;

        m_shaderMaterial = &shaderMaterial;

        // for each material in the gltf model,
        // - set up which textures to use based on the requirements in the shader material
        for (auto &material : model.materials) {
            GltfRenderPassTextureBindings renderMaterial;
            if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
                if (shaderMaterial.baseColorFragmentShaderBinding.has_value()) {
                    renderMaterial.baseColorTexture = &textures[material.pbrMetallicRoughness.baseColorTexture.index];
                }
            }
            if (material.normalTexture.index != -1) {
                if (shaderMaterial.baseColorFragmentShaderBinding.has_value()) {
                    renderMaterial.normalMapTexture = &textures[material.normalTexture.index];
                }
            }
            if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
                if (shaderMaterial.metallicRoughnessFragmentShaderBinding.has_value()) {
                    renderMaterial.metallicRoughnessTexture = &textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index];
                }
            }
            if (material.occlusionTexture.index != -1) {
                if (shaderMaterial.occlusionFragmentShaderBinding.has_value()) {
                    renderMaterial.occlusionTexture = &textures[material.occlusionTexture.index];
                }
            }
            if (material.emissiveTexture.index != -1) {
                if (shaderMaterial.emissionFragmentShaderBinding.has_value()) {
                    renderMaterial.emissionTexture = &textures[material.emissiveTexture.index];
                }
            }

            // initialize the material (creates a bind group for it)
            renderMaterial.initialize(shaderMaterial);

            // add the material to the list
            m_renderMaterials.push_back(std::move(renderMaterial));
        }
    }

    void deinitialize()
    {
        for (auto& material: m_renderMaterials) {
            material.deinitialize();
        }
    }

    GltfRenderPassTextureBindings &materialByIndex(size_t index)
    {
        return m_renderMaterials[index];
    }

    bool has_shader_material() { return m_shaderMaterial != nullptr; }
    const kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels &get_shader_material() const { return *m_shaderMaterial; }

private:
    const kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels* m_shaderMaterial = nullptr;

    std::vector<GltfRenderPassTextureBindings> m_renderMaterials;
};
}
