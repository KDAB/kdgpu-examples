#include "gltf_texture.h"

#include <KDGpu/device.h>
#include <KDGpu/queue.h>
#include <KDGpu/texture_options.h>

#include <global_resources.h>

using namespace KDGpu;

void GltfTexture::initialize(const tinygltf::Image &gltfImage, Queue &queue)
{
    using namespace kdgpu_ext::graphics;

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

    m_texture = GlobalResources::instance().graphicsDevice().createTexture(textureOptions);

    // Upload the texture data and transition to ShaderReadOnlyOptimal
    // clang-format off
    const TextureUploadOptions uploadOptions = {
        .destinationTexture = m_texture,
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

    // while texture is uploaded it can not be used
    m_validForUse = false;

    // queue the upload
    m_stagingBuffer = queue.uploadTextureData(uploadOptions);

    // create a view
    m_textureView = m_texture.createView();

    // create sampler
    SamplerOptions samplerOptions = {
        .magFilter = FilterMode::Linear,
        .minFilter = FilterMode::Linear,
        .mipmapFilter = MipmapFilterMode::Linear,
        .u = AddressMode::Repeat,
        .v = AddressMode::Repeat
    };
    m_sampler = GlobalResources::instance().graphicsDevice().createSampler(samplerOptions);
}

void GltfTexture::deinitialize()
{
    m_texture = {};
    m_textureView = {};
    m_stagingBuffer = {};
    m_sampler = {};
}

void GltfTexture::update()
{
    if (!m_validForUse) {
        if (m_stagingBuffer.fence.status() == FenceStatus::Signalled) {
            m_validForUse = true;
            m_stagingBuffer = {};
        }
    }
}
