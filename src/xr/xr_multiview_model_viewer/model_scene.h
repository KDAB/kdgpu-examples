/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include "pipeline_key.h"
#include "primitive_key.h"

#include <tinygltf_helper/camera.h>

#include <KDGpuExample/engine.h>
#include <KDGpuExample/xr_compositor/xr_projection_layer.h>

#include <KDGpu/bind_group.h>
#include <KDGpu/bind_group_layout.h>
#include <KDGpu/buffer.h>
#include <KDGpu/graphics_pipeline.h>
#include <KDGpu/pipeline_layout.h>
#include <KDGpu/render_pass_command_recorder_options.h>
#include <KDGpu/sampler.h>
#include <KDGpu/shader_module.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>

#include <kdbindings/property.h>

#include <glm/glm.hpp>

#include <unordered_map>

namespace tinygltf {
class Model;
struct BufferView;
struct Node;
struct Primitive;
} // namespace tinygltf

struct BufferAndOffset {
    Handle<Buffer_t> buffer;
    DeviceSize offset{ 0 };
};

struct IndexedDraw {
    Handle<Buffer_t> indexBuffer;
    KDGpu::DeviceSize offset{ 0 };
    uint32_t indexCount{ 0 };
    KDGpu::IndexType indexType{ KDGpu::IndexType::Uint32 };
};

// We now store which world transform should be copied into the SSBO instead of a bindGroup
struct InstanceData {
    uint32_t worldTransformIndex;
};

struct InstancedDraw {
    uint32_t firstInstance{ 0 };
    uint32_t instanceCount{ 1 };
};

struct PrimitiveInstances {
    std::unordered_map<PrimitiveKey, std::vector<InstanceData>> instanceData;
    uint32_t totalInstanceCount{ 0 };

    // Data used to populate the buffer contents
    glm::mat4 *mappedData{ nullptr }; // Mapped data point from SSBO
    uint32_t offset{ 0 }; // Where to copy the next set of matrices
};

struct PrimitiveData {
    InstancedDraw instances;
    std::vector<BufferAndOffset> vertexBuffers;

    enum class DrawType {
        NonIndexed = 0,
        Indexed = 1
    };
    DrawType drawType{ DrawType::NonIndexed };
    union {
        uint32_t vertexCount;
        IndexedDraw indexedDraw;
    } drawData;
};

struct TextureAndView {
    Texture texture;
    TextureView textureView;
};

struct TextureViewAndSampler {
    Handle<TextureView_t> view;
    Handle<Sampler_t> sampler;
};

struct MaterialData {
    glm::vec4 baseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
    float alphaCutoff{ 0.5f };
};

struct MaterialPrimitives {
    Handle<BindGroup_t> material;
    std::vector<PrimitiveData> primitives;
};

struct RenderStats {
    uint32_t pipelineCount{ 0 };
    uint32_t setPipelineCount{ 0 };
    uint32_t setVertexBufferCount{ 0 };
    uint32_t setBindGroupCount{ 0 };
    uint32_t drawCount{ 0 };
    uint32_t vertexCount{ 0 };
};

class ModelScene : public KDGpuExample::XrProjectionLayer
{
public:
    KDBindings::Property<glm::vec3> translation{ glm::vec3(0.0f, 0.0f, -1.0f) };
    KDBindings::Property<float> scale{ 1.0f };
    KDBindings::Property<KDXr::Pose> leftPalmPose{ KDXr::Pose{} };
    KDBindings::Property<KDXr::Pose> rightPalmPose{ KDXr::Pose{} };

    ModelScene(const KDGpuExample::XrProjectionLayerOptions &options);

    enum class Hand : std::uint8_t {
        Left = 0,
        Right
    };
    void toggleRay(Hand hand);

protected:
    void initializeScene();
    void initializeHands();
    void initializeRay();
    void cleanupScene();
    void initialize() override;
    void cleanup() override;
    void updateScene() override;
    void renderView() override;

private:
    Buffer createBufferForBufferView(const tinygltf::Model &model,
                                     const std::vector<KDGpu::BufferUsageFlags> &bufferViewUsages,
                                     uint32_t bufferViewIndex);

    TextureAndView createTextureForImage(const tinygltf::Model &model, uint32_t textureIndex);
    Sampler createSampler(const tinygltf::Model &model, uint32_t samplerIndex);
    TextureViewAndSampler createViewSamplerForTexture(const tinygltf::Model &model, uint32_t viewSamplerIndex);
    TextureAndView createSolidColorTexture(float r, float g, float b, float a);

    void setupMaterial(const tinygltf::Model &model, uint32_t materialIndex);

    void setupPrimitive(const tinygltf::Model &model,
                        const tinygltf::Primitive &primitive,
                        const PrimitiveKey &primitiveKey,
                        PrimitiveInstances &primitiveInstances);

    Handle<ShaderModule_t> findOrCreateShaderModule(const ShaderModuleKey &key);
    std::string shaderFilename(const PipelineShaderOptionsKey &key, ShaderStageFlagBits stage);

    InstancedDraw setupPrimitiveInstances(const PrimitiveKey &primitiveKey,
                                          PrimitiveInstances &primitiveInstances);

    void calculateWorldTransforms(const tinygltf::Model &model);

    void setupMeshNode(const tinygltf::Model &model,
                       const tinygltf::Node &node,
                       uint32_t nodeIndex,
                       PrimitiveInstances &primitiveInstances);

    void updateTransformUbo();
    void updateViewUbo();

    struct CameraData {
        glm::mat4 view;
        glm::mat4 projection;
    };

    std::vector<CameraData> m_cameraData{ 2 }; // Default to 2 views
    Buffer m_cameraBuffer;
    BindGroup m_cameraBindGroup;

    Texture m_msaaTexture;
    TextureView m_msaaTextureView;

    TextureAndView m_opaqueWhite;
    TextureAndView m_transparentBlack;
    TextureAndView m_defaultNormal;
    Sampler m_defaultSampler;

    std::unordered_map<Handle<GraphicsPipeline_t>, std::vector<MaterialPrimitives>> m_pipelinePrimitiveMap;

    std::vector<Buffer> m_buffers;
    std::vector<TextureAndView> m_textures;
    std::vector<Sampler> m_samplers;
    std::vector<TextureViewAndSampler> m_viewSamplers;

    std::vector<glm::mat4> m_worldTransforms; // Indexed as per model.nodes
    Buffer m_instanceTransformsBuffer;
    BindGroup m_instanceTransformsBindGroup;

    const PushConstantRange m_modelTransformPushConstantRange{
        .offset = 0,
        .size = sizeof(glm::mat4),
        .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit)
    };
    glm::mat4 m_modelTransform{ 1.0f };

    std::vector<Buffer> m_materialBuffers; // Indexed as per model.materials
    std::vector<BindGroup> m_materialBindGroups;

    std::unordered_map<GraphicsPipelineKey, GraphicsPipeline> m_pipelines;
    std::unordered_map<ShaderModuleKey, ShaderModule> m_shaderModules;
    BindGroupLayout m_nodeBindGroupLayout;
    BindGroupLayout m_cameraBindGroupLayout;
    BindGroupLayout m_materialBindGroupLayout;
    PipelineLayout m_pipelineLayout;
    RenderPassCommandRecorderOptions m_opaquePassOptions;
    CommandBuffer m_commandBuffer;

    RenderStats m_renderStats;

    Fence m_fence;

    // Hand related resources

    PipelineLayout m_handPipelineLayout;
    GraphicsPipeline m_handPipeline;
    BindGroupLayout m_solidTransformBindGroupLayout;

    Buffer m_leftHandBuffer;
    Buffer m_rightHandBuffer;

    glm::mat4 m_leftHandTransform;
    Buffer m_leftHandTransformBuffer;
    BindGroup m_leftHandTransformBindGroup;

    glm::mat4 m_rightHandTransform;
    Buffer m_rightHandTransformBuffer;
    BindGroup m_rightHandTransformBindGroup;

    // ray related resources

    Buffer m_rayVertexBuffer;
    Buffer m_rayIndexBuffer;

    glm::mat4 m_leftRayTransform;
    Buffer m_leftRayTransformBuffer;
    BindGroup m_leftRayTransformBindGroup;

    glm::mat4 m_rightRayTransform;
    Buffer m_rightRayTransformBuffer;
    BindGroup m_rightRayTransformBindGroup;

    std::array<bool, 2> m_rayHands{ false, false };
    struct AnimationData {
        TimePoint startTime;
        uint32_t durationMs{ 1000 };
        bool animating{ false };
    };
    std::array<AnimationData, 2> m_rayAnimationData;
};
