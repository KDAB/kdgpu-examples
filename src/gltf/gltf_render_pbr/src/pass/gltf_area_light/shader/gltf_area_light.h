/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

namespace pass::gltf_area_light::shader::gltf_area_light {
// VERTEX SHADER

// inputs
static constexpr uint32_t vertexInGltfVertexPositionLocation = 0;
static constexpr uint32_t vertexInGltfVertexNormalLocation = 1;
static constexpr uint32_t vertexInGltfVertexTextureCoordinateLocation = 2;
static constexpr uint32_t vertexInGltfVertexTangentLocation = 3;

// camera uniform buffer object
static constexpr size_t vertexUniformPassCameraSet = 0;
static constexpr size_t vertexUniformPassCameraBinding = 0;

// node transform uniform buffer object
static constexpr size_t vertexUniformGltfNodeTransformSet = 1;
static constexpr size_t vertexUniformGltfNodeTransformBinding = 0;

// FRAGMENT SHADER

// textures from gltf
static constexpr size_t fragmentUniformGltfTexturesSet = 2;
static constexpr size_t fragmentUniformGltfBaseColorMapBinding = 0;
static constexpr size_t fragmentUniformGltfMetallicRoughnessMapBinding = 1;
static constexpr size_t fragmentUniformGltfNormalMapBinding = 2;

// LUT textures
static constexpr size_t fragmentUniformPassLtcTexturesSet = 3;
static constexpr size_t fragmentUniformPassLtcAmplificationBinding = 0; // first texture in texture set
static constexpr size_t fragmentUniformPassLtcMatrixBinding = 1; // second texture in texture set
static constexpr size_t fragmentUniformPassLtcFilteredMapTextureBinding = 2; // third texture in texture set

// configuration
static constexpr size_t fragmentUniformPassConfigurationSet = 4;
static constexpr size_t fragmentUniformPassConfigurationBinding = 0;

// output
static constexpr size_t fragmentOutPassColorLocation = 0;

// shader specification for gltf rendering
inline kdgpu_ext::gltf_holder::shader_specification::GltfShaderVertexInput create_shader_vertex_input()
{
    return {
        .positionLocation = vertexInGltfVertexPositionLocation,
        .normalLocation = vertexInGltfVertexNormalLocation,
        .textureCoord0Location = vertexInGltfVertexTextureCoordinateLocation,
        .tangentLocation = vertexInGltfVertexTangentLocation
    };
}

inline kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels create_shader_material()
{
    using namespace kdgpu_ext::gltf_holder::shader_specification;
    return {
        .shaderBindGroupSets = {
                { vertexUniformPassCameraSet, GltfShaderBindGroupType::PassOwnedUniformBuffer },
                { vertexUniformGltfNodeTransformSet, GltfShaderBindGroupType::GltfNodeTransform },
                { fragmentUniformGltfTexturesSet, GltfShaderBindGroupType::GltfShaderMaterialTextureChannels },
                { fragmentUniformPassLtcTexturesSet, GltfShaderBindGroupType::PassOwnedUniformBuffer },
                { fragmentUniformPassConfigurationSet, GltfShaderBindGroupType::PassOwnedUniformBuffer }
            //
        },
        .anyTextureChannelsFromGltfEnabled = true,
        .materialTextureChannelDescriptorSet = fragmentUniformGltfTexturesSet,
        .baseColorFragmentShaderBinding = fragmentUniformGltfBaseColorMapBinding,
        .normalMapFragmentShaderBinding =  fragmentUniformGltfNormalMapBinding,
        .metallicRoughnessFragmentShaderBinding = fragmentUniformGltfMetallicRoughnessMapBinding,
    };
}

} // namespace pass::gltf_area_light::shader::gltf_area_light
