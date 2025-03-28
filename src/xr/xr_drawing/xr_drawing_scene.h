/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include "drawing_model.h"
#include "geometric_primitives.h"

#include <KDGpuExample/xr_compositor/xr_projection_layer.h>
#include <KDXr/kdxr_core.h>

#include <KDGpu/bind_group.h>
#include <KDGpu/buffer.h>
#include <KDGpu/command_buffer.h>
#include <KDGpu/graphics_pipeline.h>
#include <KDGpu/pipeline_layout.h>
#include <KDGpu/render_pass_command_recorder_options.h>

#include <kdbindings/property.h>

#include <glm/glm.hpp>

using namespace KDGpuExample;
using namespace KDGpu;

// A projection layer that draws a 3D scene in XR
// and a hand that can be used to draw shapes in the scene
class XrDrawingScene : public XrProjectionLayer
{
public:
    KDBindings::Property<glm::vec3> handScale{ glm::vec3(1.0f, 1.0f, 1.0f) };
    KDBindings::Property<KDXr::Pose> leftAimPose{ KDXr::Pose{} };
    KDBindings::Property<KDXr::Pose> rightAimPose{ KDXr::Pose{} };
    KDBindings::Property<glm::vec3> shapeColor{ glm::vec3(1.0f, 1.0f, 1.0f) };
    KDBindings::Property<ShapeType> shape{ ShapeType::Cube };
    KDBindings::Property<glm::vec3> modelOffset{ glm::vec3(0.0f, 0.0f, 0.0f) };

    explicit XrDrawingScene(const XrProjectionLayerOptions &options);
    ~XrDrawingScene() override;

    // Not copyable
    XrDrawingScene(const XrDrawingScene &) = delete;
    XrDrawingScene &operator=(const XrDrawingScene &) = delete;

    // Moveable
    XrDrawingScene(XrDrawingScene &&) = default;
    XrDrawingScene &operator=(XrDrawingScene &&) = default;

    void addShape(const DrawnShape &shape);
    void removeLastShape();

    void load(const std::string &path);
    void save(const std::string &path) const;
    void clear();

protected:
    void initialize() override;
    void cleanup() override;
    void updateScene() override;
    void renderView() override;

private:
    void initializeScene();
    void cleanupScene();

    void updateModelOffset();
    void updateTransformUbo();
    void updateViewUbo();
    void updateModelInstances();

    // Camera matrix data
    struct CameraData {
        glm::mat4 view;
        glm::mat4 projection;
    };
    std::vector<CameraData> m_cameraData{ 2 }; // Default to 2 views
    Buffer m_cameraBuffer;
    BindGroup m_cameraBindGroup;

    // Data required for rendering a geometric primitive
    struct BufferData {
        KDGpu::Buffer vertexBuffer;
        KDGpu::Buffer indexBuffer;
        uint32_t numIndices = 0;
    };

    // Generate the vertex and index buffers for a given shape
    BufferData generateBuffersFromShape(const ShapeData &drawingModel);

    // per-shape data
    std::unordered_map<ShapeType, BufferData> m_shapeBuffers;
    std::unordered_map<ShapeType, uint32_t> m_shapeCounts;

    // Per-instance data
    Buffer m_instanceTransformsBuffer;
    BindGroup m_instanceTransformsBindGroup;
    size_t m_instanceTransformsCount{ 0 };

    // Data for instance used as drawing hand
    glm::mat4 m_drawingHandTransform;
    Buffer m_drawHandTransformBuffer;
    BindGroup m_drawHandTransformBindGroup;

    // Bind group layouts
    BindGroupLayout m_instanceDataBindGroupLayout;
    BindGroupLayout m_cameraBindGroupLayout;
    BindGroupLayout m_modelTransformBindGroupLayout;

    // Pipeline and render pass data
    PipelineLayout m_pipelineLayout;
    GraphicsPipeline m_pipeline;
    RenderPassCommandRecorderOptions m_opaquePassOptions;
    CommandBuffer m_commandBuffer;

    // Transform for moving everything in the drawn scene
    glm::mat4 m_modelBaseTransform;
    Buffer m_modelBaseTransformBuffer;
    BindGroup m_modelBaseTransformBindGroup;

    // Identity matrix used as substitute for modelBaseTransform
    Buffer m_identityTransformBuffer;
    BindGroup m_identityTransformBindGroup;

    // The collection of drawn shapes
    DrawingModel m_drawingModel;

    // Fence for synchronization
    Fence m_fence;

    // Binding for updating due to model offset changes
    KDBindings::ConnectionHandle m_modelOffsetConnection;

    // Indication that the instance data needs to be updated due to model changes
    bool m_modelUpdated = false;
};
