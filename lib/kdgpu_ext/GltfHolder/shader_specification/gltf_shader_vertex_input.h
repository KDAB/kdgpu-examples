#pragma once

#include <optional>

namespace kdgpu_ext::gltf_holder::shader_specification {
/**
 * Mapping of vertex shader inputs with the GLTF render permutations
 */
struct GltfShaderVertexInput
{
    // Vertex shader inputs, if not set, they are not attempted to be initialized.
    // Corresponds to "mesh primitive attributes" in GLTF schema.
    std::optional<uint32_t> positionLocation;
    std::optional<uint32_t> normalLocation;
    std::optional<uint32_t> textureCoord0Location;
    std::optional<uint32_t> tangentLocation;
};
}
