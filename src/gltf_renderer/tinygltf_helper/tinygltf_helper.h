#pragma once

#include <tinygltf_helper/tinygltf_helper_export.h>

#include <KDGpu/gpu_core.h>

#include <tiny_gltf.h>

#include <glm/glm.hpp>

#include <stdint.h>
#include <string>

namespace TinyGltfHelper {

enum class AlphaMode : uint32_t {
    Opaque = 0,
    Mask,
    Blend
};

TINYGLTF_HELPER_EXPORT AlphaMode alphaModeForMaterialAlphaMode(const std::string &mode);

TINYGLTF_HELPER_EXPORT void calculateNodeTreeWorldTransforms(
        const tinygltf::Model &model,
        const int &nodeIndex,
        const glm::dmat4 &parentWorldMatrix,
        std::vector<bool> &visited,
        std::vector<glm::mat4> &worldTransforms);
TINYGLTF_HELPER_EXPORT KDGpu::PrimitiveTopology topologyForPrimitiveMode(int mode);
TINYGLTF_HELPER_EXPORT uint32_t numberOfComponentsForType(int type);
TINYGLTF_HELPER_EXPORT KDGpu::IndexType indexTypeForComponentType(int componentType);
TINYGLTF_HELPER_EXPORT KDGpu::Format formatForAccessor(const tinygltf::Accessor &accessor);
TINYGLTF_HELPER_EXPORT uint32_t sizeForComponentType(int componentType);
TINYGLTF_HELPER_EXPORT uint32_t packedArrayStrideForAccessor(const tinygltf::Accessor &accessor);
TINYGLTF_HELPER_EXPORT KDGpu::FilterMode filterModeForSamplerFilter(int filter);
TINYGLTF_HELPER_EXPORT KDGpu::MipmapFilterMode mipmapFilterModeForSamplerFilter(int filter);
TINYGLTF_HELPER_EXPORT KDGpu::AddressMode addressModeForSamplerAddressMode(int addressMode);
TINYGLTF_HELPER_EXPORT bool loadModel(tinygltf::Model &model, const std::string &filename);

} // namespace TinyGltfHelper
