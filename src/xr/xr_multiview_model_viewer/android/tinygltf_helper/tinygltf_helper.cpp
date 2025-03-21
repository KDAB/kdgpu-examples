/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "tinygltf_helper.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <spdlog/spdlog.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <KDUtils/dir.h>
#include <KDGui/platform/android/android_platform_integration.h>
#include <android_native_app_glue.h>

using namespace KDGpu;

namespace TinyGltfHelper {

AlphaMode alphaModeForMaterialAlphaMode(const std::string &mode)
{
    if (mode == "OPAQUE")
        return AlphaMode::Opaque;
    else if (mode == "MASK")
        return AlphaMode::Mask;
    else if (mode == "BLEND")
        return AlphaMode::Blend;
    else
        return AlphaMode::Opaque;
}

void calculateNodeTreeWorldTransforms(const tinygltf::Model &model,
                                      const int &nodeIndex,
                                      const glm::dmat4 &parentWorldMatrix,
                                      std::vector<bool> &visited,
                                      std::vector<glm::mat4> &worldTransforms)
{
    // If we have already processed this sub-tree, then bail out early.
    if (visited.at(nodeIndex) == true)
        return;

    // Evaluate the model transform for the current node. This can either be stored directly
    // as a 4x4 matrix or using properties for scale, rotation, translation.
    const auto &node = model.nodes.at(nodeIndex);
    glm::dmat4 nodeModelMatrix(1.0);
    if (node.matrix.size() == 16) {
        std::memcpy(glm::value_ptr(nodeModelMatrix), node.matrix.data(), 16 * sizeof(double));
    } else {
        // Build a model matrix from the SRT representation
        glm::dvec3 scale(1.0);
        if (node.scale.size() == 3)
            std::memcpy(glm::value_ptr(scale), node.scale.data(), 3 * sizeof(double));
        glm::dquat rotation(1.0, 0.0, 0.0, 0.0);
        if (node.rotation.size() == 4)
            std::memcpy(glm::value_ptr(rotation), node.rotation.data(), 4 * sizeof(double));
        glm::dvec3 translation(0.0);
        if (node.translation.size() == 3)
            std::memcpy(glm::value_ptr(translation), node.translation.data(), 3 * sizeof(double));

        glm::dmat4 mTrans = glm::translate(glm::dmat4(1.0f), translation);
        glm::dmat4 mRot = glm::toMat4(rotation);
        glm::dmat4 mScale = glm::scale(glm::dmat4(1.0f), scale);
        nodeModelMatrix = mTrans * mRot * mScale;
    }

    // Combine with the parent transform
    glm::dmat4 nodeWorldMatrix = parentWorldMatrix * nodeModelMatrix;

    // Convert to float and store the result
    auto &world = worldTransforms.at(nodeIndex);
    for (uint32_t col = 0; col < 4; ++col) {
        const auto &srcCol = nodeWorldMatrix[col];
        auto &dstCol = world[col];
        for (uint32_t row = 0; row < 4; ++row) {
            dstCol[row] = static_cast<float>(srcCol[row]);
        }
    }

    // Recurse to child nodes
    for (const auto &childIndex : node.children) {
        calculateNodeTreeWorldTransforms(model, childIndex, nodeWorldMatrix, visited, worldTransforms);
    }

    // Record we are done with this sub-tree
    visited[nodeIndex] = true;
}

PrimitiveTopology topologyForPrimitiveMode(int mode)
{
    switch (mode) {
    case TINYGLTF_MODE_POINTS:
        return PrimitiveTopology::PointList;
    case TINYGLTF_MODE_LINE:
        return PrimitiveTopology::LineList;
    case TINYGLTF_MODE_LINE_LOOP:
        return PrimitiveTopology::LineList; // TODO: How to do this?
    case TINYGLTF_MODE_LINE_STRIP:
        return PrimitiveTopology::LineStrip;
    case TINYGLTF_MODE_TRIANGLES:
        return PrimitiveTopology::TriangleList;
    case TINYGLTF_MODE_TRIANGLE_STRIP:
        return PrimitiveTopology::TriangleStrip;
    case TINYGLTF_MODE_TRIANGLE_FAN:
        return PrimitiveTopology::TriangleFan;
    }
    assert(false);
    return PrimitiveTopology::PointList;
}

uint32_t numberOfComponentsForType(int type)
{
    switch (type) {
    case TINYGLTF_TYPE_SCALAR:
        return 1;
    case TINYGLTF_TYPE_VEC2:
        return 2;
    case TINYGLTF_TYPE_VEC3:
        return 3;
    case TINYGLTF_TYPE_VEC4:
        return 4;
    default:
        return 0;
    }
}

IndexType indexTypeForComponentType(int componentType)
{
    switch (componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return IndexType::Uint16;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        return IndexType::Uint32;
    default: {
        assert(false);
        return IndexType::Uint32;
    }
    }
}

Format formatForAccessor(const tinygltf::Accessor &accessor)
{
    const bool norm = accessor.normalized;
    const uint32_t count = numberOfComponentsForType(accessor.type);
    // clang-format off
    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
            switch (count) {
                case 1: return norm ? Format::R8_SNORM : Format::R8_SINT;
                case 2: return norm ? Format::R8G8_SNORM : Format::R8G8_SINT;
                case 3: return norm ? Format::R8G8B8_SNORM : Format::R8G8B8_SINT;
                case 4: return norm ? Format::R8G8B8A8_SNORM : Format::R8G8B8A8_SINT;
            }
            break;
        }

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            switch (count) {
                case 1: return norm ? Format::R8_UNORM : Format::R8_UINT;
                case 2: return norm ? Format::R8G8_UNORM : Format::R8G8_UINT;
                case 3: return norm ? Format::R8G8B8_UNORM : Format::R8G8B8_UINT;
                case 4: return norm ? Format::R8G8B8A8_UNORM : Format::R8G8B8A8_UINT;
            }
            break;
        }

        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            switch (count) {
                case 1: return norm ? Format::R16_SNORM : Format::R16_SINT;
                case 2: return norm ? Format::R16G16_SNORM : Format::R16G16_SINT;
                case 3: return norm ? Format::R16G16B16_SNORM : Format::R16G16B16_SINT;
                case 4: return norm ? Format::R16G16B16A16_SNORM : Format::R16G16B16A16_SINT;
            }
            break;
        }

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            switch (count) {
                case 1: return norm ? Format::R16_UNORM : Format::R16_UINT;
                case 2: return norm ? Format::R16G16_UNORM : Format::R16G16_UINT;
                case 3: return norm ? Format::R16G16B16_UNORM : Format::R16G16B16_UINT;
                case 4: return norm ? Format::R16G16B16A16_UNORM : Format::R16G16B16A16_UINT;
            }
            break;
        }

        case TINYGLTF_COMPONENT_TYPE_INT: {
            switch (count) {
                case 1: return Format::R32_SINT;
                case 2: return Format::R32G32_SINT;
                case 3: return Format::R32G32B32_SINT;
                case 4: return Format::R32G32B32A32_SINT;
            }
            break;
        }

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            switch (count) {
                case 1: return Format::R32_UINT;
                case 2: return Format::R32G32_UINT;
                case 3: return Format::R32G32B32_UINT;
                case 4: return Format::R32G32B32A32_UINT;
            }
            break;
        }

        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
            switch (count) {
                case 1: return Format::R32_SFLOAT;
                case 2: return Format::R32G32_SFLOAT;
                case 3: return Format::R32G32B32_SFLOAT;
                case 4: return Format::R32G32B32A32_SFLOAT;
            }
            break;
        }
    }
    // clang-format on

    return Format::UNDEFINED;
}

uint32_t sizeForComponentType(int componentType)
{
    switch (componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        return 1;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return 1;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        return 2;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return 2;
    case TINYGLTF_COMPONENT_TYPE_INT:
        return 4;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        return 4;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return 4;
    default:
        return 0;
    }
}

uint32_t packedArrayStrideForAccessor(const tinygltf::Accessor &accessor)
{
    return sizeForComponentType(accessor.componentType) * numberOfComponentsForType(accessor.type);
}

KDGpu::FilterMode filterModeForSamplerFilter(int filter)
{
    switch (filter) {
    case TINYGLTF_TEXTURE_FILTER_NEAREST:
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        return KDGpu::FilterMode::Nearest;

    case TINYGLTF_TEXTURE_FILTER_LINEAR:
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
        return KDGpu::FilterMode::Linear;

    default:
        return KDGpu::FilterMode::Nearest;
    }
}

KDGpu::MipmapFilterMode mipmapFilterModeForSamplerFilter(int filter)
{
    switch (filter) {
    case TINYGLTF_TEXTURE_FILTER_NEAREST:
    case TINYGLTF_TEXTURE_FILTER_LINEAR:
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
        return KDGpu::MipmapFilterMode::Nearest;

    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
        return KDGpu::MipmapFilterMode::Linear;

    default:
        return KDGpu::MipmapFilterMode::Nearest;
    }
}

KDGpu::AddressMode addressModeForSamplerAddressMode(int addressMode)
{
    switch (addressMode) {
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
        return KDGpu::AddressMode::Repeat;
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
        return KDGpu::AddressMode::ClampToEdge;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
        return KDGpu::AddressMode::MirroredRepeat;

    default:
        return KDGpu::AddressMode::Repeat;
    }
}

bool loadModel(tinygltf::Model &model, const std::string &filename)
{
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

#ifdef ANDROID
    auto dir = KDUtils::Dir{ KDGui::AndroidPlatformIntegration::s_androidApp->activity->externalDataPath };
    auto androidFilename = dir.absoluteFilePath(filename);
    bool result = loader.LoadASCIIFromFile(&model, &err, &warn, androidFilename.data());
#else
    bool result = loader.LoadASCIIFromFile(&model, &err, &warn, filename.data());
#endif



    if (!warn.empty())
        spdlog::warn("{}", warn);

    if (!err.empty())
        spdlog::error("{}", err);

    if (!result)
        spdlog::warn("Failed to load glTF: {}", filename);
    else
        spdlog::warn("Loaded glTF: {}", filename);

    return result;
}

} // namespace TinyGltfHelper
