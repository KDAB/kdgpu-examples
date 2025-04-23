/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGpuExample/simple_example_engine_layer.h>

#include <shader/shader_vertex_fragment.h>
#include <texture_target/texture_target.h>

namespace pass::compositing {
class CompositingPass
{
public:
  void initialize(Format swapchainFormat);
  void deinitialize();

  void update(float time);

  void setInputTextures(
          Device &device,
          TextureTarget &albedoColorTexture,
          TextureTarget &otherChannelColorTexture);

  void render(CommandRecorder &commandRecorder, TextureView &swapchainView);

private:
  // shader
  kdgpu_ext::graphics::shader::ShaderVertexFragment m_shader;

  TextureTarget *m_pbrPassTexture = nullptr;
  TextureTarget *m_sourceOtherChannelColorTexture = nullptr;

  // Internal Render Pass setup
  Sampler m_colorOutputSampler;
  Buffer m_fullScreenQuad;
  PipelineLayout m_pipelineLayout;
  GraphicsPipeline m_graphicsPipeline;
  BindGroup m_bindGroup;
  BindGroupLayout m_bindGroupLayout;
};
}
