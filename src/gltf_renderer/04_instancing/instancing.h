/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include "pipeline_key.h"
#include "primitive_key.h"

#include <camera/camera.h>

#include <KDGpuExample/simple_example_engine_layer.h>

#include <KDGpu/bind_group.h>
#include <KDGpu/bind_group_layout.h>
#include <KDGpu/buffer.h>
#include <KDGpu/graphics_pipeline.h>
#include <KDGpu/render_pass_command_recorder_options.h>
#include <KDGpu/shader_module.h>

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

struct InstanceBindGroupAndCount {
    Handle<BindGroup_t> instanceBindGroup;
    uint32_t instanceCount{ 1 };
};

using PrimitiveInstances = std::unordered_map<PrimitiveKey, std::vector<InstanceData>>;

struct PrimitiveData {
    InstanceBindGroupAndCount instances;
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

struct RenderStats {
    uint32_t pipelineCount{ 0 };
    uint32_t setPipelineCount{ 0 };
    uint32_t setVertexBufferCount{ 0 };
    uint32_t setBindGroupCount{ 0 };
    uint32_t drawCount{ 0 };
};

class Instancing : public KDGpuExample::SimpleExampleEngineLayer
{
public:
    kdgpu_ext::graphics::camera::Camera *camera() { return &m_camera; }

protected:
    void initializeScene() override;
    void cleanupScene() override;
    void resize() override;
    void updateScene() override;
    void render() override;

private:
    Buffer createBufferForBufferView(const tinygltf::Model &model,
                                     const std::vector<KDGpu::BufferUsageFlags> &bufferViewUsages,
                                     uint32_t bufferViewIndex);

    void setupPrimitive(const tinygltf::Model &model,
                        const tinygltf::Primitive &primitive,
                        const PrimitiveKey &primitiveKey,
                        const PrimitiveInstances &primitiveInstances);

    InstanceBindGroupAndCount setupPrimitiveInstances(const PrimitiveKey &primitiveKey,
                                                      const PrimitiveInstances &primitiveInstances);

    void calculateWorldTransforms(const tinygltf::Model &model);

    void setupMeshNode(const tinygltf::Model &model,
                       const tinygltf::Node &node,
                       uint32_t nodeIndex,
                       PrimitiveInstances &primitiveInstances);

    kdgpu_ext::graphics::camera::Camera m_camera;
    Buffer m_cameraBuffer;
    BindGroup m_cameraBindGroup;

    // We now store a map of pipeline to the primitives that use that pipeline. Each
    // primitive may have multiple instances by way of the list of transform bind groups.
    std::unordered_map<Handle<GraphicsPipeline_t>, std::vector<PrimitiveData>> m_pipelinePrimitiveMap;

    std::vector<Buffer> m_buffers;

    std::vector<glm::mat4> m_worldTransforms; // Indexed as per model.nodes
    std::vector<Buffer> m_worldTransformBuffers;
    std::vector<BindGroup> m_worldTransformBindGroups;

    std::unordered_map<GraphicsPipelineKey, GraphicsPipeline> m_pipelines;
    ShaderModule m_vertexShader;
    ShaderModule m_fragmentShader;
    BindGroupLayout m_nodeBindGroupLayout;
    BindGroupLayout m_cameraBindGroupLayout;
    PipelineLayout m_pipelineLayout;
    RenderPassCommandRecorderOptions m_opaquePassOptions;
    CommandBuffer m_commandBuffer;

    RenderStats m_renderStats;
};
