#pragma once

#include <KDGpuExample/simple_example_engine_layer.h>

#include <texture_target/texture_usage.h>

class TextureTarget
{
public:
    void initialize(
        const Extent2D& extent,
        Format format,
        TextureUsageFlagBits usage = TextureUsageFlagBits::ColorAttachmentBit);
    void deinitialize();
    void resize(const Extent2D& extent);
    Format format() const { return m_format; }
    TextureView& textureView() { return m_textureView;}
    Texture& texture() { return m_texture; }

    Extent2D dimensions2D() const { return {m_extent.width, m_extent.height}; }
    Extent3D dimensions3D() const { return m_extent; }

    // if usage type is "color", this will return valid RenderTargetOptions
    std::optional<RenderTargetOptions> renderTargetOptions();

    // if usage type is "depth", this will return valid DepthStencilOptions
    std::optional<DepthStencilOptions> depthStencilOptions() const;
private:
    TextureUsageFlagBits m_usage = TextureUsageFlagBits::ColorAttachmentBit;
    Texture m_texture;
    TextureView m_textureView;
    Format m_format = {};
    Extent3D m_extent = {};
};
