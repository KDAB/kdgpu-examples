/*
This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGpuExample/simple_example_engine_layer.h>

#include <pass/gaussian_blur/gaussian_blur_direction.h>

#include <texture_target/texture_target.h>
#include <shader/shader_vertex_fragment.h>

/**
 * @brief Blurs sourceTexture and stores the result in targetTexture
 * Only does one direction - horizontal or vertical.
 */
class GaussianBlurRenderPass
{
public:
    void initialize(
        Device &device, // global device
        TextureTarget *targetTexture, // output of blur - target texture
        GaussianBlurDirection direction // horizontal or vertical blur - shader specialization
    );

    void deinitialize();

    void setSourceTexture(Device &device, TextureTarget &sourceTexture);

    void render(CommandRecorder& commandRecorder);

    // control blur scale (how much blur)
    void setBlurScale(float blurScale) { m_pushConstantsData.blurScale = blurScale; }

    // control blur intensity (a value between 0.0 and 1.0 usually)
    void setBlurStrength(float blurStrength) { m_pushConstantsData.blurStrength = blurStrength; }

private:

    // Target Texture
    TextureTarget* m_targetTexture = nullptr;

    // User-controlled Shader parameters - vertical moving line
    #pragma pack(push)
    #pragma pack(1)
    struct pushConstants
    {
        float blurScale{ 0.0f };
        float blurStrength{ 1.0f };
    };
    #pragma pack(pop)

    pushConstants m_pushConstantsData;

    // Internal Render Pass setup
    kdgpu_ext::graphics::shader::ShaderVertexFragment m_shader;
    Sampler m_colorOutputSampler;
    Buffer m_fullScreenQuad;
    PipelineLayout m_pipelineLayout;
    GraphicsPipeline m_graphicsPipeline;
    BindGroup m_bindGroup;
    BindGroupLayout m_bindGroupLayout;
    PushConstantRange m_pushConstantRange {};
};
