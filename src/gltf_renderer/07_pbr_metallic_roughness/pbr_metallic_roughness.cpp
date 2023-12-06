#include "pbr_metallic_roughness.h"

#include <KDGpuExample/engine.h>
#include <KDGpuExample/kdgpuexample.h>

#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/vulkan/vulkan_enums.h>

#include <KDUtils/file.h>

#include <tinygltf_helper/tinygltf_helper.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <ktx.h>
#include <ktxvulkan.h>

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <fstream>
#include <string>

namespace {

inline std::string gltfModelPath()
{
#if defined(GLTF_RENDERER_MODEL_PATH)
    return GLTF_RENDERER_MODEL_PATH;
#else
    return "";
#endif
}

inline std::string assetPath()
{
#if defined(GLTF_RENDERER_ASSET_PATH)
    return GLTF_RENDERER_ASSET_PATH;
#else
    return "";
#endif
}

ktxResult loadKtxFile(const std::string &filename, ktxTexture **texture)
{
    ktxResult result = KTX_SUCCESS;
    KDUtils::File file(filename);
    if (!file.exists()) {
        SPDLOG_CRITICAL("Failed to load file {}. File does not exist", filename);
        exit(1);
    }
    result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, texture);
    return result;
}

} // namespace

PbrMetallicRoughness::PbrMetallicRoughness()
    : KDGpuExample::SimpleExampleEngineLayer()
{
    m_samples = SampleCountFlagBits::Samples8Bit;
}

void PbrMetallicRoughness::initializeScene()
{
    // Create our multisample render target
    createRenderTarget();
    m_opaqueWhite = createSolidColorTexture(1.0f, 1.0f, 1.0f, 1.0f);
    m_transparentBlack = createSolidColorTexture(0.0f, 0.0f, 0.0f, 0.0f);
    m_defaultNormal = createSolidColorTexture(0.5f, 0.5f, 1.0f, 1.0f);
    m_defaultSampler = m_device.createSampler();

    m_environmentLightSpecular = createTextureFromKtxFile(assetPath() + "/textures/footprint_court/specular.ktx2");
    m_environmentLightDiffuse = createTextureFromKtxFile(assetPath() + "/textures/footprint_court/diffuse.ktx2");
    m_lutGGX = createTextureFromKtxFile(assetPath() + "/textures/lut_ggx.ktx2");

    // Create bind group layout consisting of a single binding holding a UBO for the camera
    // clang-format off
    const BindGroupLayoutOptions cameraBindGroupLayoutOptions = {
        .bindings = {{
            .binding = 0,
            .resourceType = ResourceBindingType::UniformBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit | ShaderStageFlagBits::FragmentBit)
        }}
    };
    // clang-format on
    m_cameraBindGroupLayout = m_device.createBindGroupLayout(cameraBindGroupLayoutOptions);

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
    m_nodeBindGroupLayout = m_device.createBindGroupLayout(nodeBindGroupLayoutOptions);

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
        },{
            .binding = 2,
            .resourceType = ResourceBindingType::CombinedImageSampler,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
        },{
            .binding = 3,
            .resourceType = ResourceBindingType::CombinedImageSampler,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
        },{
            .binding = 4,
            .resourceType = ResourceBindingType::CombinedImageSampler,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
        },{
            .binding = 5,
            .resourceType = ResourceBindingType::CombinedImageSampler,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
        }}
    };
    // clang-format on
    m_materialBindGroupLayout = m_device.createBindGroupLayout(materialBindGroupLayoutOptions);

    // Create a bind group layout for the material data
    // clang-format off
    const BindGroupLayoutOptions environmentLightBindGroupLayoutOptions = {
        .bindings = {{
                .binding = 0,
                .resourceType = ResourceBindingType::CombinedImageSampler,
                .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
            },{
                .binding = 1,
                .resourceType = ResourceBindingType::CombinedImageSampler,
                .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
            },{
                .binding = 2,
                .resourceType = ResourceBindingType::CombinedImageSampler,
                .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
            }
        }
    };
    // clang-format on
    m_environmentLightBindGroupLayout = m_device.createBindGroupLayout(environmentLightBindGroupLayoutOptions);

    // Create a pipeline layout (array of bind group layouts)
    // clang-format off
    const PipelineLayoutOptions pipelineLayoutOptions = {
        .bindGroupLayouts = {
            m_cameraBindGroupLayout,    // Set 0
            m_nodeBindGroupLayout,      // Set 1
            m_materialBindGroupLayout,  // Set 2
            m_environmentLightBindGroupLayout // Set 3
        }
    };
    // clang-format on
    m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutOptions);

    // Load the model
    tinygltf::Model model;
    // const std::string modelPath("AntiqueCamera/glTF/AntiqueCamera.gltf");
    // const std::string modelPath("BoxInterleaved/glTF/BoxInterleaved.gltf");
    const std::string modelPath("FlightHelmet/glTF/FlightHelmet.gltf");
    // const std::string modelPath("Sponza/glTF/Sponza.gltf");
    // const std::string modelPath("Buggy/glTF/Buggy.gltf");
    if (!TinyGltfHelper::loadModel(model, gltfModelPath() + modelPath))
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
    m_instanceTransformsBuffer = m_device.createBuffer(bufferOptions);

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
    m_instanceTransformsBindGroup = m_device.createBindGroup(bindGroupOptions);

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
    const BindGroupOptions environmentLightBindGroupOptions = {
        .layout = m_environmentLightBindGroupLayout,
        .resources = {{
                          .binding = 0,
                          .resource = TextureViewSamplerBinding{
                              .textureView = m_environmentLightDiffuse.textureView,
                              .sampler = m_defaultSampler
                          }
                      }, {
                          .binding = 1,
                          .resource = TextureViewSamplerBinding{
                              .textureView = m_environmentLightSpecular.textureView,
                              .sampler = m_defaultSampler
                          }
                      }, {
                          .binding = 2,
                          .resource = TextureViewSamplerBinding{
                              .textureView = m_lutGGX.textureView,
                              .sampler = m_defaultSampler
                          }
                      }}
    };
    // clang-format on
    m_environmentLightBindGroup = m_device.createBindGroup(environmentLightBindGroupOptions);

    // clang-format off
    m_opaquePassOptions = {
        .colorAttachments = {
            {
                .view = m_msaaTextureView,
                .resolveView = {}, // Not setting the swapchain texture view just yet
                .clearValue = { 0.3f, 0.3f, 0.3f, 1.0f },
                .finalLayout = TextureLayout::PresentSrc
            }
        },
        .depthStencilAttachment = {
            .view = m_depthTextureView,
        },
        .samples = m_samples.get()
    };
    // clang-format on
}

Buffer PbrMetallicRoughness::createBufferForBufferView(const tinygltf::Model &model,
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

TextureAndView PbrMetallicRoughness::createTextureForImage(const tinygltf::Model &model, uint32_t textureIndex)
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
    textureAndView.texture = m_device.createTexture(textureOptions);

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

TextureAndView PbrMetallicRoughness::createTextureFromKtxFile(const std::string &filename)
{
    ktxTexture *ktxTexture{ nullptr };
    ktxResult result = loadKtxFile(filename, &ktxTexture);

    ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
    ktx_size_t ktxTextureSize = ktxTexture->dataSize;
    const auto format = ktxTexture_GetVkFormat(ktxTexture);
    const TextureOptions options = {
        .type = ktxTexture->isCubemap ? TextureType::TextureTypeCube : TextureType::TextureType2D,
        .format = vkFormatToFormat(format),
        .extent = { .width = ktxTexture->baseWidth, .height = ktxTexture->baseHeight, .depth = 1 },
        .mipLevels = ktxTexture->numLevels,
        .arrayLayers = ktxTexture->numFaces,
        .usage = TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly,
        .initialLayout = TextureLayout::Undefined
    };

    const TextureViewOptions viewOptions = {
        .viewType = ktxTexture->isCubemap ? KDGpu::ViewType::ViewTypeCube : KDGpu::ViewType::ViewType2D,
        .format = vkFormatToFormat(format),
        .range = TextureSubresourceRange{.aspectMask = TextureAspectFlagBits::ColorBit,
                                         .baseMipLevel = 0,
                                         .levelCount = ktxTexture->numLevels,
                                         .baseArrayLayer = 0,
                                         .layerCount = ktxTexture->numFaces}
    };

    TextureAndView textureAndView{.texture = m_device.createTexture(options)};
    textureAndView.textureView = textureAndView.texture.createView(viewOptions);

    // Upload the texture data and transition to ShaderReadOnlyOptimal
    std::vector<BufferTextureCopyRegion> regions;
    regions.reserve(ktxTexture->numFaces * ktxTexture->numLevels);
    for (uint32_t face = 0; face < ktxTexture->numFaces; face++) {
        for (uint32_t level = 0; level < ktxTexture->numLevels; level++) {
            ktx_size_t offset{};
            KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
            assert(result == KTX_SUCCESS);
            // clang-format off
            const BufferTextureCopyRegion region = {
                .bufferOffset = offset,
                .textureSubResource = {
                    .aspectMask = TextureAspectFlagBits::ColorBit,
                    .mipLevel = level,
                    .baseArrayLayer = face,
                    .layerCount = 1,
                },
                .textureExtent = {
                    .width = std::max(1u, ktxTexture->baseWidth >> level),
                    .height = std::max(1u, ktxTexture->baseHeight >> level),
                    .depth = 1
                }
            };
            // clang-format on
            regions.push_back(region);
        }
    }

    // clang-format off
    const TextureUploadOptions uploadOptions = {
        .destinationTexture = textureAndView.texture,
        .dstStages = PipelineStageFlagBit::AllGraphicsBit,
        .dstMask = AccessFlagBit::MemoryReadBit,
        .data = ktxTextureData,
        .byteSize = ktxTextureSize,
        .oldLayout = TextureLayout::Undefined,
        .newLayout = TextureLayout::ShaderReadOnlyOptimal,
        .regions = regions
    };
    // clang-format on
    uploadTextureData(uploadOptions);

    return textureAndView;
}

Sampler PbrMetallicRoughness::createSampler(const tinygltf::Model &model, uint32_t samplerIndex)
{
    const tinygltf::Sampler &gltfSampler = model.samplers.at(samplerIndex);
    SamplerOptions samplerOptions = {
        .magFilter = TinyGltfHelper::filterModeForSamplerFilter(gltfSampler.magFilter),
        .minFilter = TinyGltfHelper::filterModeForSamplerFilter(gltfSampler.minFilter),
        .mipmapFilter = TinyGltfHelper::mipmapFilterModeForSamplerFilter(gltfSampler.minFilter),
        .u = TinyGltfHelper::addressModeForSamplerAddressMode(gltfSampler.wrapS),
        .v = TinyGltfHelper::addressModeForSamplerAddressMode(gltfSampler.wrapT)
    };
    Sampler sampler = m_device.createSampler(samplerOptions);
    return sampler;
}

TextureViewAndSampler PbrMetallicRoughness::createViewSamplerForTexture(const tinygltf::Model &model, uint32_t viewSamplerIndex)
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
TextureAndView PbrMetallicRoughness::createSolidColorTexture(float r, float g, float b, float a)
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
    textureAndView.texture = m_device.createTexture(textureOptions);

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

void PbrMetallicRoughness::setupMaterial(const tinygltf::Model &model, uint32_t materialIndex)
{
    const tinygltf::Material &material = model.materials.at(materialIndex);

    // Create a buffer and populate it with the material data. Just the base color factor
    // and alpha cutoff will be implemented here for simplicity.
    const BufferOptions bufferOptions = {
        .size = 11 * sizeof(float),
        .usage = BufferUsageFlagBits::UniformBufferBit | BufferUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly
    };
    Buffer buffer = m_device.createBuffer(bufferOptions);

    const auto &pbr = material.pbrMetallicRoughness;
    // clang-format off
    MaterialData materialData = {
        .baseColorFactor = {
            static_cast<float>(pbr.baseColorFactor[0]),
            static_cast<float>(pbr.baseColorFactor[1]),
            static_cast<float>(pbr.baseColorFactor[2]),
            static_cast<float>(pbr.baseColorFactor[3])
        },
        .emissiveFactor = glm::make_vec4(material.emissiveFactor.data()),
        .metallicFactor = static_cast<float>(pbr.metallicFactor),
        .roughnessFactor = static_cast<float>(pbr.roughnessFactor),
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
    const int baseColorTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
    const int metallicRoughnessTextureIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
    const int normalTextureIndex = material.normalTexture.index;
    const int occlusionTextureIndex = material.occlusionTexture.index;
    const int emissiveTextureIndex = material.emissiveTexture.index;
    const BindGroupOptions bindGroupOptions = {
        .layout = m_materialBindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = UniformBufferBinding{ .buffer = buffer }
        }, {
            .binding = 1,
            .resource = TextureViewSamplerBinding{
                .textureView = (baseColorTextureIndex != -1) ? m_viewSamplers.at(baseColorTextureIndex).view : m_opaqueWhite.textureView,
                .sampler = (baseColorTextureIndex != -1) ? m_viewSamplers.at(baseColorTextureIndex).sampler : m_defaultSampler
            }
        }, {
            .binding = 2,
            .resource = TextureViewSamplerBinding{
                .textureView = (metallicRoughnessTextureIndex != -1) ? m_viewSamplers.at(metallicRoughnessTextureIndex).view : m_opaqueWhite.textureView,
                .sampler = (metallicRoughnessTextureIndex != -1) ? m_viewSamplers.at(metallicRoughnessTextureIndex).sampler : m_defaultSampler
            }
        }, {
            .binding = 3,
            .resource = TextureViewSamplerBinding{
                .textureView = (normalTextureIndex != -1) ? m_viewSamplers.at(normalTextureIndex).view : m_defaultNormal.textureView,
                .sampler = (normalTextureIndex != -1) ? m_viewSamplers.at(normalTextureIndex).sampler : m_defaultSampler
            }
        }, {
            .binding = 4,
            .resource = TextureViewSamplerBinding{
                .textureView = (occlusionTextureIndex != -1) ? m_viewSamplers.at(occlusionTextureIndex).view : m_opaqueWhite.textureView,
                .sampler = (occlusionTextureIndex != -1) ? m_viewSamplers.at(occlusionTextureIndex).sampler : m_defaultSampler
            }
        }, {
            .binding = 5,
            .resource = TextureViewSamplerBinding{
                .textureView = (emissiveTextureIndex != -1) ? m_viewSamplers.at(emissiveTextureIndex).view : m_transparentBlack.textureView,
                .sampler = (emissiveTextureIndex != -1) ? m_viewSamplers.at(emissiveTextureIndex).sampler : m_defaultSampler
            }
        }}
    };
    // clang-format on
    BindGroup bindGroup = m_device.createBindGroup(bindGroupOptions);

    // Store the buffer and bind group to keep them alive
    m_materialBuffers.emplace_back(std::move(buffer));
    m_materialBindGroups.emplace_back(std::move(bindGroup));
}

void PbrMetallicRoughness::setupPrimitive(const tinygltf::Model &model,
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

    const std::unordered_map<std::string, uint32_t> shaderLocations{ { "POSITION", 0 }, { "NORMAL", 1 }, { "TEXCOORD_0", 2 } , { "TANGENT", 3 }};
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
                .format = m_swapchainFormat,
                .blending = key.alphaMode == TinyGltfHelper::AlphaMode::Blend ? alphaBlendOptions : opaqueBlendOptions
            }},
            .depthStencil = {
                .format = m_depthFormat,
                .depthWritesEnabled = true,
                .depthCompareOperation = CompareOperation::Less
            },
            .primitive = {
                .topology = key.topology,
                .cullMode = key.doubleSided ? CullModeFlagBits::None : CullModeFlagBits::BackBit
            },
            .multisample = {
                .samples = m_samples.get()
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

Handle<ShaderModule_t> PbrMetallicRoughness::findOrCreateShaderModule(const ShaderModuleKey &key)
{
    const auto shaderModuleIt = m_shaderModules.find(key);
    if (shaderModuleIt != m_shaderModules.end()) {
        return shaderModuleIt->second.handle();
    } else {
        const auto shaderPath = assetPath() + "/shaders/07_pbr_metallic_roughness/" + shaderFilename(key.shaderOptionsKey, key.stage);
        ShaderModule shader = m_device.createShaderModule(KDGpuExample::readShaderFile(shaderPath));
        const auto shaderHandle = shader.handle();
        m_shaderModules.insert({ key, std::move(shader) });
        return shaderHandle;
    }
}

// TODO: Should we make this general and use the configuration file used to generate the shader
// at configure/build time?
std::string PbrMetallicRoughness::shaderFilename(const PipelineShaderOptionsKey &key, ShaderStageFlagBits stage)
{
    std::string name = "pbr_metallic_roughness";
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

InstancedDraw PbrMetallicRoughness::setupPrimitiveInstances(const PrimitiveKey &primitiveKey,
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

void PbrMetallicRoughness::calculateWorldTransforms(const tinygltf::Model &model)
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

void PbrMetallicRoughness::setupMeshNode(const tinygltf::Model &model,
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

void PbrMetallicRoughness::cleanupScene()
{
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
    m_environmentLightBindGroup = {};
    m_environmentLightSpecular = {};
    m_environmentLightDiffuse = {};
    m_lutGGX = {};
    m_instanceTransformsBindGroup = {};
    m_instanceTransformsBuffer = {};
    m_pipelines.clear();
    m_shaderModules.clear();
    m_materialBindGroupLayout = {};
    m_nodeBindGroupLayout = {};
    m_cameraBindGroupLayout = {};
    m_environmentLightBindGroupLayout = {};
    m_pipelineLayout = {};
    m_viewSamplers.clear();
    m_samplers.clear();
    m_textures.clear();
    m_buffers.clear();
    m_commandBuffer = {};
}

void PbrMetallicRoughness::createRenderTarget()
{
    const TextureOptions options = {
        .type = TextureType::TextureType2D,
        .format = m_swapchainFormat,
        .extent = { .width = m_window->width(), .height = m_window->height(), .depth = 1 },
        .mipLevels = 1,
        .samples = m_samples.get(),
        .usage = TextureUsageFlagBits::ColorAttachmentBit,
        .memoryUsage = MemoryUsage::GpuOnly,
        .initialLayout = TextureLayout::Undefined
    };
    m_msaaTexture = m_device.createTexture(options);
    m_msaaTextureView = m_msaaTexture.createView();
}

void PbrMetallicRoughness::resize()
{
    createRenderTarget();
    m_opaquePassOptions.colorAttachments[0].view = m_msaaTextureView;
    m_opaquePassOptions.depthStencilAttachment.view = m_depthTextureView;
}

void PbrMetallicRoughness::updateScene()
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

void PbrMetallicRoughness::render()
{
    auto commandRecorder = m_device.createCommandRecorder();
    m_opaquePassOptions.colorAttachments[0].resolveView = m_swapchainViews.at(m_currentSwapchainImageIndex);
    auto opaquePass = commandRecorder.beginRenderPass(m_opaquePassOptions);

    // Bind the frame (camera bind group) which does not change during the frame
    opaquePass.setBindGroup(0, m_cameraBindGroup, m_pipelineLayout);
    opaquePass.setBindGroup(1, m_instanceTransformsBindGroup, m_pipelineLayout);
    opaquePass.setBindGroup(3, m_environmentLightBindGroup, m_pipelineLayout);
    m_renderStats.setBindGroupCount+= 3;

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

    renderImGuiOverlay(&opaquePass);

    opaquePass.end();
    m_commandBuffer = commandRecorder.finish();

    SubmitOptions submitOptions = { .commandBuffers = { m_commandBuffer },
                                    .waitSemaphores = { m_presentCompleteSemaphores[m_inFlightIndex] },
                                    .signalSemaphores = { m_renderCompleteSemaphores[m_inFlightIndex] } };
    m_queue.submit(submitOptions);
}
