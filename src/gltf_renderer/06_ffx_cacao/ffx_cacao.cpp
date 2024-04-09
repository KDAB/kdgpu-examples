/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "ffx_cacao.h"

#include <KDGpuExample/kdgpuexample.h>

#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/command_buffer.h>
#include <KDGpu/command_recorder.h>
#include <KDGpu/compute_pass_command_recorder.h>
#include <KDGpu/compute_pipeline_options.h>
#include <KDGpu/pipeline_layout_options.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/sampler_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/texture_view_options.h>

// Precompiled shader bytecode
#include "CACAOClearLoadCounter_32.h"
#include "CACAOPrepareDownsampledDepths_32.h"
#include "CACAOPrepareNativeDepths_32.h"
#include "CACAOPrepareDownsampledDepthsAndMips_32.h"
#include "CACAOPrepareNativeDepthsAndMips_32.h"
#include "CACAOPrepareDownsampledNormals_32.h"
#include "CACAOPrepareNativeNormals_32.h"
#include "CACAOPrepareDownsampledNormalsFromInputNormals_32.h"
#include "CACAOPrepareNativeNormalsFromInputNormals_32.h"
#include "CACAOPrepareDownsampledDepthsHalf_32.h"
#include "CACAOPrepareNativeDepthsHalf_32.h"
#include "CACAOGenerateQ0_32.h"
#include "CACAOGenerateQ1_32.h"
#include "CACAOGenerateQ2_32.h"
#include "CACAOGenerateQ3_32.h"
#include "CACAOGenerateQ3Base_32.h"
#include "CACAOGenerateImportanceMap_32.h"
#include "CACAOPostprocessImportanceMapA_32.h"
#include "CACAOPostprocessImportanceMapB_32.h"
#include "CACAOEdgeSensitiveBlur1_32.h"
#include "CACAOEdgeSensitiveBlur2_32.h"
#include "CACAOEdgeSensitiveBlur3_32.h"
#include "CACAOEdgeSensitiveBlur4_32.h"
#include "CACAOEdgeSensitiveBlur5_32.h"
#include "CACAOEdgeSensitiveBlur6_32.h"
#include "CACAOEdgeSensitiveBlur7_32.h"
#include "CACAOEdgeSensitiveBlur8_32.h"
#include "CACAOApply_32.h"
#include "CACAONonSmartApply_32.h"
#include "CACAONonSmartHalfApply_32.h"
#include "CACAOUpscaleBilateral5x5Smart_32.h"
#include "CACAOUpscaleBilateral5x5NonSmart_32.h"
#include "CACAOUpscaleBilateral5x5Half_32.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <string>
#include <cassert>

using namespace KDGpu;
using namespace std::string_literals;

namespace {

constexpr size_t SamplersCount = 5;
constexpr size_t InputBindGroupBindingsCount = 45;
constexpr size_t OutputBindGroupBindingsCount = 27;
constexpr size_t PassCount = 4;
constexpr size_t UBOCount = 4;

struct TextureInfo {
    uint32_t FFX_Cacao::BufferSizeInfo::*width;
    uint32_t FFX_Cacao::BufferSizeInfo::*height;
    Format format;
    uint32_t arraySize;
    uint32_t mipLevels;
};

enum class TextureCACAO : uint32_t {
    DeinterleavedDepth = 0,
    DeinterleavedNormals,
    SSAOBufferPing,
    SSAOBufferPong,
    ImportanceMap,
    ImportanceMapPong,
    DownsampledSSABOBuffer,
    Max
};

const std::array<TextureInfo, static_cast<uint32_t>(TextureCACAO::Max)> texturesInfo = {
    // { width, height, format, mip levels, array size }
    TextureInfo{ &FFX_Cacao::BufferSizeInfo::deinterleavedDepthBufferWidth, &FFX_Cacao::BufferSizeInfo::deinterleavedDepthBufferHeight, Format::R16_SFLOAT, 4, 4 }, // DeinterleavedDepths
    TextureInfo{ &FFX_Cacao::BufferSizeInfo::ssaoBufferWidth, &FFX_Cacao::BufferSizeInfo::ssaoBufferHeight, Format::R8G8B8A8_SNORM, 4, 1 }, // DeinterleavedNormals
    TextureInfo{ &FFX_Cacao::BufferSizeInfo::ssaoBufferWidth, &FFX_Cacao::BufferSizeInfo::ssaoBufferHeight, Format::R8G8_UNORM, 4, 1 }, // SSAOBufferPing
    TextureInfo{ &FFX_Cacao::BufferSizeInfo::ssaoBufferWidth, &FFX_Cacao::BufferSizeInfo::ssaoBufferHeight, Format::R8G8_UNORM, 4, 1 }, // SSAOBufferPong
    TextureInfo{ &FFX_Cacao::BufferSizeInfo::importanceMapWidth, &FFX_Cacao::BufferSizeInfo::importanceMapHeight, Format::R8_UNORM, 1, 1 }, // ImportanceMap
    TextureInfo{ &FFX_Cacao::BufferSizeInfo::importanceMapWidth, &FFX_Cacao::BufferSizeInfo::importanceMapHeight, Format::R8_UNORM, 1, 1 }, // ImportanceMapPong
    TextureInfo{ &FFX_Cacao::BufferSizeInfo::downsampledSsaoBufferWidth, &FFX_Cacao::BufferSizeInfo::downsampledSsaoBufferHeight, Format::R8_UNORM, 1, 1 }, // DownsampledSSAOBuffer
};

struct TextureViewInfo {
    TextureCACAO texture;
    ViewType viewType;
    uint32_t mostDetailedMip;
    uint32_t mipLevels;
    uint32_t firstArraySlice;
    uint32_t arraySize;
};

enum class TextureViewsCACAO : uint32_t {
    DeinterleavedDepth = 0,
    DeinterleavedDepthArray0,
    DeinterleavedDepthArray1,
    DeinterleavedDepthArray2,
    DeinterleavedDepthArray3,
    DeinterleavedDepthMip0,
    DeinterleavedDepthMip1,
    DeinterleavedDepthMip2,
    DeinterleavedDepthMip3,
    DeinterleavedNormals,
    ImportanceMap,
    ImportanceMapPong,
    SSAOBufferPing,
    SSAOBufferPingArray0,
    SSAOBufferPingArray1,
    SSAOBufferPingArray2,
    SSAOBufferPingArray3,
    SSAOBufferPong,
    SSAOBufferPongArray0,
    SSAOBufferPongArray1,
    SSAOBufferPongArray2,
    SSAOBufferPongArray3,
    Max
};

std::array<TextureViewInfo, static_cast<uint32_t>(TextureViewsCACAO::Max)> textureViewsInfo = {
    TextureViewInfo{ TextureCACAO::DeinterleavedDepth, ViewType::ViewType2DArray, 0, 4, 0, 4 }, // DeinterleavedDepths
    TextureViewInfo{ TextureCACAO::DeinterleavedDepth, ViewType::ViewType2DArray, 0, 4, 0, 1 }, // DeinterleavedDepthsArray0
    TextureViewInfo{ TextureCACAO::DeinterleavedDepth, ViewType::ViewType2DArray, 0, 4, 1, 1 }, // DeinterleavedDepthsArray1
    TextureViewInfo{ TextureCACAO::DeinterleavedDepth, ViewType::ViewType2DArray, 0, 4, 2, 1 }, // DeinterleavedDepthsArray2
    TextureViewInfo{ TextureCACAO::DeinterleavedDepth, ViewType::ViewType2DArray, 0, 4, 3, 1 }, // DeinterleavedDepthsArray3
    TextureViewInfo{ TextureCACAO::DeinterleavedDepth, ViewType::ViewType2DArray, 0, 1, 0, 4 }, // DeinterleavedDepthsMip0
    TextureViewInfo{ TextureCACAO::DeinterleavedDepth, ViewType::ViewType2DArray, 1, 1, 0, 4 }, // DeinterleavedDepthsMip1
    TextureViewInfo{ TextureCACAO::DeinterleavedDepth, ViewType::ViewType2DArray, 2, 1, 0, 4 }, // DeinterleavedDepthsMip2
    TextureViewInfo{ TextureCACAO::DeinterleavedDepth, ViewType::ViewType2DArray, 3, 1, 0, 4 }, // DeinterleavedDepthsMip3
    TextureViewInfo{ TextureCACAO::DeinterleavedNormals, ViewType::ViewType2DArray, 0, 1, 0, 4 }, // DeinterleavedNormals
    TextureViewInfo{ TextureCACAO::ImportanceMap, ViewType::ViewType2D, 0, 1, 0, 1 }, // ImportanceMap
    TextureViewInfo{ TextureCACAO::ImportanceMapPong, ViewType::ViewType2D, 0, 1, 0, 1 }, // ImportanceMapPong
    TextureViewInfo{ TextureCACAO::SSAOBufferPing, ViewType::ViewType2DArray, 0, 1, 0, 4 }, // SSABBufferPing
    TextureViewInfo{ TextureCACAO::SSAOBufferPing, ViewType::ViewType2DArray, 0, 1, 0, 1 }, // SSABBufferPingArray0
    TextureViewInfo{ TextureCACAO::SSAOBufferPing, ViewType::ViewType2DArray, 0, 1, 1, 1 }, // SSABBufferPingArray1
    TextureViewInfo{ TextureCACAO::SSAOBufferPing, ViewType::ViewType2DArray, 0, 1, 2, 1 }, // SSABBufferPingArray2
    TextureViewInfo{ TextureCACAO::SSAOBufferPing, ViewType::ViewType2DArray, 0, 1, 3, 1 }, // SSABBufferPingArray3
    TextureViewInfo{ TextureCACAO::SSAOBufferPong, ViewType::ViewType2DArray, 0, 1, 0, 4 }, // SSABBufferPong
    TextureViewInfo{ TextureCACAO::SSAOBufferPong, ViewType::ViewType2DArray, 0, 1, 0, 1 }, // SSABBufferPongArray0
    TextureViewInfo{ TextureCACAO::SSAOBufferPong, ViewType::ViewType2DArray, 0, 1, 1, 1 }, // SSABBufferPongArray1
    TextureViewInfo{ TextureCACAO::SSAOBufferPong, ViewType::ViewType2DArray, 0, 1, 2, 1 }, // SSABBufferPongArray2
    TextureViewInfo{ TextureCACAO::SSAOBufferPong, ViewType::ViewType2DArray, 0, 1, 3, 1 }, // SSABBufferPongArray3
};

struct BindGroupVariablesCount {
    size_t samplersCount;
    size_t constantsCount;
    size_t inputsCount;
    size_t outputsCount;
};

enum class BindGroupLayoutCACAO : uint32_t {
    ClearLoadCounter = 0,
    PrepareDepth,
    PrepareDepthMips,
    PreparePoints,
    PreparePointsMips,
    PrepareNormals,
    PrepareNormalsFromInputNormals,
    Generate,
    GenerateAdaptive,
    GenerateImportanceMap,
    PostProcessImportanceMapA,
    PostProcessImportanceMapB,
    EdgeSensitiveBlur,
    Apply,
    BilateralUpSample,
    Max
};

// BindGroupLayouts
const std::array<BindGroupVariablesCount, static_cast<uint32_t>(BindGroupLayoutCACAO::Max)> bindGroupVariablesCount = {
    // { sampler bindings, input (sampled images) bindings, output (storage images) bindings, constant (ubo) bindings}
    BindGroupVariablesCount{ SamplersCount, 1, 0, 1 }, // ClearLoadCounter
    BindGroupVariablesCount{ SamplersCount, 1, 1, 1 }, // PrepareDepth
    BindGroupVariablesCount{ SamplersCount, 1, 1, 4 }, // PrepareDepthMips
    BindGroupVariablesCount{ SamplersCount, 1, 1, 1 }, // PreparePoints
    BindGroupVariablesCount{ SamplersCount, 1, 1, 4 }, // PreparePointsMips
    BindGroupVariablesCount{ SamplersCount, 1, 1, 1 }, // PrepareNormals
    BindGroupVariablesCount{ SamplersCount, 1, 1, 1 }, // PrepareNormalsFromInputNormals
    BindGroupVariablesCount{ SamplersCount, 1, 2, 1 }, // Generate
    BindGroupVariablesCount{ SamplersCount, 1, 5, 1 }, // GenerateAdaptive
    BindGroupVariablesCount{ SamplersCount, 1, 1, 1 }, // GenerateImportanceMap
    BindGroupVariablesCount{ SamplersCount, 1, 1, 1 }, // PostProcessImportanceMapA
    BindGroupVariablesCount{ SamplersCount, 1, 1, 2 }, // PostProcessImportanceMapB
    BindGroupVariablesCount{ SamplersCount, 1, 1, 1 }, // EdgeSensitiveBlur
    BindGroupVariablesCount{ SamplersCount, 1, 1, 1 }, // Apply
    BindGroupVariablesCount{ SamplersCount, 1, 4, 1 }, // BilateralUpSample
};

struct BindGroupDescription {
    BindGroupLayoutCACAO bindGroupLayout;
    size_t passIdx;
};

enum class BindGroupCACAO : uint32_t {
    ClearLoadCounter = 0,
    PrepareDepth,
    PrepareDepthMips,
    PreparePoints,
    PreparePointsMips,
    PrepareNormals,
    PrepareNormalsFromInputNormals,
    GenerateAdaptiveBase0,
    GenerateAdaptiveBase1,
    GenerateAdaptiveBase2,
    GenerateAdaptiveBase3,
    Generate0,
    Generate1,
    Generate2,
    Generate3,
    GenerateAdaptive0,
    GenerateAdaptive1,
    GenerateAdaptive2,
    GenerateAdaptive3,
    GenerateImportanceMap,
    GenerateImportanceMapA,
    GenerateImportanceMapB,
    EdgeSensitiveBlur0,
    EdgeSensitiveBlur1,
    EdgeSensitiveBlur2,
    EdgeSensitiveBlur3,
    ApplyPing,
    ApplyPong,
    BilateralUpsamplePing,
    BilateralUpsamplePong,
    Max
};

// BindGroups
const std::array<BindGroupDescription, static_cast<uint32_t>(BindGroupCACAO::Max)> bindGroupDescriptions = {
    // { bindGroupLayoutIdx, passIdx }
    BindGroupDescription{ BindGroupLayoutCACAO::ClearLoadCounter, 0 }, // ClearLoadCounter
    BindGroupDescription{ BindGroupLayoutCACAO::PrepareDepth, 0 }, // PrepareDepth
    BindGroupDescription{ BindGroupLayoutCACAO::PrepareDepthMips, 0 }, // PrepareDepthMips
    BindGroupDescription{ BindGroupLayoutCACAO::PreparePoints, 0 }, // PreparePoints
    BindGroupDescription{ BindGroupLayoutCACAO::PreparePointsMips, 0 }, // PreparePointsMips
    BindGroupDescription{ BindGroupLayoutCACAO::PrepareNormals, 0 }, // PrepareNormals
    BindGroupDescription{ BindGroupLayoutCACAO::PrepareNormalsFromInputNormals, 0 }, // PrepareNormalsFromInputNormals
    BindGroupDescription{ BindGroupLayoutCACAO::Generate, 0 }, // GenerateAdaptiveBase0
    BindGroupDescription{ BindGroupLayoutCACAO::Generate, 1 }, // GenerateAdaptiveBase1
    BindGroupDescription{ BindGroupLayoutCACAO::Generate, 2 }, // GenerateAdaptiveBase2
    BindGroupDescription{ BindGroupLayoutCACAO::Generate, 3 }, // GenerateAdaptiveBase3
    BindGroupDescription{ BindGroupLayoutCACAO::Generate, 0 }, // Generate0
    BindGroupDescription{ BindGroupLayoutCACAO::Generate, 1 }, // Generate1
    BindGroupDescription{ BindGroupLayoutCACAO::Generate, 2 }, // Generate2
    BindGroupDescription{ BindGroupLayoutCACAO::Generate, 3 }, // Generate3
    BindGroupDescription{ BindGroupLayoutCACAO::GenerateAdaptive, 0 }, // GenerateAdaptive0
    BindGroupDescription{ BindGroupLayoutCACAO::GenerateAdaptive, 1 }, // GenerateAdaptive1
    BindGroupDescription{ BindGroupLayoutCACAO::GenerateAdaptive, 2 }, // GenerateAdaptive2
    BindGroupDescription{ BindGroupLayoutCACAO::GenerateAdaptive, 3 }, // GenerateAdaptive3
    BindGroupDescription{ BindGroupLayoutCACAO::GenerateImportanceMap, 0 }, // GenerateImportanceMap
    BindGroupDescription{ BindGroupLayoutCACAO::PostProcessImportanceMapA, 0 }, // GenerateImportanceMapA
    BindGroupDescription{ BindGroupLayoutCACAO::PostProcessImportanceMapB, 0 }, // GenerateImportanceMapB
    BindGroupDescription{ BindGroupLayoutCACAO::EdgeSensitiveBlur, 0 }, // EdgeSensitiveBlur0
    BindGroupDescription{ BindGroupLayoutCACAO::EdgeSensitiveBlur, 1 }, // EdgeSensitiveBlur1
    BindGroupDescription{ BindGroupLayoutCACAO::EdgeSensitiveBlur, 2 }, // EdgeSensitiveBlur2
    BindGroupDescription{ BindGroupLayoutCACAO::EdgeSensitiveBlur, 3 }, // EdgeSensitiveBlur3
    BindGroupDescription{ BindGroupLayoutCACAO::Apply, 0 }, // ApplyPing
    BindGroupDescription{ BindGroupLayoutCACAO::Apply, 0 }, // ApplyPong
    BindGroupDescription{ BindGroupLayoutCACAO::BilateralUpSample, 0 }, // BilateralUpsamplePing
    BindGroupDescription{ BindGroupLayoutCACAO::BilateralUpSample, 0 }, // BilateralUpsamplePong
};

struct ShaderInfo {
    std::string name;
    std::string entrypoint;
    BindGroupLayoutCACAO pipelineLayout; // We have 1 PipelineLayout per BindGroupLayout
    std::vector<uint32_t> byteCode;
};

enum class PipelineCACAO : uint32_t {
    ClearLoadCounterSPIRV32 = 0,
    PrepareDownsampledDepthsSPIRV32,
    PrepareNativeDepthsSPIRV32,
    PrepareDownsampledDepthsAndMipsSPIRV32,
    PrepareNativeDepthsAndMipsSPIRV32,
    PrepareDownsampledNormalsSPIRV32,
    PrepareNativeNormalsSPIRV32,
    PrepareDownsampledNormalsFromInputNormalsSPIRV32,
    PrepareNativeNormalsFromInputNormalsSPIRV32,
    PrepareDownsampledDepthsHalfSPIRV32,
    PrepareNativeDepthsHalfSPIRV32,
    GenerateQ0SPIRV32,
    GenerateQ1SPIRV32,
    GenerateQ2SPIRV32,
    GenerateQ3SPIRV32,
    GenerateQ3BaseSPIRV32,
    GenerateImportanceMapSPIRV32,
    PostprocessImportanceMapASPIRV32,
    PostprocessImportanceMapBSPIRV32,
    EdgeSensitiveBlur1SPIRV32,
    EdgeSensitiveBlur2SPIRV32,
    EdgeSensitiveBlur3SPIRV32,
    EdgeSensitiveBlur4SPIRV32,
    EdgeSensitiveBlur5SPIRV32,
    EdgeSensitiveBlur6SPIRV32,
    EdgeSensitiveBlur7SPIRV32,
    EdgeSensitiveBlur8SPIRV32,
    ApplySPIRV32,
    NonSmartApplySPIRV32,
    NonSmartHalfApplySPIRV32,
    UpscaleBilateral5x5SmartSPIRV32,
    UpscaleBilateral5x5NonSmartSPIRV32,
    UpscaleBilateral5x5HalfSPIRV32,
    Max
};

std::vector<uint32_t> shaderByteCode(const unsigned char *rawData, size_t byteSize)
{
    std::vector<uint32_t> tmpVecData;
    tmpVecData.resize(byteSize / 4);
    std::memcpy(tmpVecData.data(), rawData, byteSize);
    return tmpVecData;
}

// Note: building the spirv from the textual spirv code doesn't validate. Using the raw char arrays does work though

// ShaderModules / Pipelines
const std::array<ShaderInfo, static_cast<uint32_t>(PipelineCACAO::Max)> shadersInfo = {
    // { shader file, entry point, pipelineLayoutIndex }
    ShaderInfo{
            "CACAOClearLoadCounter_32"s,
            "FFX_CACAO_ClearLoadCounter"s,
            BindGroupLayoutCACAO::ClearLoadCounter,
            shaderByteCode(CSClearLoadCounterSPIRV32, sizeof(CSClearLoadCounterSPIRV32) / sizeof(*CSClearLoadCounterSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareDownsampledDepths_32"s,
            "FFX_CACAO_PrepareDownsampledDepths"s,
            BindGroupLayoutCACAO::PrepareDepth,
            shaderByteCode(CSPrepareDownsampledDepthsSPIRV32, sizeof(CSPrepareDownsampledDepthsSPIRV32) / sizeof(*CSPrepareDownsampledDepthsSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareNativeDepths_32"s,
            "FFX_CACAO_PrepareNativeDepths"s,
            BindGroupLayoutCACAO::PrepareDepth,
            shaderByteCode(CSPrepareNativeDepthsSPIRV32, sizeof(CSPrepareNativeDepthsSPIRV32) / sizeof(*CSPrepareNativeDepthsSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareDownsampledDepthsAndMips_32"s,
            "FFX_CACAO_PrepareDownsampledDepthsAndMips"s,
            BindGroupLayoutCACAO::PrepareDepthMips,
            shaderByteCode(CSPrepareDownsampledDepthsAndMipsSPIRV32, sizeof(CSPrepareDownsampledDepthsAndMipsSPIRV32) / sizeof(*CSPrepareDownsampledDepthsAndMipsSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareNativeDepthsAndMips_32"s,
            "FFX_CACAO_PrepareNativeDepthsAndMips"s,
            BindGroupLayoutCACAO::PrepareDepthMips,
            shaderByteCode(CSPrepareNativeDepthsAndMipsSPIRV32, sizeof(CSPrepareNativeDepthsAndMipsSPIRV32) / sizeof(*CSPrepareNativeDepthsAndMipsSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareDownsampledNormals_32"s,
            "FFX_CACAO_PrepareDownsampledNormals"s,
            BindGroupLayoutCACAO::PrepareNormals,
            shaderByteCode(CSPrepareDownsampledNormalsSPIRV32, sizeof(CSPrepareDownsampledNormalsSPIRV32) / sizeof(*CSPrepareDownsampledNormalsSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareNativeNormals_32"s,
            "FFX_CACAO_PrepareNativeNormals"s,
            BindGroupLayoutCACAO::PrepareNormals,
            shaderByteCode(CSPrepareNativeNormalsSPIRV32, sizeof(CSPrepareNativeNormalsSPIRV32) / sizeof(*CSPrepareNativeNormalsSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareDownsampledNormalsFromInputNormals_32"s,
            "FFX_CACAO_PrepareDownsampledNormalsFromInputNormals"s,
            BindGroupLayoutCACAO::PrepareNormalsFromInputNormals,
            shaderByteCode(CSPrepareDownsampledNormalsFromInputNormalsSPIRV32, sizeof(CSPrepareDownsampledNormalsFromInputNormalsSPIRV32) / sizeof(*CSPrepareDownsampledNormalsFromInputNormalsSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareNativeNormalsFromInputNormals_32"s,
            "FFX_CACAO_PrepareNativeNormalsFromInputNormals"s,
            BindGroupLayoutCACAO::PrepareNormalsFromInputNormals,
            shaderByteCode(CSPrepareNativeNormalsFromInputNormalsSPIRV32, sizeof(CSPrepareNativeNormalsFromInputNormalsSPIRV32) / sizeof(*CSPrepareNativeNormalsFromInputNormalsSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareDownsampledDepthsHalf_32"s,
            "FFX_CACAO_PrepareDownsampledDepthsHalf"s,
            BindGroupLayoutCACAO::PrepareDepth,
            shaderByteCode(CSPrepareDownsampledDepthsHalfSPIRV32, sizeof(CSPrepareDownsampledDepthsHalfSPIRV32) / sizeof(*CSPrepareDownsampledDepthsHalfSPIRV32)),
    },
    ShaderInfo{
            "CACAOPrepareNativeDepthsHalf_32"s,
            "FFX_CACAO_PrepareNativeDepthsHalf"s,
            BindGroupLayoutCACAO::PrepareDepth,
            shaderByteCode(CSPrepareNativeDepthsHalfSPIRV32, sizeof(CSPrepareNativeDepthsHalfSPIRV32) / sizeof(*CSPrepareNativeDepthsHalfSPIRV32)),
    },
    ShaderInfo{
            "CACAOGenerateQ0_32"s,
            "FFX_CACAO_GenerateQ0"s,
            BindGroupLayoutCACAO::Generate,
            shaderByteCode(CSGenerateQ0SPIRV32, sizeof(CSGenerateQ0SPIRV32) / sizeof(*CSGenerateQ0SPIRV32)),
    },
    ShaderInfo{
            "CACAOGenerateQ1_32"s,
            "FFX_CACAO_GenerateQ1"s,
            BindGroupLayoutCACAO::Generate,
            shaderByteCode(CSGenerateQ1SPIRV32, sizeof(CSGenerateQ1SPIRV32) / sizeof(*CSGenerateQ1SPIRV32)),
    },
    ShaderInfo{
            "CACAOGenerateQ2_32"s,
            "FFX_CACAO_GenerateQ2"s,
            BindGroupLayoutCACAO::Generate,
            shaderByteCode(CSGenerateQ2SPIRV32, sizeof(CSGenerateQ2SPIRV32) / sizeof(*CSGenerateQ2SPIRV32)),
    },
    ShaderInfo{
            "CACAOGenerateQ3_32"s,
            "FFX_CACAO_GenerateQ3"s,
            BindGroupLayoutCACAO::GenerateAdaptive,
            shaderByteCode(CSGenerateQ3SPIRV32, sizeof(CSGenerateQ3SPIRV32) / sizeof(*CSGenerateQ3SPIRV32)),
    },
    ShaderInfo{
            "CACAOGenerateQ3Base_32"s,
            "FFX_CACAO_GenerateQ3Base"s,
            BindGroupLayoutCACAO::Generate,
            shaderByteCode(CSGenerateQ3BaseSPIRV32, sizeof(CSGenerateQ3BaseSPIRV32) / sizeof(*CSGenerateQ3BaseSPIRV32)),
    },
    ShaderInfo{
            "CACAOGenerateImportanceMap_32"s,
            "FFX_CACAO_GenerateImportanceMap"s,
            BindGroupLayoutCACAO::GenerateImportanceMap,
            shaderByteCode(CSGenerateImportanceMapSPIRV32, sizeof(CSGenerateImportanceMapSPIRV32) / sizeof(*CSGenerateImportanceMapSPIRV32)),
    },
    ShaderInfo{
            "CACAOPostprocessImportanceMapA_32"s,
            "FFX_CACAO_PostprocessImportanceMapA"s,
            BindGroupLayoutCACAO::PostProcessImportanceMapA,
            shaderByteCode(CSPostprocessImportanceMapASPIRV32, sizeof(CSPostprocessImportanceMapASPIRV32) / sizeof(*CSPostprocessImportanceMapASPIRV32)),
    },
    ShaderInfo{
            "CACAOPostprocessImportanceMapB_32"s,
            "FFX_CACAO_PostprocessImportanceMapB"s,
            BindGroupLayoutCACAO::PostProcessImportanceMapB,
            shaderByteCode(CSPostprocessImportanceMapBSPIRV32, sizeof(CSPostprocessImportanceMapBSPIRV32) / sizeof(*CSPostprocessImportanceMapBSPIRV32)),
    },
    ShaderInfo{
            "CACAOEdgeSensitiveBlur1_32"s,
            "FFX_CACAO_EdgeSensitiveBlur1"s,
            BindGroupLayoutCACAO::EdgeSensitiveBlur,
            shaderByteCode(CSEdgeSensitiveBlur1SPIRV32, sizeof(CSEdgeSensitiveBlur1SPIRV32) / sizeof(*CSEdgeSensitiveBlur1SPIRV32)),
    },
    ShaderInfo{
            "CACAOEdgeSensitiveBlur2_32"s,
            "FFX_CACAO_EdgeSensitiveBlur2"s,
            BindGroupLayoutCACAO::EdgeSensitiveBlur,
            shaderByteCode(CSEdgeSensitiveBlur2SPIRV32, sizeof(CSEdgeSensitiveBlur2SPIRV32) / sizeof(*CSEdgeSensitiveBlur2SPIRV32)),
    },
    ShaderInfo{
            "CACAOEdgeSensitiveBlur3_32"s,
            "FFX_CACAO_EdgeSensitiveBlur3"s,
            BindGroupLayoutCACAO::EdgeSensitiveBlur,
            shaderByteCode(CSEdgeSensitiveBlur3SPIRV32, sizeof(CSEdgeSensitiveBlur3SPIRV32) / sizeof(*CSEdgeSensitiveBlur3SPIRV32)),
    },
    ShaderInfo{
            "CACAOEdgeSensitiveBlur4_32"s,
            "FFX_CACAO_EdgeSensitiveBlur4"s,
            BindGroupLayoutCACAO::EdgeSensitiveBlur,
            shaderByteCode(CSEdgeSensitiveBlur4SPIRV32, sizeof(CSEdgeSensitiveBlur4SPIRV32) / sizeof(*CSEdgeSensitiveBlur4SPIRV32)),
    },
    ShaderInfo{
            "CACAOEdgeSensitiveBlur5_32"s,
            "FFX_CACAO_EdgeSensitiveBlur5"s,
            BindGroupLayoutCACAO::EdgeSensitiveBlur,
            shaderByteCode(CSEdgeSensitiveBlur5SPIRV32, sizeof(CSEdgeSensitiveBlur5SPIRV32) / sizeof(*CSEdgeSensitiveBlur5SPIRV32)),
    },
    ShaderInfo{
            "CACAOEdgeSensitiveBlur6_32"s,
            "FFX_CACAO_EdgeSensitiveBlur6"s,
            BindGroupLayoutCACAO::EdgeSensitiveBlur,
            shaderByteCode(CSEdgeSensitiveBlur6SPIRV32, sizeof(CSEdgeSensitiveBlur6SPIRV32) / sizeof(*CSEdgeSensitiveBlur6SPIRV32)),
    },
    ShaderInfo{
            "CACAOEdgeSensitiveBlur7_32"s,
            "FFX_CACAO_EdgeSensitiveBlur7"s,
            BindGroupLayoutCACAO::EdgeSensitiveBlur,
            shaderByteCode(CSEdgeSensitiveBlur7SPIRV32, sizeof(CSEdgeSensitiveBlur7SPIRV32) / sizeof(*CSEdgeSensitiveBlur7SPIRV32)),
    },
    ShaderInfo{
            "CACAOEdgeSensitiveBlur8_32"s,
            "FFX_CACAO_EdgeSensitiveBlur8"s,
            BindGroupLayoutCACAO::EdgeSensitiveBlur,
            shaderByteCode(CSEdgeSensitiveBlur8SPIRV32, sizeof(CSEdgeSensitiveBlur8SPIRV32) / sizeof(*CSEdgeSensitiveBlur8SPIRV32)),
    },
    ShaderInfo{
            "CACAOApply_32"s,
            "FFX_CACAO_Apply"s,
            BindGroupLayoutCACAO::Apply,
            shaderByteCode(CSApplySPIRV32, sizeof(CSApplySPIRV32) / sizeof(*CSApplySPIRV32)),
    },
    ShaderInfo{
            "CACAONonSmartApply_32"s,
            "FFX_CACAO_NonSmartApply"s,
            BindGroupLayoutCACAO::Apply,
            shaderByteCode(CSNonSmartApplySPIRV32, sizeof(CSNonSmartApplySPIRV32) / sizeof(*CSNonSmartApplySPIRV32)),
    },
    ShaderInfo{
            "CACAONonSmartHalfApply_32"s,
            "FFX_CACAO_NonSmartHalfApply"s,
            BindGroupLayoutCACAO::Apply,
            shaderByteCode(CSNonSmartHalfApplySPIRV32, sizeof(CSNonSmartHalfApplySPIRV32) / sizeof(*CSNonSmartHalfApplySPIRV32)),
    },
    ShaderInfo{
            "CACAOUpscaleBilateral5x5Smart_32"s,
            "FFX_CACAO_UpscaleBilateral5x5Smart"s,
            BindGroupLayoutCACAO::BilateralUpSample,
            shaderByteCode(CSUpscaleBilateral5x5SmartSPIRV32, sizeof(CSUpscaleBilateral5x5SmartSPIRV32) / sizeof(*CSUpscaleBilateral5x5SmartSPIRV32)),
    },
    ShaderInfo{
            "CACAOUpscaleBilateral5x5NonSmart_32"s,
            "FFX_CACAO_UpscaleBilateral5x5NonSmart"s,
            BindGroupLayoutCACAO::BilateralUpSample,
            shaderByteCode(CSUpscaleBilateral5x5NonSmartSPIRV32, sizeof(CSUpscaleBilateral5x5NonSmartSPIRV32) / sizeof(*CSUpscaleBilateral5x5NonSmartSPIRV32)),
    },
    ShaderInfo{
            "CACAOUpscaleBilateral5x5Half_32"s,
            "FFX_CACAO_UpscaleBilateral5x5Half"s,
            BindGroupLayoutCACAO::BilateralUpSample,
            shaderByteCode(CSUpscaleBilateral5x5HalfSPIRV32, sizeof(CSUpscaleBilateral5x5HalfSPIRV32) / sizeof(*CSUpscaleBilateral5x5HalfSPIRV32)),
    },
};

struct InputBindGroupBindingInfo {
    BindGroupCACAO bindGroup;
    TextureViewsCACAO shaderTextureView;
    uint32_t binding;
};

const std::array<InputBindGroupBindingInfo, InputBindGroupBindingsCount> inputBindGroupResourcesInfo = {
    // { bindgroup index, shaderTextureViewsInfo index, binding number }
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase0, TextureViewsCACAO::DeinterleavedDepthArray0, 0 }, // GenerateAdaptiveBase0 DeinterleavedDepths0
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase0, TextureViewsCACAO::DeinterleavedNormals, 1 }, // GenerateAdaptiveBase0 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase1, TextureViewsCACAO::DeinterleavedDepthArray1, 0 }, // GenerateAdaptiveBase1 DeinterleavedDepths1
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase1, TextureViewsCACAO::DeinterleavedNormals, 1 }, // GenerateAdaptiveBase1 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase2, TextureViewsCACAO::DeinterleavedDepthArray2, 0 }, // GenerateAdaptiveBase2 DeinterleavedDepths2
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase2, TextureViewsCACAO::DeinterleavedNormals, 1 }, // GenerateAdaptiveBase2 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase3, TextureViewsCACAO::DeinterleavedDepthArray3, 0 }, // GenerateAdaptiveBase3 DeinterleavedDepths3
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase3, TextureViewsCACAO::DeinterleavedNormals, 1 }, // GenerateAdaptiveBase3 DeinterleavedNormals

    InputBindGroupBindingInfo{ BindGroupCACAO::Generate0, TextureViewsCACAO::DeinterleavedDepthArray0, 0 }, // Generate0 DeinterleavedDepths0
    InputBindGroupBindingInfo{ BindGroupCACAO::Generate0, TextureViewsCACAO::DeinterleavedNormals, 1 }, // Generate0 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::Generate1, TextureViewsCACAO::DeinterleavedDepthArray1, 0 }, // Generate1 DeinterleavedDepths1
    InputBindGroupBindingInfo{ BindGroupCACAO::Generate1, TextureViewsCACAO::DeinterleavedNormals, 1 }, // Generate1 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::Generate2, TextureViewsCACAO::DeinterleavedDepthArray2, 0 }, // Generate2 DeinterleavedDepths2
    InputBindGroupBindingInfo{ BindGroupCACAO::Generate2, TextureViewsCACAO::DeinterleavedNormals, 1 }, // Generate2 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::Generate3, TextureViewsCACAO::DeinterleavedDepthArray3, 0 }, // Generate3 DeinterleavedDepths3
    InputBindGroupBindingInfo{ BindGroupCACAO::Generate3, TextureViewsCACAO::DeinterleavedNormals, 1 }, // Generate3 DeinterleavedNormals

    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive0, TextureViewsCACAO::DeinterleavedDepthArray0, 0 }, // GenerateAdaptive0 DeinterleavedDepths0
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive0, TextureViewsCACAO::DeinterleavedNormals, 1 }, // GenerateAdaptive0 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive0, TextureViewsCACAO::ImportanceMap, 3 }, // GenerateAdaptive0 ImportanceMap
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive0, TextureViewsCACAO::SSAOBufferPongArray0, 4 }, // GenerateAdaptive0 SSABBufferPong0
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive1, TextureViewsCACAO::DeinterleavedDepthArray1, 0 }, // GenerateAdaptive1 DeinterleavedDepths1
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive1, TextureViewsCACAO::DeinterleavedNormals, 1 }, // GenerateAdaptive1 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive1, TextureViewsCACAO::ImportanceMap, 3 }, // GenerateAdaptive1 ImportanceMap
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive1, TextureViewsCACAO::SSAOBufferPongArray1, 4 }, // GenerateAdaptive1 SSABBufferPong1
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive2, TextureViewsCACAO::DeinterleavedDepthArray2, 0 }, // GenerateAdaptive2 DeinterleavedDepths2
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive2, TextureViewsCACAO::DeinterleavedNormals, 1 }, // GenerateAdaptive2 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive2, TextureViewsCACAO::ImportanceMap, 3 }, // GenerateAdaptive2 ImportanceMap
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive2, TextureViewsCACAO::SSAOBufferPongArray2, 4 }, // GenerateAdaptive2 SSABBufferPong2
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive3, TextureViewsCACAO::DeinterleavedDepthArray3, 0 }, // GenerateAdaptive3 DeinterleavedDepths3
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive3, TextureViewsCACAO::DeinterleavedNormals, 1 }, // GenerateAdaptive3 DeinterleavedNormals
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive3, TextureViewsCACAO::ImportanceMap, 3 }, // GenerateAdaptive3 ImportanceMap
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive3, TextureViewsCACAO::SSAOBufferPongArray3, 4 }, // GenerateAdaptive3 SSABBufferPong3

    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateImportanceMap, TextureViewsCACAO::SSAOBufferPong, 0 }, // GenerateImportanceMap SSABBufferPong
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateImportanceMapA, TextureViewsCACAO::ImportanceMap, 0 }, // PostProcessImportanceMapA ImportanceMap
    InputBindGroupBindingInfo{ BindGroupCACAO::GenerateImportanceMapB, TextureViewsCACAO::ImportanceMapPong, 0 }, // PostProcessImportanceMapB ImportanceMapPong

    InputBindGroupBindingInfo{ BindGroupCACAO::EdgeSensitiveBlur0, TextureViewsCACAO::SSAOBufferPingArray0, 0 }, // EdgeSensitiveBlur0 SSABBufferPing0
    InputBindGroupBindingInfo{ BindGroupCACAO::EdgeSensitiveBlur1, TextureViewsCACAO::SSAOBufferPingArray1, 0 }, // EdgeSensitiveBlur1 SSABBufferPing1
    InputBindGroupBindingInfo{ BindGroupCACAO::EdgeSensitiveBlur2, TextureViewsCACAO::SSAOBufferPingArray2, 0 }, // EdgeSensitiveBlur2 SSABBufferPing2
    InputBindGroupBindingInfo{ BindGroupCACAO::EdgeSensitiveBlur3, TextureViewsCACAO::SSAOBufferPingArray3, 0 }, // EdgeSensitiveBlur3 SSABBufferPing3

    InputBindGroupBindingInfo{ BindGroupCACAO::BilateralUpsamplePing, TextureViewsCACAO::SSAOBufferPing, 0 }, // BilateralUpsamplePing SSABBufferPing
    InputBindGroupBindingInfo{ BindGroupCACAO::BilateralUpsamplePing, TextureViewsCACAO::DeinterleavedDepth, 2 }, // BilateralUpsamplePing DeinterleaveDepth
    InputBindGroupBindingInfo{ BindGroupCACAO::BilateralUpsamplePong, TextureViewsCACAO::SSAOBufferPong, 0 }, // BilateralUpsamplePong SSABBufferPong
    InputBindGroupBindingInfo{ BindGroupCACAO::BilateralUpsamplePong, TextureViewsCACAO::DeinterleavedDepth, 2 }, // BilateralUpsamplePong DeinterleaveDepth

    InputBindGroupBindingInfo{ BindGroupCACAO::ApplyPing, TextureViewsCACAO::SSAOBufferPing, 0 }, // ApplyPing SSABBufferPing
    InputBindGroupBindingInfo{ BindGroupCACAO::ApplyPong, TextureViewsCACAO::SSAOBufferPong, 0 }, // ApplyPong SSABBufferPong
};

struct OutputBindGroupBindingInfo {
    BindGroupCACAO bindGroup;
    TextureViewsCACAO unorderedAccessTextureView;
    uint32_t binding;
};

const std::array<OutputBindGroupBindingInfo, OutputBindGroupBindingsCount> outputBindGroupResourcesInfo = {
    // { bindgroup index, unorderedAccessTextureViewsInfo index, binding number }
    OutputBindGroupBindingInfo{ BindGroupCACAO::PrepareDepth, TextureViewsCACAO::DeinterleavedDepthMip0, 0 }, // PrepareDepth DeinterleavedDepthMip0
    OutputBindGroupBindingInfo{ BindGroupCACAO::PrepareDepthMips, TextureViewsCACAO::DeinterleavedDepthMip0, 0 }, // PrepareDepthMips DeinterleavedDepthMip0
    OutputBindGroupBindingInfo{ BindGroupCACAO::PrepareDepthMips, TextureViewsCACAO::DeinterleavedDepthMip1, 1 }, // PrepareDepthMips DeinterleavedDepthMip1
    OutputBindGroupBindingInfo{ BindGroupCACAO::PrepareDepthMips, TextureViewsCACAO::DeinterleavedDepthMip2, 2 }, // PrepareDepthMips DeinterleavedDepthMip2
    OutputBindGroupBindingInfo{ BindGroupCACAO::PrepareDepthMips, TextureViewsCACAO::DeinterleavedDepthMip3, 3 }, // PrepareDepthMips DeinterleavedDepthMip3

    OutputBindGroupBindingInfo{ BindGroupCACAO::PrepareNormals, TextureViewsCACAO::DeinterleavedNormals, 0 }, // PrepareNormals DeinterleavedNormals
    OutputBindGroupBindingInfo{ BindGroupCACAO::PrepareNormalsFromInputNormals, TextureViewsCACAO::DeinterleavedNormals, 0 }, // PrepareNormalsFromInputNormals DeinterleavedNormals

    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase0, TextureViewsCACAO::SSAOBufferPongArray0, 0 }, // GenerateAdaptiveBase0 SSABBufferPong0
    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase1, TextureViewsCACAO::SSAOBufferPongArray1, 0 }, // GenerateAdaptiveBase1 SSABBufferPong1
    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase2, TextureViewsCACAO::SSAOBufferPongArray2, 0 }, // GenerateAdaptiveBase2 SSABBufferPong2
    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptiveBase3, TextureViewsCACAO::SSAOBufferPongArray3, 0 }, // GenerateAdaptiveBase3 SSABBufferPong3

    OutputBindGroupBindingInfo{ BindGroupCACAO::Generate0, TextureViewsCACAO::SSAOBufferPingArray0, 0 }, // Generate0 SSABBufferPing0
    OutputBindGroupBindingInfo{ BindGroupCACAO::Generate1, TextureViewsCACAO::SSAOBufferPingArray1, 0 }, // Generate1 SSABBufferPing1
    OutputBindGroupBindingInfo{ BindGroupCACAO::Generate2, TextureViewsCACAO::SSAOBufferPingArray2, 0 }, // Generate2 SSABBufferPing2
    OutputBindGroupBindingInfo{ BindGroupCACAO::Generate3, TextureViewsCACAO::SSAOBufferPingArray3, 0 }, // Generate3 SSABBufferPing3

    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive0, TextureViewsCACAO::SSAOBufferPingArray0, 0 }, // GenerateAdaptative0 SSAOBufferPing0
    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive1, TextureViewsCACAO::SSAOBufferPingArray1, 0 }, // GenerateAdaptative1 SSAOBufferPing1
    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive2, TextureViewsCACAO::SSAOBufferPingArray2, 0 }, // GenerateAdaptative2 SSAOBufferPing2
    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateAdaptive3, TextureViewsCACAO::SSAOBufferPingArray3, 0 }, // GenerateAdaptative3 SSAOBufferPing3

    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateImportanceMap, TextureViewsCACAO::ImportanceMap, 0 }, // GenerateImportanceMap ImportanceMap
    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateImportanceMapA, TextureViewsCACAO::ImportanceMapPong, 0 }, // PostProcessImportanceMapA ImportanceMapPong
    OutputBindGroupBindingInfo{ BindGroupCACAO::GenerateImportanceMapB, TextureViewsCACAO::ImportanceMap, 0 }, // PostProcessImportanceMapB ImportanceMap

    OutputBindGroupBindingInfo{ BindGroupCACAO::EdgeSensitiveBlur0, TextureViewsCACAO::SSAOBufferPongArray0, 0 }, // EdgeSensitiveBlur0 SSAOBufferPong0
    OutputBindGroupBindingInfo{ BindGroupCACAO::EdgeSensitiveBlur1, TextureViewsCACAO::SSAOBufferPongArray1, 0 }, // EdgeSensitiveBlur1 SSAOBufferPong1
    OutputBindGroupBindingInfo{ BindGroupCACAO::EdgeSensitiveBlur2, TextureViewsCACAO::SSAOBufferPongArray2, 0 }, // EdgeSensitiveBlur2 SSAOBufferPong2
    OutputBindGroupBindingInfo{ BindGroupCACAO::EdgeSensitiveBlur3, TextureViewsCACAO::SSAOBufferPongArray3, 0 }, // EdgeSensitiveBlur3 SSAOBufferPong3
};

// UBO struct
struct UBOConstants {
    float DepthUnpackConsts[2];
    float CameraTanHalfFOV[2];

    float NDCToViewMul[2];
    float NDCToViewAdd[2];

    float DepthBufferUVToViewMul[2];
    float DepthBufferUVToViewAdd[2];

    float EffectRadius;
    float EffectShadowStrength;
    float EffectShadowPow;
    float EffectShadowClamp;

    float EffectFadeOutMul;
    float EffectFadeOutAdd;
    float EffectHorizonAngleThreshold;
    float EffectSamplingRadiusNearLimitRec;

    float DepthPrecisionOffsetMod;
    float NegRecEffectRadius;
    float LoadCounterAvgDiv;
    float AdaptiveSampleCountLimit;

    float InvSharpness;
    int PassIndex;
    float BilateralSigmaSquared;
    float BilateralSimilarityDistanceSigma;

    float PatternRotScaleMatrices[5][4];

    float NormalsUnpackMul;
    float NormalsUnpackAdd;
    float DetailAOStrength;
    float Dummy0;

    float SSAOBufferDimensions[2];
    float SSAOBufferInverseDimensions[2];

    float DepthBufferDimensions[2];
    float DepthBufferInverseDimensions[2];

    int DepthBufferOffset[2];
    float PerPassFullResUVOffset[2];

    float InputOutputBufferDimensions[2];
    float InputOutputBufferInverseDimensions[2];

    float ImportanceMapDimensions[2];
    float ImportanceMapInverseDimensions[2];

    float DeinterleavedDepthBufferDimensions[2];
    float DeinterleavedDepthBufferInverseDimensions[2];

    float DeinterleavedDepthBufferOffset[2];
    float DeinterleavedDepthBufferNormalisedOffset[2];

    float NormalsWorldToViewspaceMatrix[4][4];
};

inline std::string assetPath()
{
#if defined(GLTF_RENDERER_ASSET_PATH)
    return GLTF_RENDERER_ASSET_PATH;
#else
    return "";
#endif
}

const uint32_t tileSize = 8;

} // namespace

FFX_Cacao::BufferSizeInfo::BufferSizeInfo(uint32_t width, uint32_t height, bool useDownsampledSsao)
{
    const uint32_t _halfWidth = (width + 1) / 2;
    const uint32_t _halfHeight = (height + 1) / 2;
    const uint32_t _quarterWidth = (_halfWidth + 1) / 2;
    const uint32_t _quarterHeight = (_halfHeight + 1) / 2;
    const uint32_t _eighthWidth = (_quarterWidth + 1) / 2;
    const uint32_t _eighthHeight = (_quarterHeight + 1) / 2;

    const uint32_t _depthBufferWidth = width;
    const uint32_t _depthBufferHeight = height;
    const uint32_t _depthBufferHalfWidth = _halfWidth;
    const uint32_t _depthBufferHalfHeight = _halfHeight;
    const uint32_t _depthBufferQuarterWidth = _quarterWidth;
    const uint32_t _depthBufferQuarterHeight = _quarterHeight;

    const uint32_t _depthBufferXOffset = 0;
    const uint32_t _depthBufferYOffset = 0;
    const uint32_t _depthBufferHalfXOffset = 0;
    const uint32_t _depthBufferHalfYOffset = 0;
    const uint32_t _depthBufferQuarterXOffset = 0;
    const uint32_t _depthBufferQuarterYOffset = 0;

    inputOutputBufferWidth = width;
    inputOutputBufferHeight = height;
    depthBufferXOffset = _depthBufferXOffset;
    depthBufferYOffset = _depthBufferYOffset;
    depthBufferWidth = _depthBufferWidth;
    depthBufferHeight = _depthBufferHeight;

    if (useDownsampledSsao) {
        ssaoBufferWidth = _quarterWidth;
        ssaoBufferHeight = _quarterHeight;
        deinterleavedDepthBufferXOffset = _depthBufferQuarterXOffset;
        deinterleavedDepthBufferYOffset = _depthBufferQuarterYOffset;
        deinterleavedDepthBufferWidth = _depthBufferQuarterWidth;
        deinterleavedDepthBufferHeight = _depthBufferQuarterHeight;
        importanceMapWidth = _eighthWidth;
        importanceMapHeight = _eighthHeight;
        downsampledSsaoBufferWidth = _halfWidth;
        downsampledSsaoBufferHeight = _halfHeight;
    } else {
        ssaoBufferWidth = _halfWidth;
        ssaoBufferHeight = _halfHeight;
        deinterleavedDepthBufferXOffset = _depthBufferHalfXOffset;
        deinterleavedDepthBufferYOffset = _depthBufferHalfYOffset;
        deinterleavedDepthBufferWidth = _depthBufferHalfWidth;
        deinterleavedDepthBufferHeight = _depthBufferHalfHeight;
        importanceMapWidth = _quarterWidth;
        importanceMapHeight = _quarterHeight;
        downsampledSsaoBufferWidth = 1;
        downsampledSsaoBufferHeight = 1;
    }
}

FFX_Cacao::FFX_Cacao(Device *device)
    : m_device(device)
{
}

FFX_Cacao::~FFX_Cacao() = default;

void FFX_Cacao::initializeScene(KDGpu::Format colorFormat, KDGpu::Format depthFormat, Settings settings)
{
    m_settings = settings;

    initializeCacaoComputePass();
    initializeCacaoFullScreenPass(colorFormat, depthFormat);
}

void FFX_Cacao::initializeCacaoComputePass()
{
    // Samplers
    {
        // clang-format off
        const std::array<SamplerOptions, SamplersCount> samplerOptions = {
            // PointClampSampler
            SamplerOptions{ .magFilter = FilterMode::Linear, .minFilter = FilterMode::Linear, .u = AddressMode::ClampToEdge, .v = AddressMode::ClampToEdge, },
            // PointMirrorSampler
            SamplerOptions{ .magFilter = FilterMode::Linear, .minFilter = FilterMode::Linear, .u = AddressMode::Repeat, .v = AddressMode::Repeat, },
            // LinearClampSampler
            SamplerOptions{ .magFilter = FilterMode::Linear, .minFilter = FilterMode::Linear, .mipmapFilter = MipmapFilterMode::Linear, .u = AddressMode::ClampToEdge, .v = AddressMode::ClampToEdge, },
            // ViewspaceDepthTapSampler
            SamplerOptions{ .magFilter = FilterMode::Linear, .minFilter = FilterMode::Linear, .mipmapFilter = MipmapFilterMode::Nearest, .u = AddressMode::ClampToEdge, .v = AddressMode::ClampToEdge, },
            // RealPointClampSampler
            SamplerOptions{ .magFilter = FilterMode::Nearest,.minFilter = FilterMode::Nearest, .mipmapFilter = MipmapFilterMode::Nearest, .u = AddressMode::Repeat, .v = AddressMode::Repeat, },
        };
        // clang-format on

        for (size_t i = 0; i < SamplersCount; ++i)
            m_samplers.emplace_back(m_device->createSampler(samplerOptions[i]));
    }

    // BindGroupLayouts
    {

        auto createBindGroupLayoutOptions = [this](const BindGroupVariablesCount vars) {
            BindGroupLayoutOptions options;
            options.bindings.reserve(vars.samplersCount + vars.constantsCount + vars.inputsCount + vars.outputsCount);
            // clang-format off
            for (size_t i = 0; i < vars.samplersCount; ++i)
                options.bindings.emplace_back(ResourceBindingLayout{ .binding = uint32_t(i), .resourceType = ResourceBindingType::Sampler, .shaderStages = ShaderStageFlags(ShaderStageFlagBits::ComputeBit), });
            for (size_t i = 0; i < vars.constantsCount; ++i)
                options.bindings.emplace_back(ResourceBindingLayout{ .binding = uint32_t(10 + i), .resourceType = ResourceBindingType::UniformBuffer, .shaderStages = ShaderStageFlags(ShaderStageFlagBits::ComputeBit), });
            for (size_t i = 0; i < vars.inputsCount; ++i)
                options.bindings.emplace_back(ResourceBindingLayout{ .binding = uint32_t(20 + i), .resourceType = ResourceBindingType::SampledImage, .shaderStages = ShaderStageFlags(ShaderStageFlagBits::ComputeBit), });
            for (size_t i = 0; i < vars.outputsCount; ++i)
                options.bindings.emplace_back(ResourceBindingLayout{ .binding = uint32_t(30 + i), .resourceType = ResourceBindingType::StorageImage, .shaderStages = ShaderStageFlags(ShaderStageFlagBits::ComputeBit), });
            // clang-format on
            return options;
        };

        for (size_t i = 0; i < static_cast<uint32_t>(BindGroupLayoutCACAO::Max); ++i)
            m_bindGroupLayouts.emplace_back(m_device->createBindGroupLayout(createBindGroupLayoutOptions(bindGroupVariablesCount[i])));
    }

    // PipelineLayouts
    {
        // We have a 1:1 mapping between backgroudlayout and pipelinelayout
        for (size_t i = 0; i < static_cast<uint32_t>(BindGroupLayoutCACAO::Max); ++i)
            m_pipelineLayouts.emplace_back(m_device->createPipelineLayout(PipelineLayoutOptions{
                    .bindGroupLayouts{
                            m_bindGroupLayouts[i],
                    },
            }));
    }

    // Shader Modules
    {
        for (size_t i = 0; i < static_cast<uint32_t>(PipelineCACAO::Max); ++i) {
            m_shaderModules.emplace_back(m_device->createShaderModule(shadersInfo[i].byteCode));
        }
    }

    // Pipelines
    {
        for (size_t i = 0; i < static_cast<uint32_t>(PipelineCACAO::Max); ++i) {
            const uint32_t pipelineLayoutIndex = static_cast<uint32_t>(shadersInfo[i].pipelineLayout);
            m_pipelines.emplace_back(m_device->createComputePipeline(ComputePipelineOptions{
                    .layout = m_pipelineLayouts[pipelineLayoutIndex],
                    .shaderStage = {
                            .shaderModule = m_shaderModules[i],
                            .entryPoint = shadersInfo[i].entrypoint,
                    },
            }));
        }
    }

    // UBOs
    for (size_t i = 0; i < UBOCount; ++i) {
        m_ubos.emplace_back(m_device->createBuffer(BufferOptions{
                .size = sizeof(UBOConstants),
                .usage = BufferUsageFlagBits::UniformBufferBit | BufferUsageFlagBits::TransferDstBit,
                .memoryUsage = MemoryUsage::CpuToGpu,
        }));
    }

    // BindGroups
    {
        // For now assume 1 swapchain image which is fine when using the SimpleExampleLayer
        for (size_t i = 0; i < static_cast<uint32_t>(BindGroupCACAO::Max); ++i) {
            const BindGroupDescription &desc = bindGroupDescriptions[i];
            const uint32_t bindGroupLayoutIdx = static_cast<uint32_t>(desc.bindGroupLayout);
            m_bindGroups.emplace_back(m_device->createBindGroup(BindGroupOptions{
                    .layout = m_bindGroupLayouts[bindGroupLayoutIdx],
            }));

            // Update BindGroup resources for resources that are constant
            const BindGroupVariablesCount variables = bindGroupVariablesCount[bindGroupLayoutIdx];
            BindGroup &bindGroup = m_bindGroups.back();
            for (uint32_t samplerIdx = 0; samplerIdx < variables.samplersCount; ++samplerIdx) {
                bindGroup.update(BindGroupEntry{
                        .binding = samplerIdx,
                        .resource = SamplerBinding{
                                .sampler = m_samplers[samplerIdx],
                        },
                });
            }
        };

        // Set UBO on BindGroups (we will update the content of the UBO but the binding remains constant)
        for (size_t i = 0, m = static_cast<uint32_t>(BindGroupCACAO::Max); i < m; ++i) {
            const BindGroupDescription &desr = bindGroupDescriptions[i];
            BindGroup &bindGroup = m_bindGroups[i];

            bindGroup.update(BindGroupEntry{
                    .binding = 10,
                    .resource = UniformBufferBinding{
                            .buffer = m_ubos[desr.passIdx],
                    },
            });
        }
    }

    // LoadCounter
    {
        m_loadCounter = m_device->createTexture(TextureOptions{
                .type = TextureType::TextureType1D,
                .format = Format::R32_UINT,
                .extent = { 1, 1, 1 },
                .mipLevels = 1,
                .usage = TextureUsageFlagBits::StorageBit | TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit,
                .memoryUsage = MemoryUsage::GpuOnly,
        });

        m_loadCounterView = m_loadCounter.createView(TextureViewOptions{
                .viewType = ViewType::ViewType1D,
                .format = Format::R32_UINT,
                .range = TextureSubresourceRange{
                        .aspectMask = TextureAspectFlagBits::ColorBit,
                        .levelCount = 1,
                        .layerCount = 1,
                },
        });
    }
}

void FFX_Cacao::initializeCacaoFullScreenPass(KDGpu::Format colorFormat, KDGpu::Format depthFormat)
{
    // Create bind group layout consisting of a single binding holding the texture the 1st pass rendered to
    m_fsqTextureBindGroupLayout = m_device->createBindGroupLayout(BindGroupLayoutOptions{
            .bindings = {
                    {
                            .binding = 0,
                            .resourceType = ResourceBindingType::CombinedImageSampler,
                            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit),
                    },
            },
    });

    // Create a pipeline layout (array of bind group layouts)
    m_fsqPipelineLayout = m_device->createPipelineLayout(PipelineLayoutOptions{
            .bindGroupLayouts = { m_fsqTextureBindGroupLayout },
    });

    // Create a vertex shader and fragment shader for fullscreen quad
    const auto vertexShaderPath = assetPath() + "/shaders/06_ffx_cacao/fullscreenquad.vert.spv";
    auto vertexShader = m_device->createShaderModule(KDGpuExample::readShaderFile(vertexShaderPath));

    const auto fragmentShaderPath = assetPath() + "/shaders/06_ffx_cacao/fullscreenquad.frag.spv";
    auto fragmentShader = m_device->createShaderModule(KDGpuExample::readShaderFile(fragmentShaderPath));

    // Create a pipeline
    m_fsqPipeline = m_device->createGraphicsPipeline(GraphicsPipelineOptions{
            .shaderStages = {
                    { .shaderModule = vertexShader, .stage = ShaderStageFlagBits::VertexBit },
                    { .shaderModule = fragmentShader, .stage = ShaderStageFlagBits::FragmentBit },
            },
            .layout = m_fsqPipelineLayout,
            .vertex = {
                    .buffers = {},
                    .attributes = {},
            },
            .renderTargets = { { .format = colorFormat } },
            .depthStencil = {
                    .format = depthFormat,
                    .depthWritesEnabled = false,
                    .depthCompareOperation = CompareOperation::Always,
            },
    });

    // Prepare pass options (we don't clear depth or color buffers)
    m_fsqPassOptions = {
        .colorAttachments = {
                { .view = {}, // Not setting the swapchain texture view just yet
                  .finalLayout = TextureLayout::PresentSrc } },
        .depthStencilAttachment = {
                .view = {},
        } // Not setting the depth texture view just yet
    };

    // Create a sampler we can use to sample from the color texture in the final pass
    m_fsqPassSampler = m_device->createSampler();

    // Create a bindGroup to hold the CACAO Output Color Texture
    m_fsqTextureBindGroup = m_device->createBindGroup(BindGroupOptions{
            .layout = m_fsqTextureBindGroupLayout,
    });
}

void FFX_Cacao::cleanupScene()
{
    cleanupCacaoComputePass();
    cleanupCacaoFullScreenPass();
}

void FFX_Cacao::cleanupCacaoComputePass()
{
    m_loadCounterView = {};
    m_loadCounter = {};

    m_outputView = {};
    m_output = {};

    m_bindGroups.clear();
    m_pipelines.clear();
    m_shaderModules.clear();
    m_pipelineLayouts.clear();
    m_bindGroupLayouts.clear();
    m_samplers.clear();
    m_ubos.clear();
    m_textureViews.clear();
    m_textures.clear();
}

void FFX_Cacao::cleanupCacaoFullScreenPass()
{
    m_fsqTextureBindGroup = {};
    m_fsqPassSampler = {};
    m_fsqPassOptions = {};
    m_fsqPipeline = {};
    m_fsqPipelineLayout = {};
    m_fsqTextureBindGroupLayout = {};
}

void FFX_Cacao::resize(uint32_t width, uint32_t height, const Texture &depthTexture, const TextureView &depthView, const KDGpu::TextureView &normalsView)
{
    m_bufferSizeInfo = BufferSizeInfo(width, height);
    m_depthHandle = depthTexture.handle();
    m_oldDepthTextureLayout = TextureLayout::Undefined;

    resizeCacaoComputePass(width, height, depthView, normalsView);
    resizeCacaoFullScreenPass(width, height, depthView);
}

void FFX_Cacao::resizeCacaoFullScreenPass(uint32_t width, uint32_t height, const TextureView &depthView)
{
    m_fsqPassOptions.depthStencilAttachment.view = depthView;

    // Update fsq Cacao texture since it might have changed following the resize
    m_fsqTextureBindGroup.update(BindGroupEntry{
            .binding = 0,
            .resource = TextureViewSamplerBinding{
                    .textureView = m_outputView,
                    .sampler = m_fsqPassSampler,
            },
    });
}

void FFX_Cacao::resizeCacaoComputePass(uint32_t width, uint32_t height, const TextureView &depthView, const KDGpu::TextureView &normalsView)
{
    // Textures
    {
        const size_t texturesCount = static_cast<uint32_t>(TextureCACAO::Max);
        m_textures.resize(texturesCount);
        for (size_t i = 0; i < texturesCount; ++i) {
            const TextureInfo &textureInfo = texturesInfo[i];
            m_textures[i] = m_device->createTexture(TextureOptions{
                    .type = TextureType::TextureType2D,
                    .format = textureInfo.format,
                    .extent = { (m_bufferSizeInfo.*textureInfo.width), (m_bufferSizeInfo.*textureInfo.height), 1 },
                    .mipLevels = textureInfo.mipLevels,
                    .arrayLayers = textureInfo.arraySize,
                    .usage = TextureUsageFlagBits::StorageBit | TextureUsageFlagBits::SampledBit,
                    .memoryUsage = MemoryUsage::GpuOnly,
            });
        }
    }

    // TextureViews
    {
        const size_t textureViewCount = static_cast<size_t>(TextureViewsCACAO::Max);
        m_textureViews.resize(textureViewCount);
        for (size_t i = 0; i < textureViewCount; ++i) {
            const TextureViewInfo &textureViewInfo = textureViewsInfo[i];
            const size_t textureIdx = static_cast<uint32_t>(textureViewInfo.texture);
            const TextureInfo &textureInfo = texturesInfo[textureIdx];
            Texture &tex = m_textures[textureIdx];
            m_textureViews[i] = tex.createView(TextureViewOptions{
                    .viewType = textureViewInfo.viewType,
                    .format = textureInfo.format,
                    .range = TextureSubresourceRange{
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                            .baseMipLevel = textureViewInfo.mostDetailedMip,
                            .levelCount = textureViewInfo.mipLevels,
                            .baseArrayLayer = textureViewInfo.firstArraySlice,
                            .layerCount = textureViewInfo.arraySize,
                    },
            });
        }
    }

    // Output Texture and Views
    {
        m_outputView = {};
        m_output = m_device->createTexture(TextureOptions{
                .type = TextureType::TextureType2D,
                .format = Format::R32_SFLOAT,
                .extent = { width, height, 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .usage = TextureUsageFlagBits::StorageBit | TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit,
                .memoryUsage = MemoryUsage::GpuOnly,
        });
        m_outputView = m_output.createView(TextureViewOptions{
                .viewType = ViewType::ViewType2D,
                .format = Format::R32_SFLOAT,
                .range = TextureSubresourceRange{
                        .aspectMask = TextureAspectFlagBits::ColorBit,
                },
        });
    }

    // Update Bind Groups
    for (const InputBindGroupBindingInfo &inputBinding : inputBindGroupResourcesInfo) {
        BindGroup &bindGroup = m_bindGroups[static_cast<uint32_t>(inputBinding.bindGroup)];
        bindGroup.update(BindGroupEntry{
                .binding = 20 + inputBinding.binding,
                .resource = TextureViewBinding{
                        .textureView = m_textureViews[static_cast<uint32_t>(inputBinding.shaderTextureView)],
                },
        });
    }

    for (const OutputBindGroupBindingInfo &outputBinding : outputBindGroupResourcesInfo) {
        BindGroup &bindGroup = m_bindGroups[static_cast<uint32_t>(outputBinding.bindGroup)];
        bindGroup.update(BindGroupEntry{
                .binding = 30 + outputBinding.binding,
                .resource = ImageBinding{
                        .textureView = m_textureViews[static_cast<uint32_t>(outputBinding.unorderedAccessTextureView)],
                },
        });
    }

    // Depth, Output and Counter View updates
    {
        struct BindGroupUptates {
            BindGroupCACAO bindGroup;
            uint32_t binding;
            BindingResource resource;
        };

        const std::vector<BindGroupUptates> updates = {
            BindGroupUptates{ BindGroupCACAO::PrepareDepth, 20, TextureViewBinding{ .textureView = depthView } },
            BindGroupUptates{ BindGroupCACAO::PrepareDepthMips, 20, TextureViewBinding{ .textureView = depthView } },
            BindGroupUptates{ BindGroupCACAO::PrepareNormals, 20, TextureViewBinding{ .textureView = depthView } },
            BindGroupUptates{ BindGroupCACAO::BilateralUpsamplePing, 21, TextureViewBinding{ .textureView = depthView } },
            BindGroupUptates{ BindGroupCACAO::BilateralUpsamplePong, 21, TextureViewBinding{ .textureView = depthView } },

            BindGroupUptates{ BindGroupCACAO::BilateralUpsamplePing, 30, ImageBinding{ .textureView = m_outputView } },
            BindGroupUptates{ BindGroupCACAO::BilateralUpsamplePong, 30, ImageBinding{ .textureView = m_outputView } },
            BindGroupUptates{ BindGroupCACAO::ApplyPing, 30, ImageBinding{ .textureView = m_outputView } },
            BindGroupUptates{ BindGroupCACAO::ApplyPong, 30, ImageBinding{ .textureView = m_outputView } },

            BindGroupUptates{ BindGroupCACAO::GenerateImportanceMapB, 31, ImageBinding{ .textureView = m_loadCounterView } },
            BindGroupUptates{ BindGroupCACAO::ClearLoadCounter, 30, ImageBinding{ .textureView = m_loadCounterView } },
            BindGroupUptates{ BindGroupCACAO::GenerateAdaptive0, 22, TextureViewBinding{ .textureView = m_loadCounterView } },
            BindGroupUptates{ BindGroupCACAO::GenerateAdaptive1, 22, TextureViewBinding{ .textureView = m_loadCounterView } },
            BindGroupUptates{ BindGroupCACAO::GenerateAdaptive2, 22, TextureViewBinding{ .textureView = m_loadCounterView } },
            BindGroupUptates{ BindGroupCACAO::GenerateAdaptive3, 22, TextureViewBinding{ .textureView = m_loadCounterView } },
        };

        for (const BindGroupUptates &update : updates) {
            BindGroup &bindGroup = m_bindGroups[static_cast<uint32_t>(update.bindGroup)];
            bindGroup.update(BindGroupEntry{
                    .binding = update.binding,
                    .resource = update.resource,
            });
        }

        if (!m_settings.generateNormals && normalsView.isValid()) {
            BindGroup &bindGroup = m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::PrepareNormalsFromInputNormals)];
            bindGroup.update(BindGroupEntry{
                    .binding = 20,
                    .resource = TextureViewBinding{
                            .textureView = normalsView,
                    },
            });
        }
    }
}

void updateUBOConstants(UBOConstants *consts, const FFX_Cacao::Settings *settings,
                        const FFX_Cacao::BufferSizeInfo *bufferSizeInfo,
                        const glm::mat4 &proj, const glm::mat4 &normalsToView)
{
    consts->BilateralSigmaSquared = settings->bilateralSigmaSquared;
    consts->BilateralSimilarityDistanceSigma = settings->bilateralSimilarityDistanceSigma;

    if (settings->generateNormals) {
        std::memcpy(consts->NormalsWorldToViewspaceMatrix, glm::value_ptr(glm::mat4(1.0f)), 16 * sizeof(float));
    } else {
        std::memcpy(consts->NormalsWorldToViewspaceMatrix, glm::value_ptr(normalsToView), 16 * sizeof(float));
    }

    // used to get average load per pixel; 9.0 is there to compensate for only doing every 9th InterlockedAdd in PSPostprocessImportanceMapB for performance reasons
    consts->LoadCounterAvgDiv = 9.0f / (float)(bufferSizeInfo->importanceMapWidth * bufferSizeInfo->importanceMapHeight * 255.0);

#define MATRIX_ROW_MAJOR_ORDER 1

    float depthLinearizeMul = (MATRIX_ROW_MAJOR_ORDER) ? (-proj[3][2]) : (-proj[2][3]); // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
    float depthLinearizeAdd = (MATRIX_ROW_MAJOR_ORDER) ? (proj[2][2]) : (proj[2][2]); // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );
    // correct the handedness issue. need to make sure this below is correct, but I think it is.
    if (depthLinearizeMul * depthLinearizeAdd < 0)
        depthLinearizeAdd = -depthLinearizeAdd;
    consts->DepthUnpackConsts[0] = depthLinearizeMul;
    consts->DepthUnpackConsts[1] = depthLinearizeAdd;

    float tanHalfFOVY = 1.0f / proj[1][1]; // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
    float tanHalfFOVX = 1.0F / proj[0][0]; // = tanHalfFOVY * drawContext.Camera.GetAspect( );
    consts->CameraTanHalfFOV[0] = tanHalfFOVX;
    consts->CameraTanHalfFOV[1] = tanHalfFOVY;

    consts->NDCToViewMul[0] = consts->CameraTanHalfFOV[0] * 2.0f;
    consts->NDCToViewMul[1] = consts->CameraTanHalfFOV[1] * -2.0f;
    consts->NDCToViewAdd[0] = consts->CameraTanHalfFOV[0] * -1.0f;
    consts->NDCToViewAdd[1] = consts->CameraTanHalfFOV[1] * 1.0f;

    float ratio = ((float)bufferSizeInfo->inputOutputBufferWidth) / ((float)bufferSizeInfo->depthBufferWidth);
    float border = (1.0f - ratio) / 2.0f;
    for (int i = 0; i < 2; ++i) {
        consts->DepthBufferUVToViewMul[i] = consts->NDCToViewMul[i] / ratio;
        consts->DepthBufferUVToViewAdd[i] = consts->NDCToViewAdd[i] - consts->NDCToViewMul[i] * border / ratio;
    }

    consts->EffectRadius = std::clamp(settings->radius, 0.0f, 100000.0f);
    consts->EffectShadowStrength = std::clamp(settings->shadowMultiplier * 4.3f, 0.0f, 10.0f);
    consts->EffectShadowPow = std::clamp(settings->shadowPower, 0.0f, 10.0f);
    consts->EffectShadowClamp = std::clamp(settings->shadowClamp, 0.0f, 1.0f);
    consts->EffectFadeOutMul = -1.0f / (settings->fadeOutTo - settings->fadeOutFrom);
    consts->EffectFadeOutAdd = settings->fadeOutFrom / (settings->fadeOutTo - settings->fadeOutFrom) + 1.0f;
    consts->EffectHorizonAngleThreshold = std::clamp(settings->horizonAngleThreshold, 0.0f, 1.0f);

    // 1.2 seems to be around the best trade off - 1.0 means on-screen radius will stop/slow growing when the camera is at 1.0 distance, so, depending on FOV, basically filling up most of the screen
    // This setting is viewspace-dependent and not screen size dependent intentionally, so that when you change FOV the effect stays (relatively) similar.
    float effectSamplingRadiusNearLimit = (settings->radius * 1.2f);

    // if the depth precision is switched to 32bit float, this can be set to something closer to 1 (0.9999 is fine)
    consts->DepthPrecisionOffsetMod = 0.9992f;

    // Special settings for lowest quality level - just nerf the effect a tiny bit
    if (settings->qualityLevel <= FFX_Cacao::Quality::Low) {
        // consts.EffectShadowStrength     *= 0.9f;
        effectSamplingRadiusNearLimit *= 1.50f;

        if (settings->qualityLevel == FFX_Cacao::Quality::Lowest)
            consts->EffectRadius *= 0.8f;
    }

    effectSamplingRadiusNearLimit /= tanHalfFOVY; // to keep the effect same regardless of FOV

    consts->EffectSamplingRadiusNearLimitRec = 1.0f / effectSamplingRadiusNearLimit;

    consts->AdaptiveSampleCountLimit = settings->adaptiveQualityLimit;

    consts->NegRecEffectRadius = -1.0f / consts->EffectRadius;

    consts->InvSharpness = std::clamp(1.0f - settings->sharpness, 0.0f, 1.0f);

    consts->DetailAOStrength = settings->detailShadowStrength;

    // set buffer size constants.
    consts->SSAOBufferDimensions[0] = (float)bufferSizeInfo->ssaoBufferWidth;
    consts->SSAOBufferDimensions[1] = (float)bufferSizeInfo->ssaoBufferHeight;
    consts->SSAOBufferInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->ssaoBufferWidth);
    consts->SSAOBufferInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->ssaoBufferHeight);

    consts->DepthBufferDimensions[0] = (float)bufferSizeInfo->depthBufferWidth;
    consts->DepthBufferDimensions[1] = (float)bufferSizeInfo->depthBufferHeight;
    consts->DepthBufferInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->depthBufferWidth);
    consts->DepthBufferInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->depthBufferHeight);

    consts->DepthBufferOffset[0] = bufferSizeInfo->depthBufferXOffset;
    consts->DepthBufferOffset[1] = bufferSizeInfo->depthBufferYOffset;

    consts->InputOutputBufferDimensions[0] = (float)bufferSizeInfo->inputOutputBufferWidth;
    consts->InputOutputBufferDimensions[1] = (float)bufferSizeInfo->inputOutputBufferHeight;
    consts->InputOutputBufferInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->inputOutputBufferWidth);
    consts->InputOutputBufferInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->inputOutputBufferHeight);

    consts->ImportanceMapDimensions[0] = (float)bufferSizeInfo->importanceMapWidth;
    consts->ImportanceMapDimensions[1] = (float)bufferSizeInfo->importanceMapHeight;
    consts->ImportanceMapInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->importanceMapWidth);
    consts->ImportanceMapInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->importanceMapHeight);

    consts->DeinterleavedDepthBufferDimensions[0] = (float)bufferSizeInfo->deinterleavedDepthBufferWidth;
    consts->DeinterleavedDepthBufferDimensions[1] = (float)bufferSizeInfo->deinterleavedDepthBufferHeight;
    consts->DeinterleavedDepthBufferInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->deinterleavedDepthBufferWidth);
    consts->DeinterleavedDepthBufferInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->deinterleavedDepthBufferHeight);

    consts->DeinterleavedDepthBufferOffset[0] = (float)bufferSizeInfo->deinterleavedDepthBufferXOffset;
    consts->DeinterleavedDepthBufferOffset[1] = (float)bufferSizeInfo->deinterleavedDepthBufferYOffset;
    consts->DeinterleavedDepthBufferNormalisedOffset[0] = ((float)bufferSizeInfo->deinterleavedDepthBufferXOffset) / ((float)bufferSizeInfo->deinterleavedDepthBufferWidth);
    consts->DeinterleavedDepthBufferNormalisedOffset[1] = ((float)bufferSizeInfo->deinterleavedDepthBufferYOffset) / ((float)bufferSizeInfo->deinterleavedDepthBufferHeight);

    if (!settings->generateNormals) {
        consts->NormalsUnpackMul = 2.0f; // inputs->NormalsUnpackMul;
        consts->NormalsUnpackAdd = -1.0f; // inputs->NormalsUnpackAdd;
    } else {
        consts->NormalsUnpackMul = 2.0f;
        consts->NormalsUnpackAdd = -1.0f;
    }
}

void updateUBOPerPassConstants(UBOConstants *consts, const FFX_Cacao::Settings *settings, const FFX_Cacao::BufferSizeInfo *bufferSizeInfo, int pass)
{
    consts->PerPassFullResUVOffset[0] = ((float)(pass % 2)) / (float)bufferSizeInfo->ssaoBufferWidth;
    consts->PerPassFullResUVOffset[1] = ((float)(pass / 2)) / (float)bufferSizeInfo->ssaoBufferHeight;

    consts->PassIndex = pass;

    float additionalAngleOffset = settings->temporalSupersamplingAngleOffset; // if using temporal supersampling approach (like "Progressive Rendering Using Multi-frame Sampling" from GPU Pro 7, etc.)
    float additionalRadiusScale = settings->temporalSupersamplingRadiusOffset; // if using temporal supersampling approach (like "Progressive Rendering Using Multi-frame Sampling" from GPU Pro 7, etc.)
    const int subPassCount = 5;
    for (int subPass = 0; subPass < subPassCount; subPass++) {
        int a = pass;
        int b = subPass;

        int spmap[5]{ 0, 1, 4, 3, 2 };
        b = spmap[subPass];

        float ca, sa;
        float angle0 = ((float)a + (float)b / (float)subPassCount) * (M_PI)*0.5f;

        ca = std::cos(angle0);
        sa = std::sin(angle0);

        float scale = 1.0f + (a - 1.5f + (b - (subPassCount - 1.0f) * 0.5f) / (float)subPassCount) * 0.07f;

        consts->PatternRotScaleMatrices[subPass][0] = scale * ca;
        consts->PatternRotScaleMatrices[subPass][1] = scale * -sa;
        consts->PatternRotScaleMatrices[subPass][2] = -scale * sa;
        consts->PatternRotScaleMatrices[subPass][3] = -scale * ca;
    }
}

void FFX_Cacao::updateScene(const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix)
{
    for (size_t i = 0; i < PassCount; ++i) {
        // Update UBO
        Buffer &ubo = m_ubos[i];
        void *data = ubo.map();

        const glm::mat4 viewNormalMatrix = glm::transpose(glm::inverse(viewMatrix));

        updateUBOConstants(reinterpret_cast<UBOConstants *>(data), &m_settings, &m_bufferSizeInfo, projectionMatrix, viewNormalMatrix);
        updateUBOPerPassConstants(reinterpret_cast<UBOConstants *>(data), &m_settings, &m_bufferSizeInfo, i);

        ubo.unmap();
    }
}

CommandBuffer FFX_Cacao::render(const KDGpu::TextureView &swapchainImageView)
{
    CommandRecorder commandRecorder = m_device->createCommandRecorder();

    // Launch compute shaders to generate AO map
    cacaoComputePass(commandRecorder);

    // Apply the AO map onto a fullscreen quad
    cacaoApplyFullScreen(commandRecorder, swapchainImageView);

    return commandRecorder.finish();
}

void FFX_Cacao::cacaoComputePass(CommandRecorder &commandRecorder)
{
    const bool useDownSampledSSAO = false;

    auto dispatchCompute = [&](const BindGroup &bindGroup, const ComputePipeline &pipeline, const ComputeCommand &command) {
        ComputePassCommandRecorder computePassRecorder = commandRecorder.beginComputePass(ComputePassCommandRecorderOptions{});
        computePassRecorder.setPipeline(pipeline);
        computePassRecorder.setBindGroup(0, bindGroup);
        computePassRecorder.dispatchCompute(command);
        computePassRecorder.end();
    };

    auto dispatchSize = [](uint32_t tileWidth, uint32_t tileHeight, uint32_t width, uint32_t height) -> ComputeCommand {
        return ComputeCommand{ (width + tileWidth - 1) / tileWidth, (height + tileHeight - 1) / tileHeight, 1 };
    };

    // Transition depth texture to shader read only layer
    commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
            .srcStages = PipelineStageFlagBit::AllGraphicsBit,
            .srcMask = AccessFlagBit::DepthStencilAttachmentWriteBit | AccessFlagBit::DepthStencilAttachmentReadBit,
            .dstStages = PipelineStageFlagBit::ComputeShaderBit,
            .dstMask = AccessFlagBit::ShaderReadBit,
            .oldLayout = m_oldDepthTextureLayout,
            .newLayout = TextureLayout::ShaderReadOnlyOptimal,
            .texture = m_depthHandle,
            .range = {
                    .aspectMask = TextureAspectFlagBits::DepthBit | TextureAspectFlagBits::StencilBit,
            },
    });

    // Transition to general layouts
    {
        const std::vector<Handle<Texture_t>> texs = {
            m_textures[static_cast<uint32_t>(TextureCACAO::DeinterleavedDepth)],
            m_textures[static_cast<uint32_t>(TextureCACAO::DeinterleavedNormals)],
            m_textures[static_cast<uint32_t>(TextureCACAO::SSAOBufferPing)],
            m_textures[static_cast<uint32_t>(TextureCACAO::SSAOBufferPong)],
            m_textures[static_cast<uint32_t>(TextureCACAO::ImportanceMap)],
            m_textures[static_cast<uint32_t>(TextureCACAO::ImportanceMapPong)],
            m_loadCounter,
        };
        for (const auto &t : texs)
            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::TopOfPipeBit,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderWriteBit,
                    .oldLayout = TextureLayout::Undefined,
                    .newLayout = TextureLayout::General,
                    .texture = t,
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });
    }

    // Prepare Pass
    {
        // ClearLoadCounter
        dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::ClearLoadCounter)],
                        m_pipelines[static_cast<uint32_t>(PipelineCACAO::ClearLoadCounterSPIRV32)],
                        { 1, 1, 1 });

        // Prepare DepthMips
        switch (m_settings.qualityLevel) {
        case Quality::Lowest:
            dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::PrepareDepth)],
                            useDownSampledSSAO
                                    ? m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareDownsampledDepthsHalfSPIRV32)]
                                    : m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareNativeDepthsHalfSPIRV32)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.deinterleavedDepthBufferWidth, m_bufferSizeInfo.deinterleavedDepthBufferHeight));
            break;

        case Quality::Low:
            dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::PrepareDepth)],
                            useDownSampledSSAO
                                    ? m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareDownsampledDepthsSPIRV32)]
                                    : m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareNativeDepthsSPIRV32)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.deinterleavedDepthBufferWidth, m_bufferSizeInfo.deinterleavedDepthBufferHeight));
            break;

        default:
            dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::PrepareDepthMips)],
                            useDownSampledSSAO
                                    ? m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareDownsampledDepthsAndMipsSPIRV32)]
                                    : m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareNativeDepthsAndMipsSPIRV32)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.deinterleavedDepthBufferWidth, m_bufferSizeInfo.deinterleavedDepthBufferHeight));
            break;
        }

        // Prepare Normal
        if (m_settings.generateNormals)
            dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::PrepareNormals)],
                            useDownSampledSSAO
                                    ? m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareDownsampledNormalsSPIRV32)]
                                    : m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareNativeNormalsSPIRV32)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.ssaoBufferWidth, m_bufferSizeInfo.ssaoBufferHeight));
        else
            dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::PrepareNormalsFromInputNormals)],
                            useDownSampledSSAO
                                    ? m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareDownsampledNormalsFromInputNormalsSPIRV32)]
                                    : m_pipelines[static_cast<uint32_t>(PipelineCACAO::PrepareNativeNormalsFromInputNormalsSPIRV32)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.ssaoBufferWidth, m_bufferSizeInfo.ssaoBufferHeight));
    }

    // Transition to shader read layouts
    {
        const std::vector<Handle<Texture_t>> texs = {
            m_textures[static_cast<uint32_t>(TextureCACAO::DeinterleavedDepth)],
            m_textures[static_cast<uint32_t>(TextureCACAO::DeinterleavedNormals)],
        };

        for (const auto &t : texs)
            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::ShaderWriteBit,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderReadBit,
                    .oldLayout = TextureLayout::General,
                    .newLayout = TextureLayout::ShaderReadOnlyOptimal,
                    .texture = t,
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });

        commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                .srcMask = AccessFlagBit::ShaderWriteBit,
                .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                .dstMask = AccessFlagBit::ShaderReadBit | AccessFlagBit::ShaderWriteBit,
                .oldLayout = TextureLayout::General,
                .newLayout = TextureLayout::General,
                .texture = m_loadCounter,
                .range = {
                        .aspectMask = TextureAspectFlagBits::ColorBit,
                },
        });
    }

    // Base Pass
    {
        // SSAO
        for (size_t i = 0; i < PassCount; ++i) {
            dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::GenerateAdaptiveBase0) + i],
                            m_pipelines[static_cast<uint32_t>(PipelineCACAO::GenerateQ3BaseSPIRV32)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.ssaoBufferWidth, m_bufferSizeInfo.ssaoBufferHeight));
        }

        commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                .srcMask = AccessFlagBit::ShaderWriteBit,
                .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                .dstMask = AccessFlagBit::ShaderReadBit,
                .oldLayout = TextureLayout::General,
                .newLayout = TextureLayout::ShaderReadOnlyOptimal,
                .texture = m_textures[static_cast<uint32_t>(TextureCACAO::SSAOBufferPong)],
                .range = {
                        .aspectMask = TextureAspectFlagBits::ColorBit,
                },
        });

        // Importance Map
        {
            dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::GenerateImportanceMap)],
                            m_pipelines[static_cast<uint32_t>(PipelineCACAO::GenerateImportanceMapSPIRV32)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.importanceMapWidth, m_bufferSizeInfo.importanceMapHeight));

            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::ShaderWriteBit,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderReadBit,
                    .oldLayout = TextureLayout::General,
                    .newLayout = TextureLayout::ShaderReadOnlyOptimal,
                    .texture = m_textures[static_cast<uint32_t>(TextureCACAO::ImportanceMap)],
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });

            dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::GenerateImportanceMapA)],
                            m_pipelines[static_cast<uint32_t>(PipelineCACAO::PostprocessImportanceMapASPIRV32)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.importanceMapWidth, m_bufferSizeInfo.importanceMapHeight));

            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::ShaderReadBit,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderWriteBit,
                    .oldLayout = TextureLayout::ShaderReadOnlyOptimal,
                    .newLayout = TextureLayout::General,
                    .texture = m_textures[static_cast<uint32_t>(TextureCACAO::ImportanceMap)],
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });
            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::ShaderWriteBit,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderReadBit,
                    .oldLayout = TextureLayout::General,
                    .newLayout = TextureLayout::ShaderReadOnlyOptimal,
                    .texture = m_textures[static_cast<uint32_t>(TextureCACAO::ImportanceMapPong)],
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });

            dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::GenerateImportanceMapB)],
                            m_pipelines[static_cast<uint32_t>(PipelineCACAO::PostprocessImportanceMapBSPIRV32)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.importanceMapWidth, m_bufferSizeInfo.importanceMapHeight));
        }

        commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                .srcMask = AccessFlagBit::ShaderWriteBit,
                .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                .dstMask = AccessFlagBit::ShaderReadBit,
                .oldLayout = TextureLayout::General,
                .newLayout = TextureLayout::ShaderReadOnlyOptimal,
                .texture = m_textures[static_cast<uint32_t>(TextureCACAO::ImportanceMap)],
                .range = {
                        .aspectMask = TextureAspectFlagBits::ColorBit,
                },
        });

        commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                .srcMask = AccessFlagBit::ShaderReadBit | AccessFlagBit::ShaderWriteBit,
                .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                .dstMask = AccessFlagBit::ShaderReadBit,
                .oldLayout = TextureLayout::General,
                .newLayout = TextureLayout::General,
                .texture = m_loadCounter,
                .range = {
                        .aspectMask = TextureAspectFlagBits::ColorBit,
                },
        });
    }

    // Main Pass
    {
        ComputeCommand cmd;

        switch (m_settings.qualityLevel) {
        case Quality::Lowest:
        case Quality::Low:
        case Quality::Medium:
            cmd = dispatchSize(4, 16, m_bufferSizeInfo.ssaoBufferWidth, m_bufferSizeInfo.ssaoBufferHeight);
            cmd.workGroupX = (cmd.workGroupX + 4) / 5;
            cmd.workGroupZ = 5;
            break;
        default:
            cmd = dispatchSize(tileSize, tileSize, m_bufferSizeInfo.ssaoBufferWidth, m_bufferSizeInfo.ssaoBufferHeight);
            break;
        };

        for (size_t i = 0; i < PassCount; ++i) {
            dispatchCompute(
                    m_settings.qualityLevel == Quality::Highest
                            ? m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::GenerateAdaptive0) + i]
                            : m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::Generate0) + i],
                    m_pipelines[static_cast<uint32_t>(PipelineCACAO::GenerateQ0SPIRV32) + std::max(0U, static_cast<uint32_t>(m_settings.qualityLevel) - 1)],
                    cmd);
        }
    }

    // Deinterleaved Blur Passes
    {
        const uint32_t blurPassCount = std::clamp(m_settings.blurPassCount, 0U, 8U);
        if (blurPassCount > 0) {

            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::ShaderWriteBit,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderReadBit,
                    .oldLayout = TextureLayout::General,
                    .newLayout = TextureLayout::ShaderReadOnlyOptimal,
                    .texture = m_textures[static_cast<uint32_t>(TextureCACAO::SSAOBufferPing)],
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });
            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::None,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderWriteBit,
                    .oldLayout = TextureLayout::Undefined,
                    .newLayout = TextureLayout::General,
                    .texture = m_textures[static_cast<uint32_t>(TextureCACAO::SSAOBufferPong)],
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });

            for (size_t i = 0; i < PassCount; ++i) {
                if (m_settings.qualityLevel == Quality::Lowest && (i == 1 || i == 2))
                    continue;

                dispatchCompute(m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::EdgeSensitiveBlur0) + i],
                                m_pipelines[static_cast<uint32_t>(PipelineCACAO::EdgeSensitiveBlur1SPIRV32) + blurPassCount - 1],
                                dispatchSize(4 * tileSize - 2 * blurPassCount,
                                             3 * tileSize - 2 * blurPassCount,
                                             m_bufferSizeInfo.ssaoBufferWidth,
                                             m_bufferSizeInfo.ssaoBufferHeight));
            }

            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::ShaderWriteBit,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderReadBit,
                    .oldLayout = TextureLayout::General,
                    .newLayout = TextureLayout::ShaderReadOnlyOptimal,
                    .texture = m_textures[static_cast<uint32_t>(TextureCACAO::SSAOBufferPong)],
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });
            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::None,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderWriteBit,
                    .oldLayout = TextureLayout::Undefined,
                    .newLayout = TextureLayout::General,
                    .texture = m_output,
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });

        } else {
            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::ShaderWriteBit,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderReadBit,
                    .oldLayout = TextureLayout::General,
                    .newLayout = TextureLayout::ShaderReadOnlyOptimal,
                    .texture = m_textures[static_cast<uint32_t>(TextureCACAO::SSAOBufferPing)],
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });
            commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                    .srcStages = PipelineStageFlagBit::ComputeShaderBit,
                    .srcMask = AccessFlagBit::None,
                    .dstStages = PipelineStageFlagBit::ComputeShaderBit,
                    .dstMask = AccessFlagBit::ShaderWriteBit,
                    .oldLayout = TextureLayout::Undefined,
                    .newLayout = TextureLayout::General,
                    .texture = m_output,
                    .range = {
                            .aspectMask = TextureAspectFlagBits::ColorBit,
                    },
            });
        }

        // Apply or Downsample Pass
        if (useDownSampledSSAO) {

            PipelineCACAO pipeline = PipelineCACAO::UpscaleBilateral5x5SmartSPIRV32;

            switch (m_settings.qualityLevel) {
            case Quality::Lowest:
                pipeline = PipelineCACAO::UpscaleBilateral5x5HalfSPIRV32;
                break;
            case Quality::Low:
            case Quality::Medium:
                pipeline = PipelineCACAO::UpscaleBilateral5x5NonSmartSPIRV32;
                break;
            default:
                break;
            }

            dispatchCompute(blurPassCount
                                    ? m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::BilateralUpsamplePong)]
                                    : m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::BilateralUpsamplePing)],
                            m_pipelines[static_cast<uint32_t>(pipeline)],
                            dispatchSize(2 * tileSize, 2 * tileSize, m_bufferSizeInfo.inputOutputBufferWidth, m_bufferSizeInfo.inputOutputBufferHeight));
        } else {

            PipelineCACAO pipeline = PipelineCACAO::ApplySPIRV32;

            switch (m_settings.qualityLevel) {
            case Quality::Lowest:
                pipeline = PipelineCACAO::NonSmartHalfApplySPIRV32;
                break;
            case Quality::Low:
                pipeline = PipelineCACAO::NonSmartApplySPIRV32;
                break;
            default:
                break;
            }

            dispatchCompute(blurPassCount
                                    ? m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::ApplyPong)]
                                    : m_bindGroups[static_cast<uint32_t>(BindGroupCACAO::ApplyPing)],
                            m_pipelines[static_cast<uint32_t>(pipeline)],
                            dispatchSize(tileSize, tileSize, m_bufferSizeInfo.inputOutputBufferWidth, m_bufferSizeInfo.inputOutputBufferHeight));
        }
    }

    // Transition output texture to be used by fragment shader
    commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
            .srcStages = PipelineStageFlagBit::ComputeShaderBit,
            .srcMask = AccessFlagBit::ShaderWriteBit,
            .dstStages = PipelineStageFlagBit::AllGraphicsBit,
            .dstMask = AccessFlagBit::ShaderReadBit,
            .oldLayout = TextureLayout::General,
            .newLayout = TextureLayout::ShaderReadOnlyOptimal,
            .texture = m_output,
            .range = {
                    .aspectMask = TextureAspectFlagBits::ColorBit,
            },
    });

    // Transition depth texture back to DepthStencilAttachment Optimal
    commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
            .srcStages = PipelineStageFlagBit::ComputeShaderBit,
            .srcMask = AccessFlagBit::ShaderReadBit,
            .dstStages = PipelineStageFlagBit::AllGraphicsBit,
            .dstMask = AccessFlagBit::DepthStencilAttachmentWriteBit | AccessFlagBit::DepthStencilAttachmentReadBit,
            .oldLayout = TextureLayout::ShaderReadOnlyOptimal,
            .newLayout = TextureLayout::DepthStencilAttachmentOptimal,
            .texture = m_depthHandle,
            .range = {
                    .aspectMask = TextureAspectFlagBits::DepthBit | TextureAspectFlagBits::StencilBit,
            },
    });

    m_oldDepthTextureLayout = TextureLayout::DepthStencilAttachmentOptimal;
}

void FFX_Cacao::cacaoApplyFullScreen(CommandRecorder &commandRecorder, const KDGpu::TextureView &swapchainImageView)
{
    m_fsqPassOptions.colorAttachments[0].view = swapchainImageView;
    auto fsqPass = commandRecorder.beginRenderPass(m_fsqPassOptions);
    fsqPass.setPipeline(m_fsqPipeline);
    fsqPass.setBindGroup(0, m_fsqTextureBindGroup);
    fsqPass.draw(DrawCommand{ .vertexCount = 6 });

    fsqPass.end();
}
