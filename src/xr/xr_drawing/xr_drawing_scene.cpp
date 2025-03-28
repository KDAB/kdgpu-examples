/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "xr_drawing_scene.h"

#include <KDGpuExample/engine.h>
#include <KDGpuExample/kdgpuexample.h>
#include <KDGpuExample/view_projection.h>
#include <KDGpuExample/xr_example_engine_layer.h>

#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/graphics_pipeline_options.h>

#include <KDGui/gui_application.h>
#include <KDUtils/dir.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <array>
#include <vector>
#include <cmath>

namespace {

struct InstanceData {
    glm::mat4 transform;
    glm::vec4 color;
};

} // namespace

XrDrawingScene::XrDrawingScene(const XrProjectionLayerOptions &options)
    : XrProjectionLayer(options)
{
    m_modelOffsetConnection = modelOffset.valueChanged().connect([this]() { updateModelOffset(); });
}

XrDrawingScene::~XrDrawingScene()
{
}

XrDrawingScene::BufferData XrDrawingScene::generateBuffersFromShape(const ShapeData &shapeData)
{
    BufferData bufferData;

    // Create a buffer to hold vertex data
    {
        const DeviceSize dataByteSize = shapeData.vertices.size() * sizeof(ShapeVertex);
        const BufferOptions bufferOptions = {
            .label = "Main Triangle ShapeVertex Buffer",
            .size = dataByteSize,
            .usage = BufferUsageFlagBits::VertexBufferBit | BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = MemoryUsage::GpuOnly
        };
        bufferData.vertexBuffer = m_device->createBuffer(bufferOptions);
        const BufferUploadOptions uploadOptions = {
            .destinationBuffer = bufferData.vertexBuffer,
            .dstStages = PipelineStageFlagBit::VertexAttributeInputBit,
            .dstMask = AccessFlagBit::VertexAttributeReadBit,
            .data = shapeData.vertices.data(),
            .byteSize = dataByteSize
        };
        uploadBufferData(uploadOptions);
    }

    // Create a buffer to hold the geometry index data
    {
        const DeviceSize dataByteSize = shapeData.indices.size() * sizeof(uint32_t);
        const BufferOptions bufferOptions = {
            .label = "Index Buffer",
            .size = dataByteSize,
            .usage = BufferUsageFlagBits::IndexBufferBit | BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = MemoryUsage::GpuOnly
        };
        bufferData.indexBuffer = m_device->createBuffer(bufferOptions);
        const BufferUploadOptions uploadOptions = {
            .destinationBuffer = bufferData.indexBuffer,
            .dstStages = PipelineStageFlagBit::IndexInputBit,
            .dstMask = AccessFlagBit::IndexReadBit,
            .data = shapeData.indices.data(),
            .byteSize = dataByteSize
        };
        uploadBufferData(uploadOptions);
    }

    // Make a note of the number of indices we will need to specify at draw time
    bufferData.numIndices = static_cast<uint32_t>(shapeData.indices.size());

    return bufferData;
}

void XrDrawingScene::initializeScene()
{
    auto app = KDGui::GuiApplication::instance();
    auto appdataDir = app->standardDir(KDFoundation::StandardDir::ApplicationDataLocal);

    // Create a buffer to hold the camera view and projection matrices
    {
        const BufferOptions bufferOptions = {
            .label = "Camera Buffer",
            .size = sizeof(glm::mat4) * 2,
            .usage = BufferUsageFlagBits::UniformBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
        };
        m_cameraBuffer = m_device->createBuffer(bufferOptions);

        // Upload identity matrices. Updated below in updateScene()
        glm::mat4 viewMatrix(1.0f);
        glm::mat4 projectionMatrix(1.0f);
        auto bufferData = static_cast<float *>(m_cameraBuffer.map());
        std::memcpy(bufferData, glm::value_ptr(viewMatrix), sizeof(glm::mat4));
        std::memcpy(bufferData + 16, glm::value_ptr(projectionMatrix), sizeof(glm::mat4));
        m_cameraBuffer.unmap();
    }

    // Create bind group layout consisting of a single binding holding a UBO for the camera view and projection matrices
    // clang-format off
    const BindGroupLayoutOptions cameraBindGroupLayoutOptions = {
        .label = "Camera Transform Bind Group",
        .bindings = {{
            .binding = 0,
            .resourceType = ResourceBindingType::UniformBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit)
        }}
    };
    // clang-format on
    m_cameraBindGroupLayout = m_device->createBindGroupLayout(cameraBindGroupLayoutOptions);

    // Create a buffer to hold the model base transformation matrix
    {
        const BufferOptions bufferOptions = {
            .label = "Transformation Buffer",
            .size = sizeof(glm::mat4),
            .usage = BufferUsageFlagBits::UniformBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
        };
        m_modelBaseTransformBuffer = m_device->createBuffer(bufferOptions);
        m_identityTransformBuffer = m_device->createBuffer(bufferOptions);

        // Upload identity matrix. Updated below in updateScene()
        auto identity = glm::mat4(1.0f);
        auto bufferData = m_modelBaseTransformBuffer.map();
        std::memcpy(bufferData, &identity, sizeof(glm::mat4));
        m_modelBaseTransformBuffer.unmap();

        // Upload identity matrix, to be used in place of modelBaseTransform
        bufferData = m_identityTransformBuffer.map();
        std::memcpy(bufferData, &identity, sizeof(glm::mat4));
        m_identityTransformBuffer.unmap();
    }

    // Create data for the shape instances in the model
    {
        // clang-format off
        const BindGroupLayoutOptions instanceDataBindGroupLayoutOptions = {
            .bindings = {{
                .binding = 0,
                .resourceType = ResourceBindingType::StorageBuffer,
                .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit)
            }}
        };
        // clang-format on
        m_instanceDataBindGroupLayout = m_device->createBindGroupLayout(instanceDataBindGroupLayoutOptions);

        m_shapeBuffers[ShapeType::Sphere] = generateBuffersFromShape(generateSphere());
        m_shapeCounts[ShapeType::Sphere] = 0;

        m_shapeBuffers[ShapeType::Cube] = generateBuffersFromShape(generateCube());
        m_shapeCounts[ShapeType::Cube] = 0;

        m_shapeBuffers[ShapeType::Cone] = generateBuffersFromShape(generateCone());
        m_shapeCounts[ShapeType::Cone] = 0;
    }

    // Create a single instance data for the drawing hand
    {
        BufferOptions bufferOptions = {
            .size = static_cast<DeviceSize>(sizeof(InstanceData)),
            .usage = BufferUsageFlags(BufferUsageFlagBits::StorageBufferBit),
            .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
        };
        m_drawHandTransformBuffer = m_device->createBuffer(bufferOptions);

        // Create a bind group for the instance transform storage buffer
        // clang-format off
        BindGroupOptions bindGroupOptions = {
            .layout = m_instanceDataBindGroupLayout,
            .resources = {{
                .binding = 0,
                .resource = StorageBufferBinding{ .buffer = m_drawHandTransformBuffer}
            }}
        };
        // clang-format on
        m_drawHandTransformBindGroup = m_device->createBindGroup(bindGroupOptions);
    }

    // Create a bindGroup to hold the UBO with the camera view and projection matrices
    // clang-format off
    const BindGroupOptions cameraBindGroupOptions = {
        .label = "Camera Bind Group",
        .layout = m_cameraBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_cameraBuffer }
        }}
    };
    // clang-format on
    m_cameraBindGroup = m_device->createBindGroup(cameraBindGroupOptions);

    // Create bind group layout consisting of a single binding holding a UBO for a whole-model transform
    // clang-format off
    const BindGroupLayoutOptions modelTransformBindGroupLayoutOptions = {
        .label = "Model Transform Bind Group",
        .bindings = {{
            .binding = 0,
            .resourceType = ResourceBindingType::UniformBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit)
        }}
    };
    // clang-format on
    m_modelTransformBindGroupLayout = m_device->createBindGroupLayout(modelTransformBindGroupLayoutOptions);

    // Create a bindGroup to hold the UBO with the model base transform
    // clang-format off
    const BindGroupOptions modelTransformBindGroupOptions = {
        .label = "Model Base Transform Bind Group",
        .layout = m_modelTransformBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_modelBaseTransformBuffer }
        }}
    };
    // clang-format on
    m_modelBaseTransformBindGroup = m_device->createBindGroup(modelTransformBindGroupOptions);

    // Create a bindGroup to hold the UBO with the model base transform
    // clang-format off
    const BindGroupOptions identityTransformBindGroupOptions = {
        .label = "Identity Transform Bind Group",
        .layout = m_modelTransformBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_identityTransformBuffer }
        }}
    };
    // clang-format on
    m_identityTransformBindGroup = m_device->createBindGroup(identityTransformBindGroupOptions);

    // Load the shaders for drawing the instanced shapes
    auto vertexShaderPath = KDGpuExample::assetDir().file("shaders/xr_drawing/xr_drawing.vert.spv");
    auto vertexShader = m_device->createShaderModule(KDGpuExample::readShaderFile(vertexShaderPath));

    auto fragmentShaderPath = KDGpuExample::assetDir().file("shaders/xr_drawing/xr_drawing.frag.spv");
    auto fragmentShader = m_device->createShaderModule(KDGpuExample::readShaderFile(fragmentShaderPath));

    // Create a pipeline layout (array of bind group layouts)
    const PipelineLayoutOptions pipelineLayoutOptions = { .bindGroupLayouts = { m_cameraBindGroupLayout,
                                                                                m_instanceDataBindGroupLayout,
                                                                                m_modelTransformBindGroupLayout } };
    m_pipelineLayout = m_device->createPipelineLayout(pipelineLayoutOptions);

    // Create a pipeline
    // clang-format off
    const GraphicsPipelineOptions pipelineOptions = {
        .label = "Shapes",
        .shaderStages = {
            { .shaderModule = vertexShader, .stage = ShaderStageFlagBits::VertexBit },
            { .shaderModule = fragmentShader, .stage = ShaderStageFlagBits::FragmentBit }
        },
        .layout = m_pipelineLayout,
        .vertex = {
            .buffers = {
                { .binding = 0, .stride = sizeof(ShapeVertex) }
            },
            .attributes = {
                { .location = 0, .binding = 0, .format = Format::R32G32B32_SFLOAT }, // Position
                { .location = 1, .binding = 0, .format = Format::R32G32B32_SFLOAT, .offset = offsetof(ShapeVertex, normal) }, // Normal
            }
        },
        .renderTargets = {
            { .format = m_colorSwapchainFormat }
        },
        .depthStencil = {
            .format = m_depthSwapchainFormat,
            .depthWritesEnabled = true,
            .depthCompareOperation = CompareOperation::Less
        },
        .primitive = {
            .cullMode = CullModeFlagBits::None
        }
    };
    // clang-format on
    m_pipeline = m_device->createGraphicsPipeline(pipelineOptions);

    // Most of the render pass is the same between frames. The only thing that changes, is which image
    // of the swapchain we wish to render to. So set up what we can here, and in the render loop we will
    // just update the color texture view.
    // clang-format off
    m_opaquePassOptions = {
        .colorAttachments = {
            {
                .view = {}, // Not setting the swapchain texture view just yet
                .clearValue = { 0.3f, 0.3f, 0.3f, 0.0f },
                .finalLayout = TextureLayout::ColorAttachmentOptimal
            }
        },
        .depthStencilAttachment = {
            .view = {} // Not setting the depth texture view just yet
        }
    };
    // clang-format on

    // We will use a fence to synchronize CPU and GPU. When we render image for each view (eye), we
    // shall wait for the fence to be signaled before we update any shared resources such as a view
    // matrix UBO (not used yet). An alternative would be to index into an array of such matrices.
    m_fence = m_device->createFence({ .label = "Projection Layer Scene Fence" });
}

void XrDrawingScene::cleanupScene()
{
    m_fence = {};

    m_cameraBindGroup = {};
    m_cameraBuffer = {};

    m_pipeline = {};
    m_pipelineLayout = {};
    m_modelBaseTransformBindGroup = {};
    m_modelBaseTransformBuffer = {};
    m_identityTransformBindGroup = {};
    m_identityTransformBuffer = {};
    m_instanceTransformsBuffer = {};
    m_instanceTransformsBindGroup = {};
    m_drawHandTransformBuffer = {};
    m_drawHandTransformBindGroup = {};

    m_commandBuffer = {};
}

void XrDrawingScene::initialize()
{
    XrProjectionLayer::initialize();
    initializeScene();
}

void XrDrawingScene::cleanup()
{
    cleanupScene();
    XrProjectionLayer::cleanup();
}

// In this function we will update our local copy of the view matrices and transform
// data. Note that we do not update the UBOs here as the GPU may still be reading from
// them at this stage. We cannot be sure it is safe to update the GPU data until we
// have waited for the fence to be signaled in the renderView() function.
void XrDrawingScene::updateScene()
{
    // Update the camera data for each view
    m_cameraData.resize(m_viewState.viewCount());
    for (uint32_t viewIndex = 0; viewIndex < m_viewState.viewCount(); ++viewIndex) {
        const auto &view = m_viewState.views[viewIndex];
        const KDXr::Quaternion &orientation = view.pose.orientation;
        const KDXr::Vector3 &position = view.pose.position;

        // clang-format off
        m_cameraData[viewIndex].view = viewMatrix({
            .orientation = glm::quat(orientation.w, orientation.x, orientation.y, orientation.z),
            .position = glm::vec3(position.x, position.y, position.z)
        });
        m_cameraData[viewIndex].projection = perspective({
            .leftFieldOfView = m_viewState.views[viewIndex].fieldOfView.angleLeft,
            .rightFieldOfView = m_viewState.views[viewIndex].fieldOfView.angleRight,
            .upFieldOfView = m_viewState.views[viewIndex].fieldOfView.angleUp,
            .downFieldOfView = m_viewState.views[viewIndex].fieldOfView.angleDown,
            .nearPlane = m_nearPlane,
            .farPlane = m_farPlane,
            .applyPostViewCorrection = ApplyPostViewCorrection::Yes
        });
        // clang-format on
    }

    // Update the transformation matrix for the drawing hand from the pose
    {
        const auto &orientation = rightAimPose().orientation;
        glm::quat q(orientation.w, orientation.x, orientation.y, orientation.z);
        glm::mat4 mRot = glm::toMat4(q);
        const auto &position = rightAimPose().position;
        glm::vec3 p(position.x, position.y, position.z);
        glm::mat4 mTrans = glm::translate(glm::mat4(1.0f), p);
        glm::mat4 mScale = glm::scale(glm::mat4(1.0f), handScale());

        m_drawingHandTransform = mTrans * mRot * mScale;
    }
}

void XrDrawingScene::updateModelOffset()
{
    // The transform is only composed of a translation for the time being
    m_modelBaseTransform = glm::translate(glm::mat4(1.0f), modelOffset());

    auto bufferData = m_modelBaseTransformBuffer.map();
    std::memcpy(bufferData, &m_modelBaseTransform, sizeof(glm::mat4));
    m_modelBaseTransformBuffer.unmap();
}

void XrDrawingScene::updateTransformUbo()
{
    // Set the transform and color for the drawing hand as instance data
    InstanceData instanceData = {
        .transform = m_drawingHandTransform,
        .color = glm::vec4(shapeColor(), 1.0f)
    };

    auto bufferData = m_drawHandTransformBuffer.map();
    std::memcpy(bufferData, &instanceData, sizeof(InstanceData));
    m_drawHandTransformBuffer.unmap();
}

void XrDrawingScene::updateViewUbo()
{
    // Update the camera view
    auto cameraBufferData = static_cast<float *>(m_cameraBuffer.map());
    std::memcpy(cameraBufferData, m_cameraData.data() + m_currentViewIndex, sizeof(CameraData));
    m_cameraBuffer.unmap();
}

void XrDrawingScene::updateModelInstances()
{
    // The number of instances is the number of shapes in the model
    const auto shapes = m_drawingModel.shapes();
    auto totalInstanceCount = shapes.size();

    // We can't create an empty buffer, so if there are no instances, we can return early
    if (totalInstanceCount == 0)
        return;

    if (m_instanceTransformsCount < totalInstanceCount) {
        // Create a storage buffer large enough to hold all of the primitive world transforms
        BufferOptions bufferOptions = {
            .size = static_cast<DeviceSize>(totalInstanceCount * sizeof(InstanceData)),
            .usage = BufferUsageFlags(BufferUsageFlagBits::StorageBufferBit),
            .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
        };
        m_instanceTransformsBuffer = m_device->createBuffer(bufferOptions);

        // Create a bind group for the instance transform storage buffer
        // clang-format off
        BindGroupOptions bindGroupOptions = {
            .layout = m_instanceDataBindGroupLayout,
            .resources = {{
                .binding = 0,
                .resource = StorageBufferBinding{ .buffer = m_instanceTransformsBuffer}
            }}
        };
        // clang-format on
        m_instanceTransformsBindGroup = m_device->createBindGroup(bindGroupOptions);
    }

    // Loop through the model for each shape type and populate the instances data from their color and transform.
    {
        auto mappedData = static_cast<InstanceData *>(m_instanceTransformsBuffer.map());

        for (uint32_t shapeType = shapeIndex({}), startOffset = 0, firstInstance = 0; shapeType < shapeIndex(ShapeType::Count); shapeType++) {
            uint32_t shapeCount = 0;
            for (auto i = 0; i < shapes.size(); ++i) {
                if (shapes[i].shape == indexShape(shapeType)) {

                    InstanceData instanceData = {
                        .transform = shapes[i].transform(),
                        .color = glm::vec4(shapes[i].color, 1.0f)
                    };

                    std::memcpy(mappedData + shapeCount + startOffset,
                                &instanceData,
                                sizeof(instanceData));
                    ++shapeCount;
                }
            }

            // Keep a count for each shape, of instance count to use at render time
            m_shapeCounts[indexShape(shapeType)] = shapeCount;
            startOffset += shapeCount;
        }

        // The instances buffer is now populated so we can unmap it
        m_instanceTransformsBuffer.unmap();
    }
}

void XrDrawingScene::addShape(const DrawnShape &shape)
{
    m_drawingModel.addShape(shape);
    m_modelUpdated = true;
}

void XrDrawingScene::removeLastShape()
{
    m_drawingModel.truncate(m_drawingModel.shapes().size() - 1);
    m_modelUpdated = true;
}

void XrDrawingScene::renderView()
{
    m_fence.wait();
    m_fence.reset();

    // Update the scene data once per frame
    if (m_currentViewIndex == 0) {
        updateTransformUbo();

        if (m_modelUpdated) {
            updateModelInstances();
        }

        m_modelUpdated = false;
    }

    // Update the per-view camera matrices
    updateViewUbo();

    auto commandRecorder = m_device->createCommandRecorder();

    // Set up the render pass using the current color and depth texture views
    m_opaquePassOptions.colorAttachments[0].view = m_colorSwapchains[m_currentViewIndex].textureViews[m_currentColorImageIndex];
    m_opaquePassOptions.depthStencilAttachment.view = m_depthSwapchains[m_currentViewIndex].textureViews[m_currentDepthImageIndex];
    auto opaquePass = commandRecorder.beginRenderPass(m_opaquePassOptions);

    ///--------------------------------------------------------------------------------------------

    // Bind the frame (camera bind group) which does not change during the frame
    opaquePass.setBindGroup(0, m_cameraBindGroup, m_pipelineLayout);
    opaquePass.setPipeline(m_pipeline);

    if (!m_drawingModel.shapes().empty()) {
        opaquePass.setBindGroup(1, m_instanceTransformsBindGroup, m_pipelineLayout);
        opaquePass.setBindGroup(2, m_modelBaseTransformBindGroup, m_pipelineLayout);

        // Draw the model shapes, instanced
        for (uint32_t shapeType = shapeIndex({}), firstInstance = 0; shapeType < shapeIndex(ShapeType::Count); shapeType++) {

            auto &shapeBuffer = m_shapeBuffers[indexShape(shapeType)];
            auto shapeCount = m_shapeCounts[indexShape(shapeType)];

            opaquePass.setVertexBuffer(0, shapeBuffer.vertexBuffer);
            opaquePass.setIndexBuffer(shapeBuffer.indexBuffer);

            opaquePass.drawIndexed(DrawIndexedCommand{
                    .indexCount = shapeBuffer.numIndices,
                    .instanceCount = shapeCount,
                    .firstInstance = firstInstance });

            firstInstance += shapeCount;
        }
    }

    {
        // Draw a single instance for the draw-hand
        opaquePass.setBindGroup(1, m_drawHandTransformBindGroup, m_pipelineLayout);
        opaquePass.setBindGroup(2, m_identityTransformBindGroup, m_pipelineLayout);

        auto &handBuffers = m_shapeBuffers[shape()];

        opaquePass.setVertexBuffer(0, handBuffers.vertexBuffer);
        opaquePass.setIndexBuffer(handBuffers.indexBuffer);

        opaquePass.drawIndexed(DrawIndexedCommand{
                .indexCount = handBuffers.numIndices,
                .instanceCount = 1,
                .firstInstance = 0 });
    }

    opaquePass.end();
    m_commandBuffer = commandRecorder.finish();

    const SubmitOptions submitOptions = {
        .commandBuffers = { m_commandBuffer },
        .signalFence = m_fence
    };
    m_queue->submit(submitOptions);
}

void XrDrawingScene::load(const std::string &path)
{
    modelOffset = glm::vec3(0.0f);
    m_drawingModel.load(path);
    m_modelUpdated = true;
}

void XrDrawingScene::save(const std::string &path) const
{
    m_drawingModel.save(path);
}

void XrDrawingScene::clear()
{
    modelOffset = glm::vec3(0.0f);
    m_drawingModel.clear();
    m_modelUpdated = true;
}
