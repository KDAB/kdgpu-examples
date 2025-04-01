/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <tinygltf_helper/tinygltf_helper.h>

#include <KDGpu/gpu_core.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/utils/hash_utils.h>

#include <vector>

// This will be used to act as a key in a map for caching and reusing
// GraphicsPipelines. We do not just reuse the GraphicsPipelineOptions struct
// because that is based upon handles which although fine for runtime use will
// not work well with a persistent cache on disk. Also the shader modules tend
// to be throw away locally scoped objects and so would change every time making
// the key pointless.
//
// So in here we try to capture the values that we alter to make each pipeline
// unique and use that as the basis for our hash key.

using namespace KDGpu;

struct PipelineShaderOptionsKey {
    // For a more complete system we could also use the shader base name here. In our
    // case though it is always the same shader (simplified PBR metallic-roughness).
    bool hasTexCoords{ true };
    bool enableAlphaCutoff{ false };

    bool operator==(const PipelineShaderOptionsKey &other) const noexcept
    {
        return hasTexCoords == other.hasTexCoords && enableAlphaCutoff == other.enableAlphaCutoff;
    }
};

struct ShaderModuleKey {
    PipelineShaderOptionsKey shaderOptionsKey;
    ShaderStageFlagBits stage{ ShaderStageFlagBits::MaxEnum };

    bool operator==(const ShaderModuleKey &other) const noexcept
    {
        return shaderOptionsKey == other.shaderOptionsKey && stage == other.stage;
    }
};

struct GraphicsPipelineKey {
    PrimitiveTopology topology{ PrimitiveTopology::TriangleList };
    VertexOptions vertexOptions;
    TinyGltfHelper::AlphaMode alphaMode{ TinyGltfHelper::AlphaMode::Opaque };
    bool doubleSided{ false };
    PipelineShaderOptionsKey shaderOptionsKey{};

    bool operator==(const GraphicsPipelineKey &other) const noexcept
    {
        if (topology != other.topology)
            return false;

        // TODO: Add equality operators to types in ToyRenderer to simplify this

        // Buffer layouts
        if (vertexOptions.buffers.size() != other.vertexOptions.buffers.size())
            return false;

        const uint32_t bufferCount = static_cast<uint32_t>(vertexOptions.buffers.size());
        for (uint32_t i = 0; i < bufferCount; ++i) {
            const auto &buffer = vertexOptions.buffers.at(i);
            const auto &otherBuffer = other.vertexOptions.buffers.at(i);
            if (buffer.binding != otherBuffer.binding || buffer.stride != otherBuffer.stride ||
                buffer.inputRate != otherBuffer.inputRate)
                return false;
        }

        // Attribute definitions
        if (vertexOptions.attributes.size() != other.vertexOptions.attributes.size())
            return false;

        const uint32_t attributeCount = static_cast<uint32_t>(vertexOptions.attributes.size());
        for (uint32_t i = 0; i < attributeCount; ++i) {
            const auto &attribute = vertexOptions.attributes.at(i);
            const auto &otherAttribute = other.vertexOptions.attributes.at(i);
            if (attribute.location != otherAttribute.location || attribute.binding != otherAttribute.binding ||
                attribute.format != otherAttribute.format || attribute.offset != otherAttribute.offset) {
                return false;
            }
        }

        if (alphaMode != other.alphaMode)
            return false;

        if (doubleSided != other.doubleSided)
            return false;

        if (shaderOptionsKey != other.shaderOptionsKey)
            return false;

        return true;
    }

    bool operator!=(const GraphicsPipelineKey &other) const noexcept { return !(*this == other); }
};

namespace std {

template<>
struct hash<KDGpu::VertexBufferLayout> {
    size_t operator()(const KDGpu::VertexBufferLayout &key) const
    {
        uint64_t hash = 0;
        KDGpu::hash_combine(hash, key.binding);
        KDGpu::hash_combine(hash, key.stride);
        KDGpu::hash_combine(hash, key.inputRate);
        return hash;
    }
};

template<>
struct hash<KDGpu::VertexAttribute> {
    size_t operator()(const KDGpu::VertexAttribute &key) const
    {
        uint64_t hash = 0;
        KDGpu::hash_combine(hash, key.location);
        KDGpu::hash_combine(hash, key.binding);
        KDGpu::hash_combine(hash, key.format);
        KDGpu::hash_combine(hash, key.offset);
        return hash;
    }
};

template<>
struct hash<PipelineShaderOptionsKey> {
    size_t operator()(const PipelineShaderOptionsKey &key) const
    {
        uint64_t hash = 0;
        KDGpu::hash_combine(hash, key.hasTexCoords);
        KDGpu::hash_combine(hash, key.enableAlphaCutoff);
        return hash;
    }
};

template<>
struct hash<ShaderModuleKey> {
    size_t operator()(const ShaderModuleKey &key) const
    {
        uint64_t hash = 0;
        KDGpu::hash_combine(hash, key.shaderOptionsKey);
        KDGpu::hash_combine(hash, key.stage);
        return hash;
    }
};

template<>
struct hash<GraphicsPipelineKey> {
    size_t operator()(const GraphicsPipelineKey &key) const
    {
        uint64_t hash = 0;

        KDGpu::hash_combine(hash, key.topology);

        for (const auto &buffer : key.vertexOptions.buffers)
            KDGpu::hash_combine(hash, buffer);

        for (const auto &attribute : key.vertexOptions.attributes)
            KDGpu::hash_combine(hash, attribute);

        KDGpu::hash_combine(hash, key.alphaMode);
        KDGpu::hash_combine(hash, key.doubleSided);

        KDGpu::hash_combine(hash, key.shaderOptionsKey);

        return hash;
    }
};

} // namespace std
