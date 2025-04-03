#pragma once

#include <cinttypes>

namespace kdgpu_ext::gltf_holder::shader_specification {
enum class GltfShaderBindGroupType : uint8_t
{
    GltfNodeTransform,
    GltfShaderMaterialTextureChannels,
    PassOwnedUniformBuffer
};
}
