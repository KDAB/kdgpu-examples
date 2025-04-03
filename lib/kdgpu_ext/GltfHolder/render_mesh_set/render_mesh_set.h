#pragma once

#include <vector>

#include "mesh_primitives.h"
#include "primitive_data.h"
#include <KDGpu/graphics_pipeline.h>

namespace kdgpu_ext::gltf_holder::render_mesh_set {
struct RenderMeshSet
{
    std::vector<MeshPrimitives> primitives;
    std::vector<PrimitiveData> primitiveData;
    std::vector<KDGpu::GraphicsPipeline> graphicsPipelines;

    void deinitialize()
    {
        primitives.clear();
        primitiveData.clear();
        graphicsPipelines.clear();
    }
};
}
