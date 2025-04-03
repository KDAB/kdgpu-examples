#include "gltf_holder.h"

#include <GltfHolder/gltf_holder_global.h>

#include <tinygltf_helper/tinygltf_helper.h>
#include <KDGpu/device.h>

#include <GltfHolder/buffer_view_helper/buffer_view_helper.h>

#include <GltfHolder/render_permutation/gltf_render_permutation.h>

#include <KDGpu/graphics_pipeline_options.h>
#include <glm/gtc/type_ptr.hpp>
#include <global_resources.h>

using namespace KDGpu;
namespace kdgpu_ext::gltf_holder {

using namespace render_mesh_set;
using namespace shader_specification;

void GltfHolder::load(const std::string &filename, KDGpu::Queue& queue)
{
    if (!TinyGltfHelper::loadModel(m_model, filename))
        return;

    // Interrogate the model to see which usage flag we need for each buffer.
    // E.g. vertex buffer or index buffer. This is needed to then create suitable
    // buffers in the next step.
    std::vector<BufferUsageFlags> bufferViewUsages(m_model.bufferViews.size());
    auto markAccessorUsage = [&bufferViewUsages, this](int accessorIndex, KDGpu::BufferUsageFlagBits flag) {
        const auto &accessor = m_model.accessors.at(accessorIndex);
        bufferViewUsages[accessor.bufferView] |= KDGpu::BufferUsageFlags(flag);
    };

    for (const auto &mesh : m_model.meshes) {
        for (const auto &primitive : mesh.primitives) {
            if (primitive.indices != -1)
                markAccessorUsage(primitive.indices, KDGpu::BufferUsageFlagBits::IndexBufferBit);

            for (const auto &[attributeName, accessorIndex] : primitive.attributes)
                markAccessorUsage(accessorIndex, KDGpu::BufferUsageFlagBits::VertexBufferBit);
        }
    }

    // Create buffers to hold the vertex data
    {
        const uint32_t bufferViewCount = m_model.bufferViews.size();
        m_buffers.reserve(bufferViewCount);
        for (uint32_t bufferViewIndex = 0; bufferViewIndex < bufferViewCount; ++bufferViewIndex) {
            auto& bufferViewUsage = bufferViewUsages[bufferViewIndex];

            // when the buffer is not to be used, add an empty one to not mess up the indices
            if (bufferViewUsage.toInt() == 0) {
                m_buffers.push_back({});
                continue;
            }

            // create a VRAM buffer for the gltf buffer
            {
                auto& bufferView = m_model.bufferViews.at(bufferViewIndex);
                m_buffers.emplace_back(createBufferForBufferView(m_model, bufferViewUsage, bufferView));
            }
        }
    }

    // Find every node with a mesh and create a bind group containing the node's transform.
    uint32_t nodeIndex = 0;
    for (const auto &node : m_model.nodes) {
        if (node.mesh != -1) {
            NodeRenderTask new_node_data;
            new_node_data.nodeIndex = nodeIndex;
            new_node_data.meshIndex = static_cast<uint32_t>(node.mesh);
            new_node_data.transformUniformBufferObject.init(
                    m_nodeTransformUniformBinding,
                    GltfHolderGlobal::instance().nodeTransformBindGroupLayout());
            m_nodeRenderTasks.push_back(std::move(new_node_data));
        }
        ++nodeIndex;
    }

    // Calculate the world transforms of the node tree
    calculateWorldTransforms();

    // Load textures
    m_textures.reserve(m_model.images.size());
    for (auto& image : m_model.images) {
        GltfTexture texture;
        texture.initialize(image, queue);
        m_textures.push_back(std::move(texture));
    }
}

void GltfHolder::deinitialize()
{
    m_nodeRenderTasks.clear();
    m_buffers.clear();
    m_textures.clear();
}

void GltfHolder::setNodeTransformShaderBinding(size_t nodeTransformUniformBinding)
{
    m_nodeTransformUniformBinding = nodeTransformUniformBinding;
}

void GltfHolder::createGraphicsRenderingPipelinesForMeshSet(
        RenderMeshSet &renderMeshSet,

        // target texture info
        const RenderTarget &renderTarget,

        // shader info
        std::vector<ShaderStage> &shaderStages,
        const GltfShaderVertexInput& shaderVertexInput,
        PipelineLayout &pipelineLayout)
{
    // Loop through each primitive of each mesh and create pipelines
    uint32_t index = 0;
    for (const auto &mesh : m_model.meshes) {
        MeshPrimitives meshPrimitives;
        for (const auto &primitive : mesh.primitives) {
            auto primitive_data = setupPrimitive(
                    shaderStages,
                    shaderVertexInput,
                    renderMeshSet.graphicsPipelines,
                    pipelineLayout,
                    renderTarget,
                    primitive);
            renderMeshSet.primitiveData.push_back(primitive_data);
            meshPrimitives.primitiveIndices.push_back(index++);
        }
        renderMeshSet.primitives.push_back(meshPrimitives);
    }
}

void GltfHolder::update()
{
    for (auto& texture: m_textures)
        texture.update();
}

void GltfHolder::renderAllNodes(
    RenderPassCommandRecorder &renderPassCommandRecorder,
    GltfRenderPermutation& renderPermutation,
    const PipelineLayout& pipelineLayout)
{
    for (const auto &render_task : m_nodeRenderTasks) {
        // Set the bind group for the world transform of this node
        // The group index (descriptor set index) comes from the render permutation
        renderPassCommandRecorder.setBindGroup(renderPermutation.nodeTransformUniformSet, render_task.transformUniformBufferObject.bindGroup(), pipelineLayout);

        const auto &meshData = renderPermutation.meshSet.primitives.at(render_task.meshIndex);
        for (const auto &primitiveIndex : meshData.primitiveIndices) {
            const auto &primitiveData = renderPermutation.meshSet.primitiveData.at(primitiveIndex);

            // if the shading material requests material at all (render-to-depth does not for instance)
            // a unique configuration for a shader in a pass
            if (renderPermutation.materials.has_shader_material()) {
                auto& shading_material = renderPermutation.materials.get_shader_material();
                if (shading_material.anyTextureChannelsFromGltfEnabled) {
                    // the mesh dictates what material to use
                    if (primitiveData.materialIndex.has_value()) {
                        auto &material = renderPermutation.materials.materialByIndex(primitiveData.materialIndex.value());
                        auto &pass_material_bind_group = material.bindGroup();
                        if (material.areAllTexturesValidForUse())
                            renderPassCommandRecorder.setBindGroup(shading_material.bindGroup, pass_material_bind_group, pipelineLayout);
                    }
                }
            }

            // Set the pipeline for this primitive
            renderPassCommandRecorder.setPipeline(primitiveData.pipeline);

            // Bind the vertex buffers
            uint32_t vertexBufferBinding = 0;
            for (const auto &vertexBuffer : primitiveData.vertexBuffers) {
                renderPassCommandRecorder.setVertexBuffer(vertexBufferBinding, vertexBuffer.buffer, vertexBuffer.offset);
                ++vertexBufferBinding;
            }

            // draw non-indexed
            if (primitiveData.drawType == PrimitiveData::DrawType::NonIndexed) {
                renderPassCommandRecorder.draw(DrawCommand{ .vertexCount = primitiveData.drawData.vertexCount });
                continue;
            }

            // draw indexed
            const IndexedDraw &indexedDraw = primitiveData.drawData.indexedDraw;
            renderPassCommandRecorder.setIndexBuffer(indexedDraw.indexBuffer,
                                                     indexedDraw.offset,
                                                     indexedDraw.indexType);
            renderPassCommandRecorder.drawIndexed(DrawIndexedCommand{ .indexCount = indexedDraw.indexCount });
        }
    }
}

PrimitiveData GltfHolder::setupPrimitive(
        std::vector<ShaderStage> &shaderStages,
        const GltfShaderVertexInput& shaderVertexInput,
        std::vector<GraphicsPipeline> &pipelines,
        PipelineLayout &pipelineLayout,
        const RenderTarget &renderTarget,
        const tinygltf::Primitive &primitive)
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

    // Iterate over each attribute in the primitive to build up a description of the
    // vertex layout needed to create the pipeline.
    for (const auto &attribute : primitive.attributes) {
        const auto &accessor = m_model.accessors.at(attribute.second);
        const uint32_t bufferIndex = accessor.bufferView;
        const auto &bufferView = m_model.bufferViews.at(accessor.bufferView);

        // Find the [shader vertex input location] for this attribute (if any)
        std::optional<uint32_t> possible_location;
        {
            if (attribute.first == "POSITION") {
                if (shaderVertexInput.positionLocation.has_value())
                    possible_location = shaderVertexInput.positionLocation.value();
            }
            if (attribute.first == "NORMAL") {
                if (shaderVertexInput.normalLocation.has_value())
                    possible_location = shaderVertexInput.normalLocation.value();
            }
            if (attribute.first == "TEXCOORD_0") {
                if (shaderVertexInput.textureCoord0Location.has_value())
                    possible_location = shaderVertexInput.textureCoord0Location.value();
            }
            if (attribute.first == "TANGENT") {
                if (shaderVertexInput.tangentLocation.has_value())
                    possible_location = shaderVertexInput.tangentLocation.value();
            }

            if (!possible_location.has_value())
                continue;
        }

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
        const VertexAttribute vertexAttribute = { .location = possible_location.value(),
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

    // Create pipeline for rendering
    {
        // Create a pipeline compatible with the above vertex buffer and attribute layout
        GraphicsPipelineOptions regularPipelineOptions = {
            .shaderStages = shaderStages,
            .layout = pipelineLayout,
            .vertex = vertexOptions,
            .primitive = {
                    .topology = TinyGltfHelper::topologyForPrimitiveMode(primitive.mode) }
        };

        // if render target has color target
        if (renderTarget.hasColorTarget()) {
            auto possibleRenderTargetOptions = renderTarget.colorTarget()->renderTargetOptions();
            if (possibleRenderTargetOptions)
                regularPipelineOptions.renderTargets = {
                    possibleRenderTargetOptions.value()
                };
        }

        if (renderTarget.hasDepthTarget()) {
            auto possibleDepthStencilOptions = renderTarget.depthTarget()->depthStencilOptions();
            if (possibleDepthStencilOptions)
                regularPipelineOptions.depthStencil = {
                    .format = renderTarget.depthTarget()->format(),
                    .depthWritesEnabled = renderTarget.depthWriteEnabled,
                    .depthCompareOperation = renderTarget.depthCompareOperation
                };
        }

        auto& device = kdgpu_ext::graphics::GlobalResources::instance().graphicsDevice();

        GraphicsPipeline pipeline = device.createGraphicsPipeline(regularPipelineOptions);
        pipelines.emplace_back(std::move(pipeline));
    }

    // Determine draw type and cache enough information for render time
    if (primitive.indices == -1) {
        return PrimitiveData{
            .pipeline = pipelines.back(),
            .vertexBuffers = buffers,
            .drawType = PrimitiveData::DrawType::NonIndexed,
            .drawData = { .vertexCount = vertexCount }
        };
    }

    const auto &accessor = m_model.accessors.at(primitive.indices);
    const IndexedDraw indexedDraw = {
        .indexBuffer = m_buffers.at(accessor.bufferView),
        .offset = accessor.byteOffset,
        .indexCount = static_cast<uint32_t>(accessor.count),
        .indexType = TinyGltfHelper::indexTypeForComponentType(accessor.componentType)
    };

    // when primitive has no material at all
    if (primitive.material == -1) {
        return PrimitiveData{
            .pipeline = pipelines.back(),
            .vertexBuffers = buffers,
            .drawType = PrimitiveData::DrawType::Indexed,
            .drawData = { .indexedDraw = indexedDraw }
        };
    }

    return PrimitiveData{
        .pipeline = pipelines.back(),
        // .depthOnlyPipeline = m_depthOnlyPipelines.back(),
        .vertexBuffers = buffers,
        .materialIndex = primitive.material,
        .drawType = PrimitiveData::DrawType::Indexed,
        .drawData = { .indexedDraw = indexedDraw }
    };
}

void GltfHolder::calculateWorldTransforms()
{
    std::vector<bool> visited(m_model.nodes.size());

    // Starting at the root node of each scene in the gltf file, traverse the node
    // tree and recursively calculate the world transform of each node. The results are
    // stored in the m_worldTransforms with indices matching those of the glTF nodes vector.
    // clang-format off
    for (const auto &scene : m_model.scenes) {
        for (const auto &rootNodeIndex : scene.nodes) {
            TinyGltfHelper::calculateNodeTreeWorldTransforms(
                m_model,              // The gltf model is passed in to lookup other data
                rootNodeIndex,      // The root node of the current scene
                glm::dmat4(1.0f),   // Initial transform of root is the identity matrix
                visited,            // To know which nodes we have already calculated
                m_nodeRenderTasks); // The vector of node transforms to calculate. Same indices as model.nodes
        }
    }
    // clang-format on
}
}
