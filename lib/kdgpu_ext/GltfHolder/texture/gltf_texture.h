#pragma once

#include <tiny_gltf.h>
#include <KDGpu/queue.h>
#include <KDGpu/sampler.h>
#include <KDGpu/texture.h>

class GltfTexture
{
public:
  void initialize(const tinygltf::Image &image, KDGpu::Queue &queue);
  void deinitialize();
  void update();
  bool isValid() { return m_validForUse; }
  KDGpu::Handle<KDGpu::Sampler_t> samplerHandle() { return m_sampler;}
  KDGpu::Handle<KDGpu::TextureView_t> textureViewHandle() { return m_textureView;}
  KDGpu::Sampler& sampler() { return m_sampler;}
  KDGpu::TextureView& textureView() { return m_textureView;}
  KDGpu::Texture& texture() { return m_texture; }


private:
  bool m_validForUse = false;
  KDGpu::Texture m_texture;
  KDGpu::TextureView m_textureView;
  KDGpu::UploadStagingBuffer m_stagingBuffer;
  KDGpu::Sampler m_sampler;
};
