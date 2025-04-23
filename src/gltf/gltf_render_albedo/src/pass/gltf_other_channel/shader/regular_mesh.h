/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

namespace pass::gltf_other_channel::shader::gltf_other_channel {
// VERTEX SHADER

// inputs
static constexpr uint32_t vertexInGltfVertexPositionLocation = 0;
static constexpr uint32_t vertexInGltfVertexTextureCoordinateLocation = 1;

// camera uniform buffer object
static constexpr size_t vertexUniformPassCameraSet = 2;
static constexpr size_t vertexUniformPassCameraBinding = 0;

// node transform uniform buffer object
static constexpr size_t vertexUniformGltfNodeTransformSet = 0;
static constexpr size_t vertexUniformGltfNodeTransformBinding = 0;

// FRAGMENT SHADER

// other channel textures
static constexpr size_t fragmentUniformPassOtherChannelTextureSet = 1;

// output
static constexpr size_t fragmentOutPassColorLocation = 0;

// shader specification for gltf rendering
inline kdgpu_ext::gltf_holder::shader_specification::GltfShaderVertexInput createShaderVertexInput()
{
    return {
        .positionLocation = vertexInGltfVertexPositionLocation,
        .textureCoord0Location = vertexInGltfVertexTextureCoordinateLocation
    };
}

inline kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels createShaderMaterial()
{
    using namespace kdgpu_ext::gltf_holder::shader_specification;
    return {
        .shaderBindGroupSets = {
                { vertexUniformPassCameraSet, GltfShaderBindGroupType::PassOwnedUniformBuffer },
                { vertexUniformGltfNodeTransformSet, GltfShaderBindGroupType::GltfNodeTransform },
                { fragmentUniformPassOtherChannelTextureSet, GltfShaderBindGroupType::PassOwnedUniformBuffer } },
        .anyTextureChannelsFromGltfEnabled = false,
    };
}

} // namespace pass::gltf_other_channel::shader::gltf_other_channel
