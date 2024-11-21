/*
This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

// render passes
#include <pass/geometry/render_geometry_to_texture_render_pass.h>
#include <pass/texture_to_screen/render_texture_to_screen_render_pass.h>
#include <pass/gaussian_blur/gaussian_blur_render_pass.h>

#include <KDGpuExample/simple_example_engine_layer.h>

class GaussianBlurEngineLayer : public KDGpuExample::SimpleExampleEngineLayer
{
protected:
    void initializeScene() override;
    void cleanupScene() override;
    void updateScene() override;
    void render() override;
    void resize() override;

private:
    float time();

    // textures (results of render passes)
    TextureTarget m_renderGeometryResult;
    TextureTarget m_gaussianBlurHorizontalResult;
    TextureTarget m_gaussianBlurVerticalResult;

    // render passes
    RenderGeometryToTextureRenderPass m_renderGeometryPass;
    GaussianBlurRenderPass m_gaussianBlurHorizontalPass;
    GaussianBlurRenderPass m_gaussianBlurVerticalPass;
    RenderTextureToScreenRenderPass m_renderTextureToScreenPass;

    // command buffer
    CommandBuffer m_commandBuffer;
};
