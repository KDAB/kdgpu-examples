#pragma once

#include <GltfHolder/gltf_holder_global.h>
#include <GltfHolder/render_permutation/gltf_render_permutation.h>

namespace kdgpu_ext::gltf_holder {

struct GltfRenderPermutations {
    std::vector<GltfRenderPermutation> permutations;

    PipelineLayout pipelineLayout;

    void initialize_pipeline_layout(
            const shader_specification::GltfShaderTextureChannels &shaderTextureChannels,
            const std::vector<PushConstantRange> pushConstantRanges)
    {
        PipelineLayoutOptions pipelineLayoutOptions;

        // the order in which the bind group layouts are specified here matters
        pipelineLayoutOptions.bindGroupLayouts.resize(shaderTextureChannels.shaderBindGroupSets.size());

        for (size_t i = 0; i < shaderTextureChannels.shaderBindGroupSets.size(); ++i) {
            auto& bind_group_set = shaderTextureChannels.shaderBindGroupSets[i];
            if (bind_group_set.type == shader_specification::GltfShaderBindGroupType::PassOwnedUniformBuffer) {
                pipelineLayoutOptions.bindGroupLayouts[bind_group_set.shaderSet] = *bind_group_set.bindGroupLayout;
                continue;
            }

            if (bind_group_set.type == shader_specification::GltfShaderBindGroupType::GltfNodeTransform) {
                pipelineLayoutOptions.bindGroupLayouts[bind_group_set.shaderSet] = GltfHolderGlobal::instance().nodeTransformBindGroupLayout();
                continue;
            }

            if (bind_group_set.type == shader_specification::GltfShaderBindGroupType::GltfShaderMaterialTextureChannels) {
                pipelineLayoutOptions.bindGroupLayouts[bind_group_set.shaderSet] = shaderTextureChannels.materialBindGroupLayout;
            }
        }

        for (auto &push_constant_range : pushConstantRanges)
            pipelineLayoutOptions.pushConstantRanges.push_back(push_constant_range);

        pipelineLayout = graphics::GlobalResources::instance().graphicsDevice().createPipelineLayout(pipelineLayoutOptions);
    }

    void create_pipelines(const RenderTarget& renderTarget,
        const std::vector<ShaderStage>& shaderStages,
        const shader_specification::GltfShaderVertexInput& shaderVertexInput
        )
    {
        for (auto &permutation : permutations) {
            permutation.create_pipelines(
                    renderTarget,
                    shaderStages,
                    shaderVertexInput,
                    pipelineLayout);
        }
    }

    void add_permutation(
        GltfHolder &holder,
        const shader_specification::GltfShaderTextureChannels& textureChannels,
        uint32_t vertexUniformNodeTransformSet
        )
    {
        GltfRenderPermutation permutation;
        permutation.holder = &holder;
        permutation.initializeShaderTextureChannels(textureChannels);
        permutation.nodeTransformUniformSet = vertexUniformNodeTransformSet;
        permutations.push_back(std::move(permutation));
    }

    void deinitialize()
    {
        for (auto &permutation : permutations)
            permutation.deinitialize();

        pipelineLayout = {};
    }

    void render(RenderPassCommandRecorder& renderPass)
    {
        for (auto &permutation : permutations)
            permutation.holder->renderAllNodes(renderPass, permutation, pipelineLayout);
    }
};
} // namespace kdgpu_ext::gltf_holder
