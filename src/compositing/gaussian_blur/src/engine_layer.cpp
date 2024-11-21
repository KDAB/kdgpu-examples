/*
This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "engine_layer.h"

#include <KDGpu/buffer_options.h>

#include <KDGpuExample/engine.h>

void GaussianBlurEngineLayer::initializeScene()
{
    // This effect is separated into 4 render passes:
    // - Render geometry
    // - Blur horizontal
    // - Blur vertical
    // - Render result to screen
    //
    // Each step uses the result from the previous one in the form of a texture.

    // set up global resources (device)
    kdgpu_ext::graphics::GlobalResources::instance().setGraphicsDevice(m_device);

    // initialize the geometry render into texture pass
    // - This pass renders a triangle and the output is stored in the m_geometryTextureTarget texture
    {
        // initialize the texture to store the rendered scene to the size of the swapchain (window)
        m_renderGeometryResult.initialize(m_swapchainExtent, Format::R8G8B8A8_UNORM, TextureUsageFlagBits::ColorAttachmentBit);
        m_renderGeometryPass.initialize(
                m_device, // device
                m_depthFormat, // depth format
                &m_renderGeometryResult // renders into this destination texture
        );
    }

    // initialize horizontal gaussian blur pass
    // - This pass blurs the in-texture and stores the output in the the m_gaussianBlurHorizontalResultTextureTarget texture
    // - Internally it renders a full-screen quad which samples the source texture multiple times to create the blur effect
    {
        m_gaussianBlurHorizontalResult.initialize(m_swapchainExtent, Format::R8G8B8A8_UNORM, TextureUsageFlagBits::ColorAttachmentBit);
        m_gaussianBlurHorizontalPass.initialize(
                m_device, // device
                &m_gaussianBlurHorizontalResult, // renders into destination texture
                GaussianBlurDirection::shaderGaussianBlurHorizontal
        );
        m_gaussianBlurHorizontalPass.setSourceTexture(m_device, m_renderGeometryResult);
    }

    // initialize vertical gaussian blur pass
    // - This pass blurs the in-texture and stores the output in the the m_gaussianBlurVerticalResultTextureTarget texture
    // - Internally it renders a full-screen quad which samples the source texture multiple times to create the blur effect
    {
        m_gaussianBlurVerticalResult.initialize(m_swapchainExtent, Format::R8G8B8A8_UNORM, TextureUsageFlagBits::ColorAttachmentBit);
        m_gaussianBlurVerticalPass.initialize(
                m_device, // device
                &m_gaussianBlurVerticalResult, // renders into destination texture
                GaussianBlurDirection::shaderGaussianBlurVertical
        );
        m_gaussianBlurVerticalPass.setSourceTexture(m_device, m_gaussianBlurHorizontalResult);
    }

    // initialize render texture to the screen pass (swapchain)
    {
        m_renderTextureToScreenPass.initialize(
                m_device, // device
                m_depthFormat, // depth format
                m_swapchainFormat // format of the swap chain (final destination)
        );

        // read from this source texture
        m_renderTextureToScreenPass.setSourceTexture(m_device, m_gaussianBlurVerticalResult);
    }
}

void GaussianBlurEngineLayer::cleanupScene()
{
    // deinitialize textures
    m_renderGeometryResult.deinitialize();
    m_gaussianBlurHorizontalResult.deinitialize();
    m_gaussianBlurVerticalResult.deinitialize();

    // deinitialize render passes
    m_renderGeometryPass.deinitialize();
    m_renderTextureToScreenPass.deinitialize();
    m_gaussianBlurHorizontalPass.deinitialize();
    m_gaussianBlurVerticalPass.deinitialize();

    // deinitialize command buffer
    m_commandBuffer = {};
}

void GaussianBlurEngineLayer::updateScene()
{
    const float currentTime = time();

    // animates the blur a bit
    {
        // this makes it more or less blurry
        auto blurScale = 5.0f + 2.0f * std::sin(currentTime);

        // this makes it dim and brighter
        auto blurStrength = 1.0f + 0.5 * std::sin(currentTime * 0.5);

        // propagate these values to the passes
        m_gaussianBlurHorizontalPass.setBlurScale(blurScale);
        m_gaussianBlurHorizontalPass.setBlurStrength(blurStrength);
        m_gaussianBlurVerticalPass.setBlurScale(blurScale);
        m_gaussianBlurVerticalPass.setBlurStrength(blurStrength);
    }

    // rotate the geometry (triangle) a little
    m_renderGeometryPass.update();

    // set time in render-to-screen pass to move the vertical line
    m_renderTextureToScreenPass.update(currentTime);
}

void GaussianBlurEngineLayer::render()
{
    // get a reference to the current (back or front) swapchain texture view
    auto &currentSwapchainTextureView = m_swapchainViews.at(m_currentSwapchainImageIndex);

    // Create a command recorder for rendering
    auto commandRecorder = m_device.createCommandRecorder();

    // Pass 1: Render Geometry To Texture
    m_renderGeometryPass.render(commandRecorder, m_depthTextureView);

    // Pass 2 + 3: filter the texture
    // Note that these passes are set up to consume textures from previous passes, see initializeScene()
    m_gaussianBlurHorizontalPass.render(commandRecorder);
    m_gaussianBlurVerticalPass.render(commandRecorder);

    // Pass 4: Render Texture To Screen
    {
        // dynamically connect to another source texture - bypassing the blur or not
        {
            const float currentTime = time();
            if (fmodf(currentTime, 2.0f) < 1.0f) {
                m_renderTextureToScreenPass.setSourceTexture(m_device, m_renderGeometryResult);
            }
            else {
                m_renderTextureToScreenPass.setSourceTexture(m_device, m_gaussianBlurVerticalResult);
                // show only the horizontal blur
                //m_renderTextureToScreenPass.setSourceTexture(m_device, m_gaussianBlurHorizontalResult);
            }
        }
        m_renderTextureToScreenPass.render(commandRecorder, m_depthTextureView, currentSwapchainTextureView);
    }

    // Pass 5: (optional) Render ImGui
    {
        // clang-format off
        auto imGuiPass = commandRecorder.beginRenderPass({
           .colorAttachments = {
               {
                   .view = currentSwapchainTextureView, // one of the swap chain textures
                   .loadOperation = AttachmentLoadOperation::DontCare, // don't clear the color attachment, just render on top
                   .finalLayout = TextureLayout::PresentSrc
               }
           },
           .depthStencilAttachment = {
               .view = m_depthTextureView
           }
        });
        // clang-format on
        renderImGuiOverlay(&imGuiPass);
        imGuiPass.end();
    }

    // Finalize the command recording
    m_commandBuffer = commandRecorder.finish();

    // Submit the commands to the GPU
    {
        const SubmitOptions submitOptions = {
            .commandBuffers = { m_commandBuffer },
            .waitSemaphores = { m_presentCompleteSemaphores[m_inFlightIndex] },
            .signalSemaphores = { m_renderCompleteSemaphores[m_inFlightIndex] }
        };
        m_queue.submit(submitOptions);
    }
}

void GaussianBlurEngineLayer::resize()
{
    // textures
    m_renderGeometryResult.resize(m_swapchainExtent);
    m_gaussianBlurHorizontalResult.resize(m_swapchainExtent);
    m_gaussianBlurVerticalResult.resize(m_swapchainExtent);

    // render passes
    m_renderGeometryPass.resize(&m_renderGeometryResult);
    // m_gaussianBlurHorizontalPass.resize(m_device);
    // m_gaussianBlurVerticalPass.resize(m_device);
    m_gaussianBlurHorizontalPass.setSourceTexture(m_device, m_renderGeometryResult);
    m_gaussianBlurVerticalPass.setSourceTexture(m_device, m_gaussianBlurHorizontalResult);

    // re-bind to the source texture, the binding is invalid
    m_renderTextureToScreenPass.setSourceTexture(m_device, m_gaussianBlurVerticalResult);
}

float GaussianBlurEngineLayer::time()
{
    return engine()->simulationTime().count() / 1.0e9;
}
