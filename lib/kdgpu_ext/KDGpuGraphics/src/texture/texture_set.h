#pragma once

#include <vector>

#include <KDUtils/file.h>

#include <texture/single_texture.h>

namespace kdgpu_ext::graphics::texture {

/**
 * Supports one or more textures to be used in a shader under a specific bind group or set.
 */
class TextureUniformSet
{
public:
    void loadAddTexture(KDGpuExample::ExampleEngineLayer &engine, const KDUtils::File &file);

    void loadBinaryTexture(
            KDGpuExample::ExampleEngineLayer &engine,
            const KDUtils::File &file,
            size_t width,
            size_t height,
            size_t channelCount,
            size_t channelSizeInBytes,
            KDGpu::Format format);

    void initializeCreateBindGroups();
    void deinitialize();

    BindGroup &bindGroup() { return m_textureBindGroup; }
    BindGroupLayout &bindGroupLayout() { return m_bindGroupLayout; }

private:
    std::vector<SingleTexture> m_textures;
    BindGroup m_textureBindGroup;
    BindGroupLayout m_bindGroupLayout;
};

} // namespace kdgpu_ext::graphics::texture
