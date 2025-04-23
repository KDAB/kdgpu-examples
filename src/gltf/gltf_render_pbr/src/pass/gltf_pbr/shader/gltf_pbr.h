/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

namespace pass::gltf_pbr::shader::gltf_pbr {
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
static constexpr size_t fragmentUniformGltfOpaqueTexturesSet = 2;
static constexpr size_t fragmentUniformGltfColorMapBinding = 0;
static constexpr size_t fragmentUniformGltfMetallicRoughnessMapBinding = 1;
static constexpr size_t fragmentUniformGltfNormalMapBinding = 2;
static constexpr size_t fragmentUniformGltfOcclusionMapBinding = 3;
static constexpr size_t fragmentUniformGltfEmissionMapBinding = 5;

// LUT textures
static constexpr size_t fragmentUniformPassLutTexturesSet = 3;
static constexpr size_t fragmentUniformPassLutEnvironmentIrradianceBinding = 0; // first texture in texture set
static constexpr size_t fragmentUniformPassLutEnvironmentSpecularBinding = 1; // second texture in texture set
static constexpr size_t fragmentUniformPassLutPbrBrdfLutBinding = 2; // third texture in texture set

// material configuration
static constexpr size_t fragmentUniformPassConfigurationSet = 4;
static constexpr size_t fragmentUniformPassMaterialBinding = 0;

// output
static constexpr size_t fragmentOutPassColorLocation = 0;

// shader specification for gltf rendering
inline kdgpu_ext::gltf_holder::shader_specification::GltfShaderVertexInput createShaderVertexInput()
{
    return {
        .positionLocation = vertexInGltfVertexPositionLocation,
        .normalLocation = vertexInGltfVertexNormalLocation,
        .textureCoord0Location = vertexInGltfVertexTextureCoordinateLocation,
        .tangentLocation = vertexInGltfVertexTangentLocation
    };
}

inline kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels createShaderMaterial()
{
    using namespace kdgpu_ext::gltf_holder::shader_specification;
    return {
        .shaderBindGroupSets = {
                { vertexUniformPassCameraSet, GltfShaderBindGroupType::PassOwnedUniformBuffer },
                { vertexUniformGltfNodeTransformSet, GltfShaderBindGroupType::GltfNodeTransform },
                { fragmentUniformGltfOpaqueTexturesSet, GltfShaderBindGroupType::GltfShaderMaterialTextureChannels },
                { fragmentUniformPassLutTexturesSet, GltfShaderBindGroupType::PassOwnedUniformBuffer },
                { fragmentUniformPassConfigurationSet, GltfShaderBindGroupType::PassOwnedUniformBuffer }
            //
        },
        .anyTextureChannelsFromGltfEnabled = true,
        .materialTextureChannelDescriptorSet = fragmentUniformGltfOpaqueTexturesSet,
        .baseColorFragmentShaderBinding = fragmentUniformGltfColorMapBinding,
        .normalMapFragmentShaderBinding =  fragmentUniformGltfNormalMapBinding,
        .metallicRoughnessFragmentShaderBinding = fragmentUniformGltfMetallicRoughnessMapBinding,
        .occlusionFragmentShaderBinding = fragmentUniformGltfOcclusionMapBinding,
        // The model in question does not have emission channels, when backend supports non-existent channels, this can be re-enabled
        // .emission_fragment_shader_binding = fragment_uniform_gltf_emission_map_binding
    };
}

} // namespace pass::gltf_pbr::shader::gltf_pbr
