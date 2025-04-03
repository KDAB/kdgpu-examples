#pragma once

#include <KDUtils/file.h>

#include <KDGpu/texture.h>
#include <KDGpuExample/example_engine_layer.h>
#include <texture/internal/image_data.h>

namespace kdgpu_ext::graphics::texture {
/**
 * Just one texture, for one "set" in GLSL.
 */
class SingleTexture
{
public:
    void load(KDGpuExample::ExampleEngineLayer &engine, const KDUtils::File &file);

    void loadKtx(KDGpuExample::ExampleEngineLayer &engine, const KDUtils::File &file);

    void loadBinary(
            KDGpuExample::ExampleEngineLayer &engine,
            const KDUtils::File &file,
            size_t width,
            size_t height,
            size_t channelCount,
            size_t channelSizeInBytes,
            KDGpu::Format format);

    void deinitialize();

    TextureView &view() { return m_textureView; }
    Sampler &sampler() { return m_sampler; }

private:
    void uploadImage(KDGpuExample::ExampleEngineLayer &engine, internal::ImageData image);
    Texture m_texture;
    TextureView m_textureView;
    Sampler m_sampler;
};
} // namespace kdgpu_ext::graphics::texture
