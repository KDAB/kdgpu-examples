#include "single_texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <ktx.h>
#include <ktxvulkan.h>

#include <KDGpu/texture_options.h>
#include <global_resources.h>
#include <KDGpu/vulkan/vulkan_enums.h>

using namespace KDGpu;

namespace {
kdgpu_ext::graphics::texture::internal::ImageData loadImage(const KDUtils::File &file)
{
    kdgpu_ext::graphics::texture::internal::ImageData result;
    int channel_count;
    int width;
    int height;

    if (!file.isOpen()) {
        const_cast<KDUtils::File*>(&file)->open(std::ios::in | std::ios::binary);
    }

    auto bytes = const_cast<KDUtils::File*>(&file)->readAll();

    result.pixelData = stbi_load_from_memory(bytes.data(), bytes.size(), &width, &height, &channel_count, STBI_rgb_alpha);

    if (result.pixelData == nullptr) {
        fprintf(stderr, "Failed to load texture %hs %hs", file.path().c_str(), stbi_failure_reason());
        return result;
    }

    if (channel_count == 3)
        result.format = KDGpu::Format::R8G8B8_UNORM;
    if (channel_count == 4)
        result.format = KDGpu::Format::R8G8B8A8_UNORM;

    result.width = static_cast<uint32_t>(width);
    result.height = static_cast<uint32_t>(height);
    result.byteSize = channel_count * static_cast<DeviceSize>(width) * static_cast<DeviceSize>(height);
    return result;
}

} // namespace

namespace kdgpu_ext::graphics::texture {
void SingleTexture::uploadImage(KDGpuExample::ExampleEngineLayer &engine, internal::ImageData image)
{
    const TextureOptions textureOptions = {
        .type = TextureType::TextureType2D,
        .format = image.format,
        .extent = { .width = image.width, .height = image.height, .depth = 1 },
        .mipLevels = 1,
        .usage = TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly,
        .initialLayout = TextureLayout::Undefined
    };
    m_texture = GlobalResources::instance().graphicsDevice().createTexture(textureOptions);
    // Upload the texture data and transition to ShaderReadOnlyOptimal
    const std::vector<BufferTextureCopyRegion> regions = { { .textureSubResource = { .aspectMask = TextureAspectFlagBits::ColorBit },
                                                             .textureExtent = { .width = image.width, .height = image.height, .depth = 1 } } };
    const TextureUploadOptions uploadOptions = {
        .destinationTexture = m_texture,
        .dstStages = PipelineStageFlagBit::AllGraphicsBit,
        .dstMask = AccessFlagBit::MemoryReadBit,
        .data = image.pixelData,
        .byteSize = image.byteSize,
        .oldLayout = TextureLayout::Undefined,
        .newLayout = TextureLayout::ShaderReadOnlyOptimal,
        .regions = regions
    };

    // TODO: this could benefit from using an uploader decoupled from the example class
    engine.uploadTextureData(uploadOptions);

    // Create a view and sampler
    m_textureView = m_texture.createView();
    m_sampler = GlobalResources::instance().graphicsDevice().createSampler();
}

void SingleTexture::load(KDGpuExample::ExampleEngineLayer &engine, const KDUtils::File &file)
{
    auto image = loadImage(file);
    uploadImage(engine, image);
}

void SingleTexture::loadKtx(KDGpuExample::ExampleEngineLayer &engine, const KDUtils::File &file)
{
    ktxTexture *texture{ nullptr };
    ktxResult result = KTX_SUCCESS;
    if (!file.isOpen()) {
        const_cast<KDUtils::File*>(&file)->open(std::ios::in | std::ios::binary);
    }

    auto bytes = const_cast<KDUtils::File*>(&file)->readAll();

    result = ktxTexture_CreateFromMemory(bytes.data(), bytes.size(),KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
    ktx_uint8_t *ktxTextureData = ktxTexture_GetData(texture);
    ktx_size_t ktxTextureSize = texture->dataSize;
    const auto format = ktxTexture_GetVkFormat(texture);

    const TextureOptions options = {
        .type = texture->isCubemap ? TextureType::TextureTypeCube : TextureType::TextureType2D,
        .format = KDGpu::vkFormatToFormat(format),
        .extent = { .width = texture->baseWidth, .height = texture->baseHeight, .depth = 1 },
        .mipLevels = texture->numLevels,
        .arrayLayers = texture->numFaces,
        .usage = TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly,
        .initialLayout = TextureLayout::Undefined
    };
    const TextureViewOptions viewOptions = {
        .viewType = texture->isCubemap ? KDGpu::ViewType::ViewTypeCube : KDGpu::ViewType::ViewType2D,
        .format = vkFormatToFormat(format),
        .range = TextureSubresourceRange{ .aspectMask = TextureAspectFlagBits::ColorBit,
                                          .baseMipLevel = 0,
                                          .levelCount = texture->numLevels,
                                          .baseArrayLayer = 0,
                                          .layerCount = texture->numFaces }
    };
    m_texture = GlobalResources::instance().graphicsDevice().createTexture(options);

    std::vector<BufferTextureCopyRegion> regions;
    regions.reserve(texture->numFaces * texture->numLevels);
    for (uint32_t face = 0; face < texture->numFaces; face++) {
        for (uint32_t level = 0; level < texture->numLevels; level++) {
            ktx_size_t offset{};
            KTX_error_code result = ktxTexture_GetImageOffset(texture, level, 0, face, &offset);
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
                    .width = std::max(1u, texture->baseWidth >> level),
                    .height = std::max(1u, texture->baseHeight >> level),
                    .depth = 1
                }
            };
            // clang-format on
            regions.push_back(region);
        }
    }

    // clang-format off
    const TextureUploadOptions uploadOptions = {
        .destinationTexture = m_texture,
        .dstStages = PipelineStageFlagBit::AllGraphicsBit,
        .dstMask = AccessFlagBit::MemoryReadBit,
        .data = ktxTextureData,
        .byteSize = ktxTextureSize,
        .oldLayout = TextureLayout::Undefined,
        .newLayout = TextureLayout::ShaderReadOnlyOptimal,
        .regions = regions
    };
    // clang-format on

    // TODO: this could benefit from using an uploader decoupled from the example class
    engine.uploadTextureData(uploadOptions);

    // Create a view and sampler
    m_textureView = m_texture.createView(viewOptions);
    m_sampler = GlobalResources::instance().graphicsDevice().createSampler();
}

void SingleTexture::loadBinary(KDGpuExample::ExampleEngineLayer &engine, const KDUtils::File &file, size_t width, size_t height, size_t channelCount, size_t channelSizeInBytes, KDGpu::Format format)
{
    if (!file.isOpen()) {
        const_cast<KDUtils::File*>(&file)->open(std::ios::in | std::ios::binary);
    }

    auto bytes = const_cast<KDUtils::File*>(&file)->readAll();

    internal::ImageData image;
    image.width = width;
    image.height = height;
    image.format = format;
    image.byteSize = bytes.size();
    image.pixelData = new uint8_t[bytes.size()];
    memcpy(image.pixelData, bytes.data(), bytes.size());
    uploadImage(engine, image);
}

void SingleTexture::deinitialize()
{
    m_texture = {};
    m_textureView = {};
    m_sampler = {};
}

} // namespace kdgpu_ext::graphics
