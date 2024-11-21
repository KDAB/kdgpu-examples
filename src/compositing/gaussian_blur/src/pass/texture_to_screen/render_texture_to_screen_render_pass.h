/*
This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGpuExample/simple_example_engine_layer.h>

#include <texture_target/texture_target.h>
#include <shader/shader_vertex_fragment.h>


class RenderTextureToScreenRenderPass
{
public:
    void initialize(Device &device, Format depthFormat, Format swapchainFormat);
    void deinitialize();

    void update(float time);

    void setSourceTexture(Device &device, TextureTarget &sourceTexture);

    void render(CommandRecorder& commandRecorder, TextureView& depthTextureView, TextureView& swapchainView);

private:
    // User-controlled Shader parameter - vertical moving line
    float m_filterPos{ 0.0f };

    kdgpu_ext::graphics::shader::ShaderVertexFragment m_shader;

    // Internal Render Pass setup
    Sampler m_colorOutputSampler;
    Buffer m_fullScreenQuad;
    PipelineLayout m_pipelineLayout;
    GraphicsPipeline m_graphicsPipeline;
    BindGroup m_bindGroup;
    BindGroupLayout m_bindGroupLayout;
    PushConstantRange m_filterPosPushConstantRange {};
    std::vector<uint8_t> m_filterPosData;
};
