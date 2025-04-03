#include "texture_target.h"

#include <global_resources.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/texture_options.h>

void TextureTarget::initialize(const Extent2D &extent, Format format, TextureUsageFlagBits usage)
{
    m_usage = usage;
    m_extent = {extent.width, extent.height, 1};
    m_format = format;
    auto& device = kdgpu_ext::graphics::GlobalResources::instance().graphicsDevice();
    m_texture = device.createTexture(
            { // KdGPU::TextureOptions:
              .type = TextureType::TextureType2D,
              .format = m_format,
              .extent = m_extent,
              .mipLevels = 1,
              .usage = m_usage | TextureUsageFlagBits::SampledBit,
              .memoryUsage = MemoryUsage::GpuOnly }
            // ---
    );
    m_textureView = m_texture.createView();
}

void TextureTarget::deinitialize()
{
    m_textureView = {};
    m_texture = {};
}

void TextureTarget::resize(const Extent2D &extent)
{
    initialize(extent, m_format, m_usage);
}

std::optional<RenderTargetOptions> TextureTarget::renderTargetOptions()
{
    if (m_usage == TextureUsageFlagBits::DepthStencilAttachmentBit)
        return {};
    return RenderTargetOptions{ .format = format() };
}

std::optional<DepthStencilOptions> TextureTarget::depthStencilOptions() const
{
    if (m_usage == TextureUsageFlagBits::DepthStencilAttachmentBit)
        return DepthStencilOptions{
            .format = format(),
            .depthWritesEnabled = true,
            .depthCompareOperation = CompareOperation::Less
        };
    return {};
}
