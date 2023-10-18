#include "pipeline_caching.h"

#include <KDGpuExample/engine.h>
#include <KDGpuExample/kdgpuexample.h>

#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/graphics_pipeline_options.h>

#include <tinygltf_helper/tinygltf_helper.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <fstream>
#include <string>

namespace {

inline std::string assetPath()
{
#if defined(GLTF_RENDERER_ASSET_PATH)
    return GLTF_RENDERER_ASSET_PATH;
#else
    return "";
#endif
}

} // namespace

void PipelineCaching::initializeScene()
{
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
    m_cameraBindGroupLayout = m_device.createBindGroupLayout(cameraBindGroupLayoutOptions);

    const BindGroupLayoutOptions nodeBindGroupLayoutOptions = {
        .bindings = { { .binding = 0,
                        .resourceType = ResourceBindingType::UniformBuffer,
                        .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit) } }
    };
    // clang-format on
    m_nodeBindGroupLayout = m_device.createBindGroupLayout(nodeBindGroupLayoutOptions);

    // Create a pipeline layout (array of bind group layouts)
    const PipelineLayoutOptions pipelineLayoutOptions = { .bindGroupLayouts = { m_cameraBindGroupLayout,
                                                                                m_nodeBindGroupLayout } };
    m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutOptions);

    const auto vertexShaderPath = assetPath() + "/shaders/01_simple_gltf/simple_gltf.vert.spv";
    m_vertexShader = m_device.createShaderModule(KDGpuExample::readShaderFile(vertexShaderPath));

    const auto fragmentShaderPath = assetPath() + "/shaders/01_simple_gltf/simple_gltf.frag.spv";
    m_fragmentShader = m_device.createShaderModule(KDGpuExample::readShaderFile(fragmentShaderPath));

    // Load the model
    tinygltf::Model model;
    // const std::string modelPath("AntiqueCamera/glTF/AntiqueCamera.gltf");
    // const std::string modelPath("BoxInterleaved/glTF/BoxInterleaved.gltf");
    // const std::string modelPath("FlightHelmet/glTF/FlightHelmet.gltf");
    // const std::string modelPath("Sponza/glTF/Sponza.gltf");
    const std::string modelPath("Buggy/glTF/Buggy.gltf");
    if (!TinyGltfHelper::loadModel(model, assetPath() + "/../_deps/gltfsamplemodels-src/2.0/" + modelPath))
        return;

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

    // Loop through each primitive of each mesh and create a compatible WebGPU pipeline.
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

    // Create a UBO and bind group for the camera. The contents of the camera UBO will be
    // populated in the updateScene() function.
    m_camera.lens().aspectRatio = float(m_window->width()) / float(m_window->height());
    const BufferOptions cameraBufferOptions = {
        .size = 2 * sizeof(glm::mat4),
        .usage = BufferUsageFlags(BufferUsageFlagBits::UniformBufferBit),
        .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
    };
    m_cameraBuffer = m_device.createBuffer(cameraBufferOptions);

    // clang-format off
    const BindGroupOptions cameraBindGroupOptions = {
        .layout = m_cameraBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_cameraBuffer }
        }}
    };
    // clang-format on
    m_cameraBindGroup = m_device.createBindGroup(cameraBindGroupOptions);

    // clang-format off
    m_opaquePassOptions = {
        .colorAttachments = {
            {
                .view = {}, // Not setting the swapchain texture view just yet
                .clearValue = { 0.3f, 0.3f, 0.3f, 1.0f },
                .finalLayout = TextureLayout::PresentSrc
            }
        },
        .depthStencilAttachment = {
            .view = m_depthTextureView,
        }
    };
    // clang-format on
}

Buffer PipelineCaching::createBufferForBufferView(const tinygltf::Model &model,
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
    Buffer buffer = m_device.createBuffer(bufferOptions);

    const tinygltf::Buffer &gltfBuffer = model.buffers.at(gltfBufferView.buffer);

    auto bufferData = static_cast<uint8_t *>(buffer.map());
    std::memcpy(bufferData, gltfBuffer.data.data() + gltfBufferView.byteOffset, gltfBufferView.byteLength);

    buffer.unmap();

    return buffer;
}

void PipelineCaching::setupPrimitive(const tinygltf::Model &model,
                                     const tinygltf::Primitive &primitive,
                                     const PrimitiveKey &primitiveKey,
                                     const PrimitiveInstances &primitiveInstances)
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

    const std::unordered_map<std::string, uint32_t> shaderLocations{ { "POSITION", 0 }, { "NORMAL", 1 } };

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
    Handle<GraphicsPipeline_t> pipelineHandle;
    const GraphicsPipelineKey key = { .topology = TinyGltfHelper::topologyForPrimitiveMode(primitive.mode),
                                      .vertexOptions = std::move(vertexOptions) };

    const auto pipelineIt = m_pipelines.find(key);
    if (pipelineIt == m_pipelines.end()) {
        // Create a pipeline compatible with the above vertex buffer and attribute layout
        // clang-format off
        GraphicsPipelineOptions pipelineOptions = {
            .shaderStages = {
                { .shaderModule = m_vertexShader, .stage = ShaderStageFlagBits::VertexBit },
                { .shaderModule = m_fragmentShader, .stage = ShaderStageFlagBits::FragmentBit }
            },
            .layout = m_pipelineLayout,
            .vertex = key.vertexOptions,
            .renderTargets = {
                { .format = m_swapchainFormat }
            },
            .depthStencil = {
                .format = m_depthFormat,
                .depthWritesEnabled = true,
                .depthCompareOperation = CompareOperation::Less
            },
            .primitive = {
                .topology = key.topology
            }
        };
        // clang-format on
        GraphicsPipeline pipeline = m_device.createGraphicsPipeline(pipelineOptions);
        pipelineHandle = pipeline.handle();
        m_pipelines.insert({ key, std::move(pipeline) });
    } else {
        pipelineHandle = pipelineIt->second.handle();
    }

    // Determine draw type and cache enough information for render time
    const auto &instances = primitiveInstances.at(primitiveKey);
    PrimitiveData primitiveData = {
        .instances = instances,
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

    // Find the pipeline in our draw data or create a new one and append this primitive to it
    // (including all instances)
    auto primitiveIt = m_pipelinePrimitiveMap.find(pipelineHandle);
    if (primitiveIt == m_pipelinePrimitiveMap.end()) {
        m_pipelinePrimitiveMap.insert({ pipelineHandle, { primitiveData } });
    } else {
        primitiveIt->second.push_back(primitiveData);
    }
}

void PipelineCaching::calculateWorldTransforms(const tinygltf::Model &model)
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

void PipelineCaching::setupMeshNode(const tinygltf::Model &model,
                                    const tinygltf::Node &node,
                                    uint32_t nodeIndex,
                                    PrimitiveInstances &primitiveInstances)
{
    // Create a UBO to hold the world transform matrix of the node
    BufferOptions bufferOptions = {
        .size = sizeof(glm::mat4),
        .usage = BufferUsageFlags(BufferUsageFlagBits::UniformBufferBit),
        .memoryUsage = MemoryUsage::CpuToGpu // So we can map it to CPU address space
    };
    Buffer buffer = m_device.createBuffer(bufferOptions);
    auto bufferData = buffer.map();
    std::memcpy(bufferData, glm::value_ptr(m_worldTransforms.at(nodeIndex)), sizeof(glm::mat4));
    buffer.unmap();
    m_worldTransformBuffers.emplace_back(std::move(buffer));

    // Create a bind group for the UBO
    // clang-format off
    BindGroupOptions bindGroupOptions = {
        .layout = m_nodeBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = m_worldTransformBuffers.back()}
        }}
    };
    // clang-format on
    m_worldTransformBindGroups.emplace_back(m_device.createBindGroup(bindGroupOptions));

    // Loop through every primitive of the node's mesh and append this node's transform bind
    // group to the primitive's instance list.
    const tinygltf::Mesh &mesh = model.meshes.at(node.mesh);
    const uint32_t meshIndex = static_cast<uint32_t>(node.mesh);
    uint32_t primitiveIndex = 0;
    for (const auto primitive : mesh.primitives) {
        const PrimitiveKey primitiveKey = { .meshIndex = meshIndex, .primitiveIndex = primitiveIndex };
        const auto instancesIt = primitiveInstances.find(primitiveKey);
        if (instancesIt != primitiveInstances.end()) {
            auto &instances = instancesIt->second;
            instances.push_back(InstanceData{ .nodeTransformBindGroup = m_worldTransformBindGroups.back() });
        } else {
            const std::vector<InstanceData> instances{ InstanceData{ .nodeTransformBindGroup = m_worldTransformBindGroups.back() } };
            primitiveInstances.insert({ primitiveKey, instances });
        }

        ++primitiveIndex;
    }
}

void PipelineCaching::cleanupScene()
{
    m_cameraBindGroup = {};
    m_cameraBuffer = {};
    m_worldTransformBindGroups.clear();
    m_worldTransformBuffers.clear();
    m_pipelines.clear();
    m_vertexShader = {};
    m_fragmentShader = {};
    m_nodeBindGroupLayout = {};
    m_cameraBindGroupLayout = {};
    m_pipelineLayout = {};
    m_buffers.clear();
    m_commandBuffer = {};
}

void PipelineCaching::resize()
{
    m_opaquePassOptions.depthStencilAttachment.view = m_depthTextureView;
}

void PipelineCaching::updateScene()
{
    auto cameraBufferData = static_cast<float *>(m_cameraBuffer.map());
    std::memcpy(cameraBufferData, glm::value_ptr(m_camera.lens().projectionMatrix()), sizeof(glm::mat4));
    std::memcpy(cameraBufferData + 16, glm::value_ptr(m_camera.viewMatrix()), sizeof(glm::mat4));
    m_cameraBuffer.unmap();

    static TimePoint s_lastFpsTimestamp;
    const auto frameEndTime = std::chrono::high_resolution_clock::now();
    const auto timer = std::chrono::duration<double, std::milli>(frameEndTime - s_lastFpsTimestamp).count();
    if (timer > 1000.0) {
        s_lastFpsTimestamp = frameEndTime;

        SPDLOG_INFO("pipelines = {}, setPipelineCount = {}, setVertexBufferCount = {}, setBindGroupCount = {}, drawCount = {}",
                    m_renderStats.pipelineCount, m_renderStats.setPipelineCount, m_renderStats.setVertexBufferCount,
                    m_renderStats.setBindGroupCount, m_renderStats.drawCount);
    }

    m_renderStats.setPipelineCount = 0;
    m_renderStats.setVertexBufferCount = 0;
    m_renderStats.setBindGroupCount = 0;
    m_renderStats.drawCount = 0;
}

void PipelineCaching::render()
{
    auto commandRecorder = m_device.createCommandRecorder();
    m_opaquePassOptions.colorAttachments[0].view = m_swapchainViews.at(m_currentSwapchainImageIndex);
    auto opaquePass = commandRecorder.beginRenderPass(m_opaquePassOptions);

    // Bind the frame (camera bind group) which does not change during the frame
    opaquePass.setBindGroup(0, m_cameraBindGroup, m_pipelineLayout);
    ++m_renderStats.setBindGroupCount;

    for (const auto &[pipeline, primitives] : m_pipelinePrimitiveMap) {
        opaquePass.setPipeline(pipeline);
        ++m_renderStats.setPipelineCount;

        // Iterate over each primitive using this pipeline
        for (const auto &primitiveData : primitives) {
            // Bind the vertex buffers for this primitive
            uint32_t vertexBufferBinding = 0;
            for (const auto &vertexBuffer : primitiveData.vertexBuffers) {
                opaquePass.setVertexBuffer(vertexBufferBinding, vertexBuffer.buffer, vertexBuffer.offset);
                ++m_renderStats.setVertexBufferCount;
                ++vertexBufferBinding;
            }

            // Draw each instance of the primitive using the corresponding transform
            for (const auto &instance : primitiveData.instances) {
                opaquePass.setBindGroup(1, instance.nodeTransformBindGroup);
                ++m_renderStats.setBindGroupCount;

                if (primitiveData.drawType == PrimitiveData::DrawType::NonIndexed) {
                    opaquePass.draw(DrawCommand{ .vertexCount = primitiveData.drawData.vertexCount });
                } else {
                    const IndexedDraw &indexedDraw = primitiveData.drawData.indexedDraw;
                    opaquePass.setIndexBuffer(indexedDraw.indexBuffer, indexedDraw.offset, indexedDraw.indexType);
                    opaquePass.drawIndexed(DrawIndexedCommand{ .indexCount = indexedDraw.indexCount });
                }
                ++m_renderStats.drawCount;
            }
        }
    }

    opaquePass.end();
    m_commandBuffer = commandRecorder.finish();

    SubmitOptions submitOptions = { .commandBuffers = { m_commandBuffer },
                                    .waitSemaphores = { m_presentCompleteSemaphores[m_inFlightIndex] },
                                    .signalSemaphores = { m_renderCompleteSemaphores[m_inFlightIndex] } };
    m_queue.submit(submitOptions);
}
