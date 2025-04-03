#include "texture_set.h"

#include <global_resources.h>
#include <filesystem>

namespace kdgpu_ext::graphics::texture {

void TextureUniformSet::loadAddTexture(KDGpuExample::ExampleEngineLayer &engine, const KDUtils::File &file)
{
    SingleTexture new_texture;
    std::filesystem::path p = file.path();
    std::string ext = p.extension().string();
    if (ext == ".ktx" || ext == ".ktx2")
        new_texture.loadKtx(engine, file);
    else
        new_texture.load(engine, file);
    m_textures.push_back(std::move(new_texture));
}

void TextureUniformSet::loadBinaryTexture(KDGpuExample::ExampleEngineLayer &engine, const KDUtils::File &file, size_t width, size_t height, size_t channelCount, size_t channelSizeInBytes, KDGpu::Format format)
{
    SingleTexture new_texture;
    new_texture.loadBinary(
            engine,
            file,
            width,
            height,
            channelCount,
            channelSizeInBytes,
            format);
    m_textures.push_back(std::move(new_texture));

}

void TextureUniformSet::initializeCreateBindGroups()
{
    BindGroupLayoutOptions bind_group_layout_options;
    {
        uint32_t binding_index = 0;
        for (const auto &texture : m_textures) {
            bind_group_layout_options.bindings.push_back(
                    { .binding = binding_index,
                      .resourceType = ResourceBindingType::CombinedImageSampler,
                      .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit) });
            binding_index++;
        }
    }

    m_bindGroupLayout = GlobalResources::instance().graphicsDevice().createBindGroupLayout(bind_group_layout_options);

    BindGroupOptions bind_group_options;
    {
        bind_group_options.layout = m_bindGroupLayout;
        uint32_t binding_index = 0;
        for (auto &texture : m_textures) {
            bind_group_options.resources.push_back({ .binding = binding_index,
                                                     .resource = TextureViewSamplerBinding{
                                                             .textureView = texture.view(),
                                                             .sampler = texture.sampler() } });
            binding_index++;
        }
    }

    m_textureBindGroup = GlobalResources::instance().graphicsDevice().createBindGroup(bind_group_options);
}
void TextureUniformSet::deinitialize()
{
    for (auto &texture : m_textures)
        texture.deinitialize();

    m_bindGroupLayout = {};
    m_textureBindGroup = {};
}
} // namespace kdgpu_ext::graphics
