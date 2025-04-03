#pragma once

#include <vector>
#include <cstdint>

namespace kdgpu_ext::gltf_holder::render_mesh_set {
struct MeshPrimitives {
    std::vector<uint32_t> primitiveIndices;
};
}
