#pragma once
#include <global_resources.h>

#include <KDGpu/bind_group_layout_options.h>

#include <GltfHolder/shader_specification/gltf_shader_bind_group_set.h>

namespace kdgpu_ext::gltf_holder::shader_specification {

// shader texture channels dictates:
// - which textures to load into a material for a pass
// - which descriptor set id is to be used
// - bindings for each sampler
struct GltfShaderTextureChannels
{
    // configures how to interpret each set in the shader
    std::vector<GltfShaderBindGroupSet> shaderBindGroupSets;

    bool anyTextureChannelsFromGltfEnabled = false;

    union
    {
        size_t materialTextureChannelDescriptorSet = 0; // vulkan and GLSL notation
        size_t bindGroup; // KDGpu notation
    };

    std::optional<size_t> baseColorFragmentShaderBinding = std::nullopt;
    std::optional<size_t> normalMapFragmentShaderBinding = std::nullopt;
    std::optional<size_t> metallicRoughnessFragmentShaderBinding = std::nullopt;
    std::optional<size_t> occlusionFragmentShaderBinding = std::nullopt;
    std::optional<size_t> emissionFragmentShaderBinding = std::nullopt;

    void initializeBindGroupLayout()
    {
        KDGpu::BindGroupLayoutOptions bindGroupLayoutOptions;
        if (baseColorFragmentShaderBinding.has_value()) {
            bindGroupLayoutOptions.bindings.push_back({
                .binding = static_cast<uint32_t>(baseColorFragmentShaderBinding.value()), // set from outside to match shader
                .resourceType = KDGpu::ResourceBindingType::CombinedImageSampler,
                .shaderStages = KDGpu::ShaderStageFlags(KDGpu::ShaderStageFlagBits::FragmentBit)
            });
            anyTextureChannelsFromGltfEnabled = true;
        }
        if (normalMapFragmentShaderBinding.has_value()) {
            bindGroupLayoutOptions.bindings.push_back({
                .binding = static_cast<uint32_t>(normalMapFragmentShaderBinding.value()), // set from outside to match shader
                .resourceType = KDGpu::ResourceBindingType::CombinedImageSampler,
                .shaderStages = KDGpu::ShaderStageFlags(KDGpu::ShaderStageFlagBits::FragmentBit)
            });
            anyTextureChannelsFromGltfEnabled = true;
        }
        if (metallicRoughnessFragmentShaderBinding.has_value()) {
            bindGroupLayoutOptions.bindings.push_back({
                .binding = static_cast<uint32_t>(metallicRoughnessFragmentShaderBinding.value()), // set from outside to match shader
                .resourceType = KDGpu::ResourceBindingType::CombinedImageSampler,
                .shaderStages = KDGpu::ShaderStageFlags(KDGpu::ShaderStageFlagBits::FragmentBit)
            });
            anyTextureChannelsFromGltfEnabled = true;
        }
        if (occlusionFragmentShaderBinding.has_value()) {
            bindGroupLayoutOptions.bindings.push_back({
                .binding = static_cast<uint32_t>(occlusionFragmentShaderBinding.value()), // set from outside to match shader
                .resourceType = KDGpu::ResourceBindingType::CombinedImageSampler,
                .shaderStages = KDGpu::ShaderStageFlags(KDGpu::ShaderStageFlagBits::FragmentBit)
            });
            anyTextureChannelsFromGltfEnabled = true;
        }
        if (emissionFragmentShaderBinding.has_value()) {
            bindGroupLayoutOptions.bindings.push_back({
                .binding = static_cast<uint32_t>(emissionFragmentShaderBinding.value()), // set from outside to match shader
                .resourceType = KDGpu::ResourceBindingType::CombinedImageSampler,
                .shaderStages = KDGpu::ShaderStageFlags(KDGpu::ShaderStageFlagBits::FragmentBit)
            });
            anyTextureChannelsFromGltfEnabled = true;
        }

        materialBindGroupLayout = graphics::GlobalResources::instance().graphicsDevice().createBindGroupLayout(bindGroupLayoutOptions);
    }

    void deinitialize()
    {
        materialBindGroupLayout = {};
    }

    void setBindGroupLayoutForBindGroup(uint8_t bindGroupId, const KDGpu::BindGroupLayout *bindGroupLayout)
    {
        for (auto &set : shaderBindGroupSets) {
            if (set.shaderSet == bindGroupId) {
                set.bindGroupLayout = bindGroupLayout;
                return;
            }
        }

        assert(false);
    }

    KDGpu::BindGroupLayout materialBindGroupLayout;
};
}
