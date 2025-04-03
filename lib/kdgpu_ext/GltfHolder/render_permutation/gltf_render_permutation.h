#pragma once

#include <GltfHolder/gltf_holder.h>

// shader specification
#include <GltfHolder/shader_specification/gltf_shader_vertex_input.h>
#include <GltfHolder/shader_specification/gltf_shader_texture_channels.h>

#include <GltfHolder/material/rendering/gltf_materials_for_a_pass.h>

namespace kdgpu_ext::gltf_holder {
/**
 * A render permutation is the combination of:
 * - a gltf model
 * - a specif render pass with specific texture channel requirements
 * - a specific pipeline layout (target textures such as color+depth or just depth)
 */
struct GltfRenderPermutation {
    GltfHolder *holder = nullptr;
    render_mesh_set::RenderMeshSet meshSet;

    // materials for this pass/permutation
    ::gltf_holder::material::rendering::GltfMaterialsForAPass materials;

    // descriptor set index for the per-node uniform buffer object binding
    // which transforms each node by a matrix
    uint32_t nodeTransformUniformSet = 0;

    // initializes the unique material setup for the pass and the gltf model
    void initializeShaderTextureChannels(const shader_specification::GltfShaderTextureChannels & shadingMaterial)
    {
        materials.initialize(
            holder->model(),
            holder->textures(),
            shadingMaterial
            );
    }

    void deinitialize()
    {
        materials.deinitialize();
        meshSet.deinitialize();
    }

    void create_pipelines(
            const RenderTarget &renderTarget,
            std::vector<ShaderStage> shaderStages,
            const shader_specification::GltfShaderVertexInput & shaderVertexInput,
            PipelineLayout& pipelineLayout)
    {
        holder->createGraphicsRenderingPipelinesForMeshSet(
                meshSet,
                renderTarget,
                shaderStages,
                shaderVertexInput,
                pipelineLayout);
    }
};
}
