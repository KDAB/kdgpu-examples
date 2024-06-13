/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "model_scene.h"
#include "example_utility.h"

#include <KDGpuExample/engine.h>
#include <KDGpuExample/kdgpuexample.h>
#include <KDGpuExample/view_projection.h>

#include <KDGui/gui_application.h>

#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/device.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/texture_options.h>

#include <tinygltf_helper/tinygltf_helper.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <fstream>
#include <string>

ModelScene::ModelScene(const KDGpuExample::XrProjectionLayerOptions &options)
    : KDGpuExample::XrProjectionLayer(options)
{
    m_samples = SampleCountFlagBits::Samples8Bit;
}
inline std::string assetPath()
{
#if defined(KDGPU_ASSET_PATH)
    return KDGPU_ASSET_PATH;
#else
    return "";
#endif
}

void ModelScene::toggleRay(Hand hand)
{
    const auto handIndex = static_cast<uint32_t>(hand);
    m_rayHands[handIndex] = !m_rayHands[handIndex];

    // Store the animation start time and mark the ray as being animated
    m_rayAnimationData[handIndex].startTime = engine()->currentFrameTime();
    m_rayAnimationData[handIndex].animating = true;
}

void ModelScene::initializeScene()
{
    // Create some default textures and samplers
    m_opaqueWhite = createSolidColorTexture(1.0f, 1.0f, 1.0f, 1.0f);
    m_transparentBlack = createSolidColorTexture(0.0f, 0.0f, 0.0f, 0.0f);
    m_defaultNormal = createSolidColorTexture(0.5f, 0.5f, 1.0f, 1.0f);
    m_defaultSampler = m_device->createSampler();

    // Create bind group layout consisting of a single binding holding a UBO for the camera
    // clang-format off
    const BindGroupLayoutOptions cameraBindGroupLayoutOptions = {
        .bindings = {{
            .binding = 0,
            .resourceType = ResourceBindingType::UniformBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit)
        }}
    };
    // clang-format on
    m_cameraBindGroupLayout = m_device->createBindGroupLayout(cameraBindGroupLayoutOptions);

    // Create a bind group layout for the instance transform data
    // clang-format off
    const BindGroupLayoutOptions nodeBindGroupLayoutOptions = {
        .bindings = {{
            .binding = 0,
            .resourceType = ResourceBindingType::StorageBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit)
        }}
    };
    // clang-format on
    m_nodeBindGroupLayout = m_device->createBindGroupLayout(nodeBindGroupLayoutOptions);

    // Create a bind group layout for the material data
    // clang-format off
    const BindGroupLayoutOptions materialBindGroupLayoutOptions = {
        .bindings = {{
            .binding = 0,
            .resourceType = ResourceBindingType::UniformBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
        },{
            .binding = 1,
            .resourceType = ResourceBindingType::CombinedImageSampler,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
        }}
    };
    // clang-format on
    m_materialBindGroupLayout = m_device->createBindGroupLayout(materialBindGroupLayoutOptions);

    // Create a pipeline layout (array of bind group layouts)
    // clang-format off
    const PipelineLayoutOptions pipelineLayoutOptions = {
        .bindGroupLayouts = {
            m_cameraBindGroupLayout,    // Set 0
            m_nodeBindGroupLayout,      // Set 1
            m_materialBindGroupLayout   // Set 2
        },
        .pushConstantRanges = { m_modelTransformPushConstantRange }
    };
    // clang-format on
    m_pipelineLayout = m_device->createPipelineLayout(pipelineLayoutOptions);

    // Load the model
    tinygltf::Model model;
    // const std::string modelPath("AntiqueCamera/glTF/AntiqueCamera.gltf");
    // const std::string modelPath("BoxInterleaved/glTF/BoxInterleaved.gltf");
    // const std::string modelPath("FlightHelmet/FlightHelmet.gltf");
    // const std::string modelPath("Sponza/glTF/Sponza.gltf");
    const std::string modelPath("Buggy/Buggy.gltf");
    if (!TinyGltfHelper::loadModel(model, ExampleUtility::gltfModelPath() + modelPath))
        return;

    // Load any gltf images (Textures) needed
    const uint32_t textureCount = static_cast<uint32_t>(model.textures.size());
    SPDLOG_INFO("Model contains {} textures.", textureCount);
    m_textures.reserve(textureCount);
    for (uint32_t textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
        TextureAndView textureAndView = createTextureForImage(model, textureIndex);
        m_textures.emplace_back(std::move(textureAndView));
    }

    // Load any samplers needed
    const uint32_t samplerCount = static_cast<uint32_t>(model.samplers.size());
    SPDLOG_INFO("Model contains {} samplers.", samplerCount);
    m_samplers.reserve(samplerCount);
    for (uint32_t samplerIndex = 0; samplerIndex < samplerCount; ++samplerIndex) {
        Sampler sampler = createSampler(model, samplerIndex);
        m_samplers.emplace_back(std::move(sampler));
    }

    // Create the combined image samplers needed
    const uint32_t viewSamplerCount = static_cast<uint32_t>(model.textures.size());
    SPDLOG_INFO("Model contains {} view samplers.", viewSamplerCount);
    m_viewSamplers.reserve(viewSamplerCount);
    for (uint32_t viewSamplerIndex = 0; viewSamplerIndex < viewSamplerCount; ++viewSamplerIndex) {
        TextureViewAndSampler viewSampler = createViewSamplerForTexture(model, viewSamplerIndex);
        m_viewSamplers.emplace_back(std::move(viewSampler));
    }

    // Load any materials needed
    const uint32_t materialCount = static_cast<uint32_t>(model.materials.size());
    SPDLOG_INFO("Model contains {} materials.", materialCount);
    m_materialBuffers.reserve(materialCount);
    m_materialBindGroups.reserve(materialCount);
    for (uint32_t materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
        setupMaterial(model, materialIndex);
    }

    // Interrogate the model to see which usage flag we need for each buffer.
    // E.g. vertex buffer or index buffer. This is needed to then create suitable
    // buffers in the next step.
    std::vector<KDGpu::BufferUsageFlags> bufferViewUsages(model.bufferViews.size());
    auto markAccessorUsage = [&bufferViewUsages, &model](int accessorIndex, KDGpu::BufferUsageFlagBits flag) {
        const auto &accessor = model.accessors.at(accessorIndex);
        bufferViewUsages[accessor.bufferView] |= KDGpu::BufferUsageFlags(flag);
    };

    for (const auto &mesh : model.meshes) {
        for (const auto &primitive : mesh.primitives) {
            if (primitive.indices != -1)
                markAccessorUsage(primitive.indices, KDGpu::BufferUsageFlagBits::IndexBufferBit);

            for (const auto &[attributeName, accessorIndex] : primitive.attributes)
                markAccessorUsage(accessorIndex, KDGpu::BufferUsageFlagBits::VertexBufferBit);
        }
    }

    // Create buffers to hold the vertex data
    const uint32_t bufferViewCount = model.bufferViews.size();
    m_buffers.reserve(bufferViewCount);
    for (uint32_t bufferViewIndex = 0; bufferViewIndex < bufferViewCount; ++bufferViewIndex) {
        Buffer vertexBuffer = createBufferForBufferView(model, bufferViewUsages, bufferViewIndex);
        m_buffers.emplace_back(std::move(vertexBuffer));
    }

    // Calculate the world transforms of the node tree
    calculateWorldTransforms(model);

    // The next two blocks will populate the tracking data between pipelines and primitives
    // along with the transformation bind groups they need. We index by the primitive index.
    PrimitiveInstances primitiveInstances;

    // Find every node with a mesh and create a bind group containing the node's transform.
    uint32_t nodeIndex = 0;
    for (const auto &node : model.nodes) {
        if (node.mesh != -1)
            setupMeshNode(model, node, nodeIndex, primitiveInstances);
        ++nodeIndex;
    }

    // Create a storage buffer large enough to hold all of the primitive world transforms
    BufferOptions bufferOptions = {
        .size = primitiveInstances.totalInstanceCount * sizeof(glm::mat4),
        .usage = BufferUsageFlags(BufferUsageFlagBits::StorageBufferBit),
        .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
    };
    m_instanceTransformsBuffer = m_device->createBuffer(bufferOptions);

    // Create a bind group for the instance transform storage buffer
    // clang-format off
    BindGroupOptions bindGroupOptions = {
        .layout = m_nodeBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = StorageBufferBinding{ .buffer = m_instanceTransformsBuffer}
        }}
    };
    // clang-format on
    m_instanceTransformsBindGroup = m_device->createBindGroup(bindGroupOptions);

    // Loop through each primitive of each mesh and create a compatible WebGPU pipeline.
    // During this process we will also populate the instances world transform SSBO.
    primitiveInstances.mappedData = static_cast<glm::mat4 *>(m_instanceTransformsBuffer.map());
    uint32_t meshIndex = 0;
    for (const auto &mesh : model.meshes) {
        uint32_t primitiveIndex = 0;
        for (const auto &primitive : mesh.primitives) {
            const PrimitiveKey primitiveKey = { .meshIndex = meshIndex, .primitiveIndex = primitiveIndex };
            setupPrimitive(model, primitive, primitiveKey, primitiveInstances);
            ++primitiveIndex;
        }
        ++meshIndex;
    }
    m_renderStats.pipelineCount = m_pipelines.size();

    // The instances transform buffer is now populated so we can unmap it
    m_instanceTransformsBuffer.unmap();
    primitiveInstances.mappedData = nullptr;

    // Create a UBO and bind group for the camera. The contents of the camera UBO will be
    // populated in the updateScene() function.
    const BufferOptions cameraBufferOptions = {
        .label = "Camera Buffer",
        .size = viewCount() * sizeof(CameraData),
        .usage = BufferUsageFlags(BufferUsageFlagBits::UniformBufferBit),
        .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
    };
    m_cameraBuffer = m_device->createBuffer(cameraBufferOptions);

    // Upload identity matrices. Updated below in updateScene()
    const glm::mat4 identityMatrix(1.0f);
    auto bufferData = static_cast<float *>(m_cameraBuffer.map());
    std::memcpy(bufferData, glm::value_ptr(identityMatrix), sizeof(glm::mat4));
    std::memcpy(bufferData + 16, glm::value_ptr(identityMatrix), sizeof(glm::mat4));
    std::memcpy(bufferData + 32, glm::value_ptr(identityMatrix), sizeof(glm::mat4));
    std::memcpy(bufferData + 48, glm::value_ptr(identityMatrix), sizeof(glm::mat4));
    m_cameraBuffer.unmap();

    // clang-format off
    const BindGroupOptions cameraBindGroupOptions = {
        .layout = m_cameraBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_cameraBuffer }
        }}
    };
    // clang-format on
    m_cameraBindGroup = m_device->createBindGroup(cameraBindGroupOptions);

    // clang-format off
    m_opaquePassOptions = {
        .colorAttachments = {
            {
                .view = {}, // Not setting the swapchain texture view just yet
                .clearValue = { 0.3f, 0.3f, 0.3f, 1.0f },
                .finalLayout = TextureLayout::ColorAttachmentOptimal
            }
        },
        .depthStencilAttachment = {
            .view = {} // Not setting the depth texture view just yet
        },
        .viewCount = viewCount()
    };
    // clang-format on

    // We will use a fence to synchronize CPU and GPU. When we render image for each view (eye), we
    // shall wait for the fence to be signaled before we update any shared resources such as a view
    // matrix UBO (not used yet). An alternative would be to index into an array of such matrices.
    m_fence = m_device->createFence({ .label = "Projection Layer Scene Fence" });

    initializeHands();
    initializeRay();
}

void ModelScene::initializeHands()
{
    struct Vertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    // Create a buffer to hold triangle vertex data for the left controller
    {
        const std::array<Vertex, 3> vertexData = {
            Vertex{ // Back-left, red
                    .position = { -0.05f, 0.0f, 0.0f },
                    .color = { 1.0f, 0.0f, 0.0f } },
            Vertex{ // Back-right, red
                    .position = { 0.05f, 0.0f, 0.0f },
                    .color = { 1.0f, 0.0f, 0.0f } },
            Vertex{ // Front-center, red
                    .position = { 0.0f, 0.0f, -0.2f },
                    .color = { 1.0f, 0.0f, 0.0f } }
        };

        const DeviceSize dataByteSize = vertexData.size() * sizeof(Vertex);
        const BufferOptions bufferOptions = {
            .label = "Left Hand Triangle Vertex Buffer",
            .size = dataByteSize,
            .usage = BufferUsageFlagBits::VertexBufferBit | BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = MemoryUsage::GpuOnly
        };
        m_leftHandBuffer = m_device->createBuffer(bufferOptions);
        const BufferUploadOptions uploadOptions = {
            .destinationBuffer = m_leftHandBuffer,
            .dstStages = PipelineStageFlagBit::VertexAttributeInputBit,
            .dstMask = AccessFlagBit::VertexAttributeReadBit,
            .data = vertexData.data(),
            .byteSize = dataByteSize
        };
        uploadBufferData(uploadOptions);
    }

    // Create a buffer to hold triangle vertex data for the right controller
    {
        const std::array<Vertex, 3> vertexData = {
            Vertex{ // Back-left, blue
                    .position = { -0.05f, 0.0f, 0.0f },
                    .color = { 0.0f, 0.0f, 1.0f } },
            Vertex{ // Back-right, blue
                    .position = { 0.05f, 0.0f, 0.0f },
                    .color = { 0.0f, 0.0f, 1.0f } },
            Vertex{ // Front-center, blue
                    .position = { 0.0f, 0.0f, -0.2f },
                    .color = { 0.0f, 0.0f, 1.0f } }
        };

        const DeviceSize dataByteSize = vertexData.size() * sizeof(Vertex);
        const BufferOptions bufferOptions = {
            .label = "Right Hand Triangle Vertex Buffer",
            .size = dataByteSize,
            .usage = BufferUsageFlagBits::VertexBufferBit | BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = MemoryUsage::GpuOnly
        };
        m_rightHandBuffer = m_device->createBuffer(bufferOptions);
        const BufferUploadOptions uploadOptions = {
            .destinationBuffer = m_rightHandBuffer,
            .dstStages = PipelineStageFlagBit::VertexAttributeInputBit,
            .dstMask = AccessFlagBit::VertexAttributeReadBit,
            .data = vertexData.data(),
            .byteSize = dataByteSize
        };
        uploadBufferData(uploadOptions);
    }
    {
        const BufferOptions bufferOptions = {
            .label = "Left Hand Transformation Buffer",
            .size = sizeof(glm::mat4),
            .usage = BufferUsageFlagBits::UniformBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
        };
        m_leftHandTransformBuffer = m_device->createBuffer(bufferOptions);

        // Upload identity matrix. Updated below in updateScene()
        m_leftHandTransform = glm::mat4(1.0f);
        auto bufferData = m_leftHandTransformBuffer.map();
        std::memcpy(bufferData, &m_leftHandTransform, sizeof(glm::mat4));
        m_leftHandTransformBuffer.unmap();
    }

    // Create a buffer to hold the right hand transformation matrix
    {
        const BufferOptions bufferOptions = {
            .label = "Right Hand Transformation Buffer",
            .size = sizeof(glm::mat4),
            .usage = BufferUsageFlagBits::UniformBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
        };
        m_rightHandTransformBuffer = m_device->createBuffer(bufferOptions);

        // Upload identity matrix. Updated below in updateScene()
        m_rightHandTransform = glm::mat4(1.0f);
        auto bufferData = m_rightHandTransformBuffer.map();
        std::memcpy(bufferData, &m_rightHandTransform, sizeof(glm::mat4));
        m_rightHandTransformBuffer.unmap();
    }

    // Create bind group layout consisting of a single binding holding a UBO for the entity transform
    // clang-format off
    const BindGroupLayoutOptions handBindGroupLayoutOptions = {
        .label = "Hand Transform Bind Group",
        .bindings = {{
            .binding = 0,
            .resourceType = ResourceBindingType::UniformBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit)
        }}
    };
    // clang-format on
    m_solidTransformBindGroupLayout = m_device->createBindGroupLayout(handBindGroupLayoutOptions);

    // Create a bindGroup to hold the UBO with the left hand transform
    // clang-format off
    const BindGroupOptions leftHandBindGroupOptions = {
        .label = "Left Hand Transform Bind Group",
        .layout = m_solidTransformBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_leftHandTransformBuffer }
        }}
    };
    // clang-format on
    m_leftHandTransformBindGroup = m_device->createBindGroup(leftHandBindGroupOptions);

    // Create a bindGroup to hold the UBO with the right hand transform
    // clang-format off
    const BindGroupOptions rightHandBindGroupOptions = {
        .label = "Right Hand Transform Bind Group",
        .layout = m_solidTransformBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_rightHandTransformBuffer }
        }}
    };
    // clang-format on
    m_rightHandTransformBindGroup = m_device->createBindGroup(rightHandBindGroupOptions);

    // Create a pipeline to draw the hands with a solid color
    const auto vertexShaderPath = ExampleUtility::assetPath() + "/shaders/xr_multiview_model_viewer/solid.vert.spv";
    auto vertexShader = m_device->createShaderModule(KDGpuExample::readShaderFile(vertexShaderPath));

    const auto fragmentShaderPath = ExampleUtility::assetPath() + "/shaders/xr_multiview_model_viewer/solid.frag.spv";
    auto fragmentShader = m_device->createShaderModule(KDGpuExample::readShaderFile(fragmentShaderPath));

    const PipelineLayoutOptions pipelineLayoutOptions = {
        .label = "Hand Pipeline Layout",
        .bindGroupLayouts = { m_solidTransformBindGroupLayout, m_cameraBindGroupLayout }
    };
    m_handPipelineLayout = m_device->createPipelineLayout(pipelineLayoutOptions);

    // clang-format off
    const GraphicsPipelineOptions pipelineOptions = {
        .label = "Triangle",
        .shaderStages = {
            { .shaderModule = vertexShader, .stage = ShaderStageFlagBits::VertexBit },
            { .shaderModule = fragmentShader, .stage = ShaderStageFlagBits::FragmentBit }
        },
        .layout = m_handPipelineLayout,
        .vertex = {
            .buffers = {
                { .binding = 0, .stride = sizeof(Vertex) }
            },
            .attributes = {
                { .location = 0, .binding = 0, .format = Format::R32G32B32_SFLOAT }, // Position
                { .location = 1, .binding = 0, .format = Format::R32G32B32_SFLOAT, .offset = sizeof(glm::vec3) } // Color
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
        },
        .viewCount = viewCount()
    };
    // clang-format on
    m_handPipeline = m_device->createGraphicsPipeline(pipelineOptions);
}

void ModelScene::initializeRay()
{
    struct Vertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    // Create a buffer to hold triangle vertex data
    {
        // This is in model space which is y-up in this example. Note this is upside down vs the hello_triangle example which
        // draws the triangle directly in NDC space which is y-down.
        const float r = 0.8f;
        const std::array<Vertex, 4> vertexData = {
            Vertex{ // Bottom-left
                    .position = { -0.01f, 0.0f, 0.0f },
                    .color = { 0.0f, 0.0f, 0.0f } },
            Vertex{ // Bottom-right
                    .position = { 0.01f, 0.0f, 0.0f },
                    .color = { 0.0f, 0.0f, 0.0f } },
            Vertex{ // Top-Left
                    .position = { -0.01f, 0.0f, -1.0f },
                    .color = { 0.0f, 0.0f, 0.0f } },
            Vertex{ // Top-Right
                    .position = { 0.01f, 0.0f, -1.0f },
                    .color = { 0.0f, 0.0f, 0.0f } }
        };

        const DeviceSize dataByteSize = vertexData.size() * sizeof(Vertex);
        const BufferOptions bufferOptions = {
            .label = "Ray vertex buffer",
            .size = dataByteSize,
            .usage = BufferUsageFlagBits::VertexBufferBit | BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = MemoryUsage::GpuOnly
        };
        m_rayVertexBuffer = m_device->createBuffer(bufferOptions);
        const BufferUploadOptions uploadOptions = {
            .destinationBuffer = m_rayVertexBuffer,
            .dstStages = PipelineStageFlagBit::VertexAttributeInputBit,
            .dstMask = AccessFlagBit::VertexAttributeReadBit,
            .data = vertexData.data(),
            .byteSize = dataByteSize
        };
        uploadBufferData(uploadOptions);
    }

    // Create a buffer to hold the geometry index data
    {
        std::array<uint32_t, 6> indexData = { 0, 1, 2, 1, 3, 2 };
        const DeviceSize dataByteSize = indexData.size() * sizeof(uint32_t);
        const BufferOptions bufferOptions = {
            .label = "Index Buffer",
            .size = dataByteSize,
            .usage = BufferUsageFlagBits::IndexBufferBit | BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = MemoryUsage::GpuOnly
        };
        m_rayIndexBuffer = m_device->createBuffer(bufferOptions);
        const BufferUploadOptions uploadOptions = {
            .destinationBuffer = m_rayIndexBuffer,
            .dstStages = PipelineStageFlagBit::IndexInputBit,
            .dstMask = AccessFlagBit::IndexReadBit,
            .data = indexData.data(),
            .byteSize = dataByteSize
        };
        uploadBufferData(uploadOptions);
    }

    {
        const BufferOptions bufferOptions = {
            .label = "Left Ray Transformation Buffer",
            .size = sizeof(glm::mat4),
            .usage = BufferUsageFlagBits::UniformBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
        };
        m_leftRayTransformBuffer = m_device->createBuffer(bufferOptions);

        // Upload identity matrix. Updated below in updateScene()
        m_leftRayTransform = glm::mat4(1.0f);
        auto bufferData = m_leftRayTransformBuffer.map();
        std::memcpy(bufferData, &m_leftRayTransform, sizeof(glm::mat4));
        m_leftRayTransformBuffer.unmap();
    }

    // Create a bindGroup to hold the UBO with the left hand transform
    // clang-format off
    const BindGroupOptions leftRayBindGroupOptions = {
        .label = "Left Ray Transform Bind Group",
        .layout = m_solidTransformBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_leftRayTransformBuffer }
        }}
    };
    // clang-format on
    m_leftRayTransformBindGroup = m_device->createBindGroup(leftRayBindGroupOptions);

    {
        const BufferOptions bufferOptions = {
            .label = "Right Ray Transformation Buffer",
            .size = sizeof(glm::mat4),
            .usage = BufferUsageFlagBits::UniformBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
        };
        m_rightRayTransformBuffer = m_device->createBuffer(bufferOptions);

        // Upload identity matrix. Updated below in updateScene()
        m_rightRayTransform = glm::mat4(1.0f);
        auto bufferData = m_rightRayTransformBuffer.map();
        std::memcpy(bufferData, &m_rightRayTransform, sizeof(glm::mat4));
        m_rightRayTransformBuffer.unmap();
    }

    // Create a bindGroup to hold the UBO with the left hand transform
    // clang-format off
    const BindGroupOptions rightRayBindGroupOptions = {
        .label = "Right Ray Transform Bind Group",
        .layout = m_solidTransformBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_rightRayTransformBuffer }
        }}
    };
    // clang-format on
    m_rightRayTransformBindGroup = m_device->createBindGroup(rightRayBindGroupOptions);
}

Buffer ModelScene::createBufferForBufferView(const tinygltf::Model &model,
                                             const std::vector<KDGpu::BufferUsageFlags> &bufferViewUsages,
                                             uint32_t bufferViewIndex)
{
    const tinygltf::BufferView &gltfBufferView = model.bufferViews.at(bufferViewIndex);

    const auto bufferSize = static_cast<DeviceSize>(std::ceil(gltfBufferView.byteLength / 4.0) * 4);
    BufferOptions bufferOptions = {
        .size = bufferSize,
        .usage = bufferViewUsages.at(bufferViewIndex),
        .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
    };
    Buffer buffer = m_device->createBuffer(bufferOptions);

    const tinygltf::Buffer &gltfBuffer = model.buffers.at(gltfBufferView.buffer);

    auto bufferData = static_cast<uint8_t *>(buffer.map());
    std::memcpy(bufferData, gltfBuffer.data.data() + gltfBufferView.byteOffset, gltfBufferView.byteLength);

    buffer.unmap();

    return buffer;
}

TextureAndView ModelScene::createTextureForImage(const tinygltf::Model &model, uint32_t textureIndex)
{
    // TODO: Mip map generation
    TextureAndView textureAndView;
    const tinygltf::Image &gltfImage = model.images.at(textureIndex);

    // clang-format off
    const Extent3D extent = {
        .width = static_cast<uint32_t>(gltfImage.width),
        .height = static_cast<uint32_t>(gltfImage.height),
        .depth = 1
    };
    TextureOptions textureOptions = {
        .type = TextureType::TextureType2D,
        .format = Format::R8G8B8A8_UNORM,
        .extent = extent,
        .mipLevels = 1,
        .usage = TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly
    };
    // clang-format on
    textureAndView.texture = m_device->createTexture(textureOptions);

    // Upload the texture data and transition to ShaderReadOnlyOptimal
    // clang-format off
    const TextureUploadOptions uploadOptions = {
        .destinationTexture = textureAndView.texture,
        .dstStages = PipelineStageFlagBit::AllGraphicsBit,
        .dstMask = AccessFlagBit::MemoryReadBit,
        .data = gltfImage.image.data(),
        .byteSize = gltfImage.image.size(),
        .oldLayout = TextureLayout::Undefined,
        .newLayout = TextureLayout::ShaderReadOnlyOptimal,
        .regions = {{
            .textureSubResource = { .aspectMask = TextureAspectFlagBits::ColorBit },
            .textureExtent = extent
        }}
    };
    // clang-format on
    uploadTextureData(uploadOptions);

    // Create a view
    textureAndView.textureView = textureAndView.texture.createView();
    return textureAndView;
}

Sampler ModelScene::createSampler(const tinygltf::Model &model, uint32_t samplerIndex)
{
    const tinygltf::Sampler &gltfSampler = model.samplers.at(samplerIndex);
    SamplerOptions samplerOptions = {
        .magFilter = TinyGltfHelper::filterModeForSamplerFilter(gltfSampler.magFilter),
        .minFilter = TinyGltfHelper::filterModeForSamplerFilter(gltfSampler.minFilter),
        .mipmapFilter = TinyGltfHelper::mipmapFilterModeForSamplerFilter(gltfSampler.minFilter),
        .u = TinyGltfHelper::addressModeForSamplerAddressMode(gltfSampler.wrapS),
        .v = TinyGltfHelper::addressModeForSamplerAddressMode(gltfSampler.wrapT)
    };
    Sampler sampler = m_device->createSampler(samplerOptions);
    return sampler;
}

TextureViewAndSampler ModelScene::createViewSamplerForTexture(const tinygltf::Model &model, uint32_t viewSamplerIndex)
{
    const tinygltf::Texture &gltfTexture = model.textures.at(viewSamplerIndex);
    TextureViewAndSampler viewSampler = {
        .view = m_textures.at(gltfTexture.source).textureView,
        .sampler = m_samplers.at(gltfTexture.sampler)
    };
    return viewSampler;
}

// Used to create default placeholder textures when a material does not contain a texture for
// a particular use. This is needed because we can't indicate "no texture" to a shader.
TextureAndView ModelScene::createSolidColorTexture(float r, float g, float b, float a)
{
    const TextureOptions textureOptions = {
        .type = TextureType::TextureType2D,
        .format = Format::R8G8B8A8_UNORM,
        .extent = { .width = 1, .height = 1, .depth = 1 },
        .mipLevels = 1,
        .usage = TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly
    };

    TextureAndView textureAndView;
    textureAndView.texture = m_device->createTexture(textureOptions);

    const std::array<uint8_t, 4> textureData{ uint8_t(r * 255), uint8_t(g * 255), uint8_t(b * 255), uint8_t(a * 255) };
    // clang-format off
    const TextureUploadOptions uploadOptions = {
        .destinationTexture = textureAndView.texture,
        .dstStages = PipelineStageFlagBit::AllGraphicsBit,
        .dstMask = AccessFlagBit::MemoryReadBit,
        .data = textureData.data(),
        .byteSize = textureData.size(),
        .oldLayout = TextureLayout::Undefined,
        .newLayout = TextureLayout::ShaderReadOnlyOptimal,
        .regions = {{
            .textureSubResource = { .aspectMask = TextureAspectFlagBits::ColorBit },
            .textureExtent = { .width = 1, .height = 1, .depth = 1 }
        }}
    };
    // clang-format on
    uploadTextureData(uploadOptions);

    // Create a view
    textureAndView.textureView = textureAndView.texture.createView();
    return textureAndView;
}

void ModelScene::setupMaterial(const tinygltf::Model &model, uint32_t materialIndex)
{
    const tinygltf::Material &material = model.materials.at(materialIndex);

    // Create a buffer and populate it with the material data. Just the base color factor
    // and alpha cutoff will be implemented here for simplicity.
    const BufferOptions bufferOptions = {
        .size = 8 * sizeof(float), // Pad to 16 bytes for std140 layout
        .usage = BufferUsageFlagBits::UniformBufferBit | BufferUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly
    };
    Buffer buffer = m_device->createBuffer(bufferOptions);

    const auto &baseColorFactor = material.pbrMetallicRoughness.baseColorFactor;
    // clang-format off
    MaterialData materialData = {
        .baseColorFactor = {
            static_cast<float>(baseColorFactor[0]),
            static_cast<float>(baseColorFactor[1]),
            static_cast<float>(baseColorFactor[2]),
            static_cast<float>(baseColorFactor[3])
        },
        .alphaCutoff = static_cast<float>(material.alphaCutoff)
    };
    // clang-format on
    const BufferUploadOptions uploadOptions = {
        .destinationBuffer = buffer,
        .dstStages = PipelineStageFlagBit::FragmentShaderBit,
        .dstMask = AccessFlagBit::ShaderReadBit,
        .data = &materialData,
        .byteSize = sizeof(MaterialData)
    };
    uploadBufferData(uploadOptions);

    // Create a bind group for this material
    // clang-format off
    const int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
    const BindGroupOptions bindGroupOptions = {
        .layout = m_materialBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = buffer }
        }, {
            .binding = 1,
            .resource = TextureViewSamplerBinding{
                .textureView = (textureIndex != -1) ? m_viewSamplers.at(textureIndex).view : m_opaqueWhite.textureView,
                .sampler = (textureIndex != -1) ? m_viewSamplers.at(textureIndex).sampler : m_defaultSampler
            }
        }}
    };
    // clang-format on
    BindGroup bindGroup = m_device->createBindGroup(bindGroupOptions);

    // Store the buffer and bind group to keep them alive
    m_materialBuffers.emplace_back(std::move(buffer));
    m_materialBindGroups.emplace_back(std::move(bindGroup));
}

void ModelScene::setupPrimitive(const tinygltf::Model &model,
                                const tinygltf::Primitive &primitive,
                                const PrimitiveKey &primitiveKey,
                                PrimitiveInstances &primitiveInstances)
{
    std::vector<BufferAndOffset> buffers;
    VertexOptions vertexOptions{};
    uint32_t nextBinding{ 0 };
    uint32_t currentBinding{ 0 };
    uint32_t vertexCount{ 0 };

    // Used to keep track of which bindings are used for each buffer view and
    // which attributes use each vertex buffer layout
    std::map<int, std::vector<uint32_t>> bufferViewToLayoutMap;
    std::map<uint32_t, std::vector<uint32_t>> layoutToAttributeMap;

    const std::unordered_map<std::string, uint32_t> shaderLocations{ { "POSITION", 0 }, { "NORMAL", 1 }, { "TEXCOORD_0", 2 } };
    bool hasTexCoords{ false };

    // Iterate over each attribute in the primitive to build up a description of the
    // vertex layout needed to create the pipeline.
    for (const auto &attribute : primitive.attributes) {
        const auto &accessor = model.accessors.at(attribute.second);
        const uint32_t bufferIndex = accessor.bufferView;
        const auto &bufferView = model.bufferViews.at(accessor.bufferView);

        // Find the location for this attribute (if any)
        auto locationIt = shaderLocations.find(attribute.first);
        if (locationIt == shaderLocations.end())
            continue;
        const uint32_t location = locationIt->second;

        // Note if the model contains texture coordinates so that we can select a suitable
        // shader variant below.
        if (location == 2)
            hasTexCoords = true;

        // Do we already have a binding for this buffer view?
        bool foundCompatibleLayout = false;
        const auto layoutIt = bufferViewToLayoutMap.find(accessor.bufferView);
        if (layoutIt != bufferViewToLayoutMap.end()) {
            // We already have one or more bindings (buffer layouts) for this buffer view. Are any
            // of them compatible with this use of the buffer view? Are the offsets within limits?
            const std::vector<uint32_t> &layoutIndices = layoutIt->second;

            for (const auto &layoutIndex : layoutIndices) {
                // Loop over attributes for this buffer layout
                VertexBufferLayout &bufferLayout = vertexOptions.buffers[layoutIndex];
                const std::vector<uint32_t> &attributeIndices = layoutToAttributeMap.at(layoutIndex);

                for (const auto &attributeIndex : attributeIndices) {
                    const DeviceSize attributeOffsetDelta = std::abs(
                            int32_t(accessor.byteOffset) - int32_t(vertexOptions.attributes[attributeIndex].offset));
                    if (attributeOffsetDelta < vertexOptions.buffers[layoutIndex].stride) {
                        // Found a compatible vertex buffer layout
                        foundCompatibleLayout = true;
                        currentBinding = layoutIndex;

                        // Track the minimum offset across all attributes that share a buffer layout
                        BufferAndOffset &buffer = buffers.at(layoutIndex);
                        buffer.offset = std::min(buffer.offset, static_cast<DeviceSize>(accessor.byteOffset));

                        break;
                    }
                }
            }
        }

        if (!foundCompatibleLayout) {
            // Add a buffer layout and binding for this buffer view
            const VertexBufferLayout bufferLayout = {
                .binding = nextBinding,
                .stride = bufferView.byteStride ? static_cast<uint32_t>(bufferView.byteStride)
                                                : TinyGltfHelper::packedArrayStrideForAccessor(accessor)
            };
            vertexOptions.buffers.push_back(bufferLayout);

            // Note that this buffer view maps to this binding
            bufferViewToLayoutMap.insert({ accessor.bufferView, { nextBinding } });
            layoutToAttributeMap.insert({ nextBinding, {} });

            // Store the buffer and offset to be used with this buffer layout.
            // NB: The index in the vector is equal to the buffer layout binding
            buffers.push_back({ .buffer = m_buffers.at(bufferIndex), .offset = accessor.byteOffset });

            currentBinding = nextBinding;
            ++nextBinding;
        }

        // Set up the vertex attribute using the current binding
        const VertexAttribute vertexAttribute = { .location = location,
                                                  .binding = currentBinding,
                                                  .format = TinyGltfHelper::formatForAccessor(accessor),
                                                  .offset = accessor.byteOffset };
        vertexOptions.attributes.push_back(vertexAttribute);

        uint32_t attributeIndex = static_cast<uint32_t>(vertexOptions.attributes.size() - 1);
        layoutToAttributeMap.at(currentBinding).push_back(attributeIndex);

        vertexCount = accessor.count;
    }

    // Normalize attribute offsets by subtracting off the buffer layout offset. By nature of the
    // way in which we create the buffer layouts they are already sorted by binding number.
    const uint32_t bufferLayoutCount = vertexOptions.buffers.size();
    for (uint32_t bufferLayoutIndex = 0; bufferLayoutIndex < bufferLayoutCount; ++bufferLayoutIndex) {
        const BufferAndOffset &buffer = buffers.at(bufferLayoutIndex);
        const std::vector<uint32_t> &attributeIndices = layoutToAttributeMap.at(bufferLayoutIndex);
        for (const auto &attributeIndex : attributeIndices)
            vertexOptions.attributes[attributeIndex].offset -= buffer.offset;
    }

    // Sort the attributes to be in order of their location. This normalizes the data so that we can
    // compare them to remove duplicates.
    std::sort(vertexOptions.attributes.begin(), vertexOptions.attributes.end(),
              [](const VertexAttribute &a, const VertexAttribute &b) { return a.location < b.location; });

    // Find or create a pipeline that is compatible with the above vertex buffer layout and topology
    const tinygltf::Material &gltfMaterial = model.materials.at(primitive.material);
    const TinyGltfHelper::AlphaMode alphaMode = TinyGltfHelper::alphaModeForMaterialAlphaMode(gltfMaterial.alphaMode);
    Handle<GraphicsPipeline_t> pipelineHandle;
    // clang-format off
    const GraphicsPipelineKey key = {
        .topology = TinyGltfHelper::topologyForPrimitiveMode(primitive.mode),
        .vertexOptions = std::move(vertexOptions),
        .alphaMode = alphaMode,
        .doubleSided = gltfMaterial.doubleSided,
        .shaderOptionsKey = {
            .hasTexCoords = hasTexCoords,
            .enableAlphaCutoff = alphaMode == TinyGltfHelper::AlphaMode::Mask
        }
    };
    // clang-format on

    const auto pipelineIt = m_pipelines.find(key);
    if (pipelineIt == m_pipelines.end()) {
        // Create a pipeline compatible with the above vertex buffer and attribute layout
        // clang-format off
        constexpr BlendOptions opaqueBlendOptions = {};
        constexpr BlendOptions alphaBlendOptions = {
            .blendingEnabled = true,
            .color = {
                .srcFactor = BlendFactor::SrcAlpha,
                .dstFactor = BlendFactor::OneMinusSrcAlpha
            }
        };

        const auto vertexShader = findOrCreateShaderModule(ShaderModuleKey{
            .shaderOptionsKey = key.shaderOptionsKey,
            .stage = ShaderStageFlagBits::VertexBit
        });
        const auto fragmentShader = findOrCreateShaderModule(ShaderModuleKey{
            .shaderOptionsKey = key.shaderOptionsKey,
            .stage = ShaderStageFlagBits::FragmentBit
        });

        GraphicsPipelineOptions pipelineOptions = {
            .shaderStages = {
                { .shaderModule = vertexShader, .stage = ShaderStageFlagBits::VertexBit },
                { .shaderModule = fragmentShader, .stage = ShaderStageFlagBits::FragmentBit }
            },
            .layout = m_pipelineLayout,
            .vertex = key.vertexOptions,
            .renderTargets = {{
                .format = m_colorSwapchainFormat,
                .blending = key.alphaMode == TinyGltfHelper::AlphaMode::Blend ? alphaBlendOptions : opaqueBlendOptions
            }},
            .depthStencil = {
                .format = m_depthSwapchainFormat,
                .depthWritesEnabled = true,
                .depthCompareOperation = CompareOperation::Less
            },
            .primitive = {
                .topology = key.topology,
                .cullMode = key.doubleSided ? CullModeFlagBits::None : CullModeFlagBits::BackBit
            },
            .viewCount = viewCount()
        };
        // clang-format on
        GraphicsPipeline pipeline = m_device->createGraphicsPipeline(pipelineOptions);
        pipelineHandle = pipeline.handle();
        m_pipelines.insert({ key, std::move(pipeline) });
    } else {
        pipelineHandle = pipelineIt->second.handle();
    }

    // Determine draw type and cache enough information for render time
    const auto &instances = primitiveInstances.instanceData.at(primitiveKey);
    PrimitiveData primitiveData = {
        .instances = setupPrimitiveInstances(primitiveKey, primitiveInstances),
        .vertexBuffers = buffers
    };
    if (primitive.indices == -1) {
        primitiveData.drawType = PrimitiveData::DrawType::NonIndexed;
        primitiveData.drawData = { .vertexCount = vertexCount };
    } else {
        const auto &accessor = model.accessors.at(primitive.indices);
        const IndexedDraw indexedDraw = {
            .indexBuffer = m_buffers.at(accessor.bufferView),
            .offset = accessor.byteOffset,
            .indexCount = static_cast<uint32_t>(accessor.count),
            .indexType = TinyGltfHelper::indexTypeForComponentType(accessor.componentType)
        };
        primitiveData.drawType = PrimitiveData::DrawType::Indexed;
        primitiveData.drawData = { .indexedDraw = indexedDraw };
    }

    // Find the pipeline in our draw data or create a new one and append this material and
    // primitive to it (including all instances)
    const Handle<BindGroup_t> material = m_materialBindGroups.at(primitive.material);
    auto pipelineMaterialIt = m_pipelinePrimitiveMap.find(pipelineHandle);
    if (pipelineMaterialIt == m_pipelinePrimitiveMap.end()) {
        const MaterialPrimitives materialPrimitive{
            .material = material,
            .primitives = { primitiveData }
        };
        m_pipelinePrimitiveMap.insert({ pipelineHandle, { materialPrimitive } });
    } else {
        //  Find the MaterialPrimitives object we should append the primitive too (if any).
        //  If this material is not yet included, then add it.
        auto &pipelineMaterialPrimitives = pipelineMaterialIt->second;
        const auto materialPrimitivesIt = std::find_if(
                pipelineMaterialPrimitives.begin(),
                pipelineMaterialPrimitives.end(),
                [&material](const MaterialPrimitives &x) {
                    return x.material == material;
                });
        if (materialPrimitivesIt == pipelineMaterialPrimitives.end()) {
            // Append the material and this primitive as the first user of it
            const MaterialPrimitives materialPrimitive{
                .material = material,
                .primitives = { primitiveData }
            };
            pipelineMaterialPrimitives.push_back(materialPrimitive);
        } else {
            // Append this primitive as another user of this material (and pipeline)
            materialPrimitivesIt->primitives.push_back(primitiveData);
        }
    }
}

Handle<ShaderModule_t> ModelScene::findOrCreateShaderModule(const ShaderModuleKey &key)
{
    const auto shaderModuleIt = m_shaderModules.find(key);
    if (shaderModuleIt != m_shaderModules.end()) {
        return shaderModuleIt->second.handle();
    } else {
        const auto shaderPath = ExampleUtility::assetPath() + "/shaders/xr_multiview_model_viewer/" + shaderFilename(key.shaderOptionsKey, key.stage);
        ShaderModule shader = m_device->createShaderModule(KDGpuExample::readShaderFile(shaderPath));
        const auto shaderHandle = shader.handle();
        m_shaderModules.insert({ key, std::move(shader) });
        return shaderHandle;
    }
}

// TODO: Should we make this general and use the configuration file used to generate the shader
// at configure/build time?
std::string ModelScene::shaderFilename(const PipelineShaderOptionsKey &key, ShaderStageFlagBits stage)
{
    std::string name = "materials";
    switch (stage) {
    case ShaderStageFlagBits::VertexBit: {
        if (key.hasTexCoords)
            name += "_texcoord_0_enabled";
        name += ".vert.spv";
        break;
    }

    case ShaderStageFlagBits::FragmentBit: {
        if (key.hasTexCoords)
            name += "_texcoord_0_enabled";
        if (key.enableAlphaCutoff)
            name += "_alpha_cutoff_enabled";
        name += ".frag.spv";
        break;
    }

    default: {
        SPDLOG_CRITICAL("Unhandled shader type");
        return {};
    }
    }
    return name;
}

InstancedDraw ModelScene::setupPrimitiveInstances(const PrimitiveKey &primitiveKey,
                                                  PrimitiveInstances &primitiveInstances)
{
    const auto &instanceDataSet = primitiveInstances.instanceData.at(primitiveKey);
    const uint32_t instanceCount = static_cast<uint32_t>(instanceDataSet.size());

    uint32_t firstInstance = primitiveInstances.offset;
    uint32_t instanceIndex = 0;
    for (const auto instanceData : instanceDataSet) {
        std::memcpy(primitiveInstances.mappedData + primitiveInstances.offset + instanceIndex,
                    glm::value_ptr(m_worldTransforms.at(instanceData.worldTransformIndex)),
                    sizeof(glm::mat4));
        ++instanceIndex;
    }
    primitiveInstances.offset += static_cast<uint32_t>(instanceDataSet.size());

    return InstancedDraw{
        .firstInstance = firstInstance,
        .instanceCount = instanceCount
    };
}

void ModelScene::calculateWorldTransforms(const tinygltf::Model &model)
{
    std::vector<bool> visited(model.nodes.size());
    m_worldTransforms.resize(model.nodes.size());

    // Starting at the root node of each scene in the gltf file, traverse the node
    // tree and recursively calculate the world transform of each node. The results are
    // stored in the m_worldTransforms with indices matching those of the glTF nodes vector.
    // clang-format off
    for (const auto &scene : model.scenes) {
        for (const auto &rootNodeIndex : scene.nodes) {
            TinyGltfHelper::calculateNodeTreeWorldTransforms(
                model,              // The gltf model is passed in to lookup other data
                rootNodeIndex,      // The root node of the current scene
                glm::dmat4(1.0f),   // Initial transform of root is the identity matrix
                visited,            // To know which nodes we have already calculated
                m_worldTransforms); // The vector of transforms to calculate. Same indices as model.nodes
        }
    }
    // clang-format on
}

void ModelScene::setupMeshNode(const tinygltf::Model &model,
                               const tinygltf::Node &node,
                               uint32_t nodeIndex,
                               PrimitiveInstances &primitiveInstances)
{
    // Loop through every primitive of the node's mesh and append this node's transform bind
    // group to the primitive's instance list.
    const tinygltf::Mesh &mesh = model.meshes.at(node.mesh);
    const uint32_t meshIndex = static_cast<uint32_t>(node.mesh);
    uint32_t primitiveIndex = 0;
    for (const auto primitive : mesh.primitives) {
        const PrimitiveKey primitiveKey = { .meshIndex = meshIndex, .primitiveIndex = primitiveIndex };
        const auto instancesIt = primitiveInstances.instanceData.find(primitiveKey);
        if (instancesIt != primitiveInstances.instanceData.end()) {
            auto &instances = instancesIt->second;
            instances.push_back(InstanceData{ .worldTransformIndex = nodeIndex });
        } else {
            const std::vector<InstanceData> instances{ InstanceData{ .worldTransformIndex = nodeIndex } };
            primitiveInstances.instanceData.insert({ primitiveKey, instances });
        }

        ++primitiveIndex;
    }

    // Record how many instances we tracked
    primitiveInstances.totalInstanceCount += static_cast<uint32_t>(mesh.primitives.size());
}

void ModelScene::cleanupScene()
{
    m_rightHandTransformBindGroup = {};
    m_rightHandTransformBuffer = {};
    m_rightHandBuffer = {};
    m_leftHandTransformBindGroup = {};
    m_leftHandTransformBuffer = {};
    m_leftHandBuffer = {};
    m_handPipelineLayout = {};
    m_handPipeline = {};

    m_msaaTextureView = {};
    m_msaaTexture = {};
    m_opaqueWhite = {};
    m_transparentBlack = {};
    m_defaultNormal = {};
    m_defaultSampler = {};
    m_materialBuffers.clear();
    m_materialBindGroups.clear();
    m_cameraBindGroup = {};
    m_cameraBuffer = {};
    m_instanceTransformsBindGroup = {};
    m_instanceTransformsBuffer = {};
    m_pipelines.clear();
    m_shaderModules.clear();
    m_materialBindGroupLayout = {};
    m_nodeBindGroupLayout = {};
    m_cameraBindGroupLayout = {};
    m_pipelineLayout = {};
    m_viewSamplers.clear();
    m_samplers.clear();
    m_textures.clear();
    m_buffers.clear();
    m_commandBuffer = {};
}

void ModelScene::initialize()
{
    XrProjectionLayer::initialize();

    // Check to see if multiview is actually enabled? If not we will abort.
    // In a real application you could fallback to a non-multiview configuration
    // as we use in the hello_xr example.
    if (!m_enableMultiview) {
        SPDLOG_LOGGER_CRITICAL(logger(), "This example requires a GPU with multiview support. Aborting.");
        KDGui::GuiApplication::instance()->quit();
        return;
    }

    initializeScene();
}

void ModelScene::cleanup()
{
    cleanupScene();
    XrProjectionLayer::cleanup();
}

void ModelScene::updateScene()
{
    // Update the camera data for each view
    m_cameraData.resize(m_viewState.viewCount());
    for (uint32_t viewIndex = 0; viewIndex < m_viewState.viewCount(); ++viewIndex) {
        const auto &view = m_viewState.views[viewIndex];
        const KDXr::Quaternion &orientation = view.pose.orientation;
        const KDXr::Vector3 &position = view.pose.position;

        // clang-format off
        m_cameraData[viewIndex].view = KDGpuExample::viewMatrix({
            .orientation = glm::quat(orientation.w, orientation.x, orientation.y, orientation.z),
            .position = glm::vec3(position.x, position.y, position.z)
        });
        m_cameraData[viewIndex].projection = KDGpuExample::perspective({
            .leftFieldOfView = m_viewState.views[viewIndex].fieldOfView.angleLeft,
            .rightFieldOfView = m_viewState.views[viewIndex].fieldOfView.angleRight,
            .upFieldOfView = m_viewState.views[viewIndex].fieldOfView.angleUp,
            .downFieldOfView = m_viewState.views[viewIndex].fieldOfView.angleDown,
            .nearPlane = m_nearPlane,
            .farPlane = m_farPlane,
            .applyPostViewCorrection = KDGpuExample::ApplyPostViewCorrection::Yes
        });
        // clang-format on
    }

    // Update the transformation matrix for the left hand and ray from the pose
    {
        const auto &orientation = leftPalmPose().orientation;
        glm::quat q(orientation.w, orientation.x, orientation.y, orientation.z);
        glm::mat4 mRot = glm::toMat4(q);
        const auto &position = leftPalmPose().position;
        glm::vec3 p(position.x, position.y, position.z);
        glm::mat4 mTrans = glm::translate(glm::mat4(1.0f), p);
        m_leftHandTransform = mTrans * mRot;

        float length = 4.0f;
        if (m_rayAnimationData[0].animating) {
            const auto msecsSinceStart = (engine()->currentFrameTime() - m_rayAnimationData[0].startTime).count() / 1e6;
            const float t = float(msecsSinceStart) / float(m_rayAnimationData[0].durationMs);
            if (m_rayHands[0] == true) {
                // Turning on animation
                length *= std::powf(t, 3.0f);
            } else {
                // Turning off animation
                length *= 1.0f - std::powf(t, 3.0f);
            }

            if (msecsSinceStart > m_rayAnimationData[0].durationMs)
                m_rayAnimationData[0].animating = false;
        }
        glm::mat4 mRayScaleAndOffset = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.2f));
        mRayScaleAndOffset = glm::scale(mRayScaleAndOffset, glm::vec3(1.0f, 1.0f, length));
        m_leftRayTransform = glm::mat4(1.0f);
        m_leftRayTransform = mTrans * mRot * mRayScaleAndOffset;
    }

    // Update the transformation matrix for the right hand and ray from the pose
    {
        const auto &orientation = rightPalmPose().orientation;
        glm::quat q(orientation.w, orientation.x, orientation.y, orientation.z);
        glm::mat4 mRot = glm::toMat4(q);
        const auto &position = rightPalmPose().position;
        glm::vec3 p(position.x, position.y, position.z);
        glm::mat4 mTrans = glm::translate(glm::mat4(1.0f), p);
        m_rightHandTransform = mTrans * mRot;

        float length = 4.0f;
        if (m_rayAnimationData[1].animating) {
            const auto msecsSinceStart = (engine()->currentFrameTime() - m_rayAnimationData[1].startTime).count() / 1e6;
            const float t = float(msecsSinceStart) / float(m_rayAnimationData[1].durationMs);
            if (m_rayHands[1] == true) {
                // Turning on animation
                length *= std::powf(t, 3.0f);
            } else {
                // Turning off animation
                length *= 1.0f - std::powf(t, 3.0f);
            }

            if (msecsSinceStart > m_rayAnimationData[1].durationMs)
                m_rayAnimationData[1].animating = false;
        }
        glm::mat4 mRayScaleAndOffset = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.2f));
        mRayScaleAndOffset = glm::scale(mRayScaleAndOffset, glm::vec3(1.0f, 1.0f, length));
        m_rightRayTransform = glm::mat4(1.0f);
        m_rightRayTransform = mTrans * mRot * mRayScaleAndOffset;
    }

    // Update the transform for the entire model
    // TODO: Replace this hack with a simple scene graph.
    m_modelTransform = glm::mat4(1.0f);
    m_modelTransform = glm::translate(m_modelTransform, translation());
    m_modelTransform = glm::scale(m_modelTransform, glm::vec3(scale()));

    static TimePoint s_lastFpsTimestamp;
    const auto frameEndTime = std::chrono::high_resolution_clock::now();
    const auto timer = std::chrono::duration<double, std::milli>(frameEndTime - s_lastFpsTimestamp).count();
    if (timer > 1000.0) {
        s_lastFpsTimestamp = frameEndTime;

        SPDLOG_INFO("pipelines = {}, setPipelineCount = {}, setVertexBufferCount = {}, setBindGroupCount = {}, drawCount = {}, verts = {}",
                    m_renderStats.pipelineCount, m_renderStats.setPipelineCount, m_renderStats.setVertexBufferCount,
                    m_renderStats.setBindGroupCount, m_renderStats.drawCount, m_renderStats.vertexCount);
    }

    m_renderStats.setPipelineCount = 0;
    m_renderStats.setVertexBufferCount = 0;
    m_renderStats.setBindGroupCount = 0;
    m_renderStats.drawCount = 0;
    m_renderStats.vertexCount = 0;
}

void ModelScene::updateTransformUbo()
{
    auto bufferData = m_leftHandTransformBuffer.map();
    std::memcpy(bufferData, &m_leftHandTransform, sizeof(glm::mat4));
    m_leftHandTransformBuffer.unmap();

    bufferData = m_rightHandTransformBuffer.map();
    std::memcpy(bufferData, &m_rightHandTransform, sizeof(glm::mat4));
    m_rightHandTransformBuffer.unmap();

    bufferData = m_leftRayTransformBuffer.map();
    std::memcpy(bufferData, &m_leftRayTransform, sizeof(glm::mat4));
    m_leftRayTransformBuffer.unmap();

    bufferData = m_rightRayTransformBuffer.map();
    std::memcpy(bufferData, &m_rightRayTransform, sizeof(glm::mat4));
    m_rightRayTransformBuffer.unmap();
}

void ModelScene::updateViewUbo()
{
    auto cameraBufferData = static_cast<float *>(m_cameraBuffer.map());
    std::memcpy(cameraBufferData, m_cameraData.data(), viewCount() * sizeof(CameraData));
    m_cameraBuffer.unmap();
}

void ModelScene::renderView()
{
    m_fence.wait();
    m_fence.reset();

    // Update the scene data once per frame
    updateTransformUbo();

    // Update the per-view camera matrices
    updateViewUbo();

    auto commandRecorder = m_device->createCommandRecorder();

    // Set up the render pass using the current color and depth texture views
    m_opaquePassOptions.colorAttachments[0].view = m_colorSwapchains[0].textureViews[m_currentColorImageIndex];
    m_opaquePassOptions.depthStencilAttachment.view = m_depthSwapchains[0].textureViews[m_currentDepthImageIndex];
    auto opaquePass = commandRecorder.beginRenderPass(m_opaquePassOptions);

    // Bind the frame (camera bind group) which does not change during the frame
    opaquePass.setBindGroup(0, m_cameraBindGroup, m_pipelineLayout);
    opaquePass.setBindGroup(1, m_instanceTransformsBindGroup, m_pipelineLayout);
    m_renderStats.setBindGroupCount += 2;

    for (const auto &[pipeline, materialPrimitives] : m_pipelinePrimitiveMap) {
        opaquePass.setPipeline(pipeline);
        ++m_renderStats.setPipelineCount;

        // Iterate over each material primitives using this pipeline
        for (const auto &materialPrimitiveData : materialPrimitives) {
            opaquePass.setBindGroup(2, materialPrimitiveData.material);
            ++m_renderStats.setBindGroupCount;

            // Iterate over each primitive using this pipeline
            for (const auto &primitiveData : materialPrimitiveData.primitives) {

                // Bind the vertex buffers for this primitive
                uint32_t vertexBufferBinding = 0;
                for (const auto &vertexBuffer : primitiveData.vertexBuffers) {
                    opaquePass.setVertexBuffer(vertexBufferBinding, vertexBuffer.buffer, vertexBuffer.offset);
                    ++m_renderStats.setVertexBufferCount;
                    ++vertexBufferBinding;
                }

                opaquePass.pushConstant(m_modelTransformPushConstantRange, &m_modelTransform);

                // Render every instance of this primitive in a single call.
                if (primitiveData.drawType == PrimitiveData::DrawType::NonIndexed) {
                    opaquePass.draw(DrawCommand{
                            .vertexCount = primitiveData.drawData.vertexCount,
                            .instanceCount = primitiveData.instances.instanceCount,
                            .firstInstance = primitiveData.instances.firstInstance });
                    m_renderStats.vertexCount += primitiveData.drawData.vertexCount * primitiveData.instances.instanceCount;
                } else {
                    const IndexedDraw &indexedDraw = primitiveData.drawData.indexedDraw;
                    opaquePass.setIndexBuffer(indexedDraw.indexBuffer, indexedDraw.offset, indexedDraw.indexType);
                    opaquePass.drawIndexed(DrawIndexedCommand{
                            .indexCount = indexedDraw.indexCount,
                            .instanceCount = primitiveData.instances.instanceCount,
                            .firstInstance = primitiveData.instances.firstInstance });
                    m_renderStats.vertexCount += indexedDraw.indexCount * primitiveData.instances.instanceCount;
                }
                ++m_renderStats.drawCount;
            }
        }
    }

    opaquePass.setPipeline(m_handPipeline);
    opaquePass.setBindGroup(0, m_cameraBindGroup);
    // draw left hand triangle
    opaquePass.setVertexBuffer(0, m_leftHandBuffer);
    opaquePass.setBindGroup(1, m_leftHandTransformBindGroup);
    const DrawIndexedCommand drawCmd = { .indexCount = 3 };
    opaquePass.drawIndexed(drawCmd);

    // Draw the right hand triangle
    opaquePass.setVertexBuffer(0, m_rightHandBuffer);
    opaquePass.setBindGroup(1, m_rightHandTransformBindGroup);
    opaquePass.drawIndexed(drawCmd);

    // Draw the ray
    opaquePass.setVertexBuffer(0, m_rayVertexBuffer);
    opaquePass.setIndexBuffer(m_rayIndexBuffer);
    if (m_rayHands[0] || m_rayAnimationData[0].animating) {
        opaquePass.setBindGroup(1, m_leftRayTransformBindGroup);
        opaquePass.drawIndexed({ .indexCount = 6 });
    }
    if (m_rayHands[1] || m_rayAnimationData[1].animating) {
        opaquePass.setBindGroup(1, m_rightRayTransformBindGroup);
        opaquePass.drawIndexed({ .indexCount = 6 });
    }

    opaquePass.end();
    m_commandBuffer = commandRecorder.finish();

    const SubmitOptions submitOptions = {
        .commandBuffers = { m_commandBuffer },
        .signalFence = m_fence
    };
    m_queue->submit(submitOptions);
}
