#pragma once

#include <render_mesh_set/render_mesh_set.h>

#include <model/node_render_task.h>

#include <GltfHolder/texture/gltf_texture.h>

#include <GltfHolder/shader_specification/gltf_shader_vertex_input.h>

#include <texture_target/texture_target.h>
#include <render_target/render_target.h>

#include <tiny_gltf.h>


#include <KDGpu/buffer.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/pipeline_layout.h>

namespace kdgpu_ext::gltf_holder {
struct GltfRenderPermutation;

class GltfHolder
{
  friend struct GltfRenderPermutation;
public:
  void load(const std::string& filename, Queue& queue);
  void deinitialize();

  tinygltf::Model &model()
  {
    return m_model;
  }

  std::vector<GltfTexture>& textures()
  {
    return m_textures;
  }

  /**
   * Shader binding id of the uniform buffer object containing the per-node transform.
   * This has to be the same across all shaders wanting to transform the nodes.
   * @param nodeTransformUniformBinding usually 0
   */
  void setNodeTransformShaderBinding(size_t nodeTransformUniformBinding);

  void createGraphicsRenderingPipelinesForMeshSet(
    render_mesh_set::RenderMeshSet& renderMeshSet,
    const RenderTarget &renderTarget,
    std::vector<ShaderStage>& shaderStages,
    const shader_specification::GltfShaderVertexInput& shaderVertexInput,
    PipelineLayout& pipelineLayout
  );

  void update();

  void renderAllNodes(
    RenderPassCommandRecorder& renderPassCommandRecorder,
    GltfRenderPermutation& renderPermutation,
    const PipelineLayout& pipelineLayout);

private:

  void calculateWorldTransforms();
  render_mesh_set::PrimitiveData setupPrimitive(
    std::vector<ShaderStage>& shaderStages,
    const shader_specification::GltfShaderVertexInput& shaderVertexInput,
    std::vector<GraphicsPipeline>& pipelines,
    PipelineLayout& pipelineLayout,
    const RenderTarget &renderTarget,
    const tinygltf::Primitive &primitive
  );

  tinygltf::Model m_model;

  std::vector<GltfTexture> m_textures;

  // the set can differ between passes but the binding should be the same
  uint32_t m_nodeTransformUniformBinding = 0;

  // rendering
  std::vector<NodeRenderTask> m_nodeRenderTasks;

  // mesh data buffers
  std::vector<Buffer> m_buffers;
};

}
