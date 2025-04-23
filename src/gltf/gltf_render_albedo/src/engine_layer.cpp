/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "engine_layer.h"

#include <global_resources.h>

#include <KDGpuExample/engine.h>

#include <KDUtils/dir.h>

#include <GltfHolder/gltf_holder_global.h>

GltfRenderAlbedoEngineLayer::GltfRenderAlbedoEngineLayer()
    : SimpleExampleEngineLayer()
{
}

void GltfRenderAlbedoEngineLayer::initializeScene()
{
    // set a closer far plane
    camera()->lens().farPlane = 50.0f;

    // set up global resources (device)
    kdgpu_ext::graphics::GlobalResources::instance().setGraphicsDevice(m_device);

    // initialize global gltf variables
    kdgpu_ext::gltf_holder::GltfHolderGlobal::instance().initialize();

    std::string baseDir = "gltf/gltf_render_albedo/";
    auto assetDir = KDGpuExample::assetDir();

    // load texture
    m_textureSet.loadAddTexture(*this, assetDir.file(baseDir + "red_green_blobs_on_white.png").path());
    m_textureSet.loadAddTexture(*this, assetDir.file(baseDir + "small_red_blobs_on_black.png").path());
    m_textureSet.initializeCreateBindGroups();

    m_otherChannelTextureSet.loadAddTexture(*this, assetDir.file(baseDir + "small_red_blobs_on_black.png").path());
    m_otherChannelTextureSet.initializeCreateBindGroups();

    // load gltf object(s)
    {
        m_flightHelmet.load(KDGpuExample::assetDir().file(baseDir + "FlightHelmet.gltf").path(), m_queue);

        // set the per-node uniform buffer object binding to 0
        m_flightHelmet.setNodeTransformShaderBinding(0);
    }

    // set camera aspect ratio
    m_camera.lens().aspectRatio = float(m_window->width()) / float(m_window->height());

    // initialize the albedo render into texture pass
    // - This pass renders a triangle and the output is stored in the m_geometryTextureTarget texture
    {
        m_albedoPass.m_textureSet = &m_textureSet;
        m_albedoPass.initializeShader();
        m_albedoPass.addGltfHolder(m_flightHelmet);
        m_albedoPass.initialize(m_depthFormat, m_swapchainExtent);
    }

    // initialize "other channel" pass
    {
        m_otherChannelPass.m_textureSet = &m_otherChannelTextureSet;
        m_otherChannelPass.initializeShader();
        m_otherChannelPass.addGltfHolder(m_flightHelmet);
        m_otherChannelPass.initialize(m_albedoPass.resultDepthTextureTarget(), m_depthFormat, m_swapchainExtent);
    }

    // initialize render texture to the screen pass (swapchain)
    {
        m_compositingPass.initialize(
                m_swapchainFormat // format of the swap chain (final destination)
        );

        // read from this source texture
        m_compositingPass.setInputTextures(
            m_device,
            m_albedoPass.result_color_texture_target(),
            m_otherChannelPass.resultColorTextureTarget()
        );
    }
}

void GltfRenderAlbedoEngineLayer::cleanupScene()
{
    m_textureSet.deinitialize();
    m_otherChannelTextureSet.deinitialize();
    m_flightHelmet.deinitialize();

    // deinitialize render passes
    m_albedoPass.deinitialize();
    m_otherChannelPass.deinitialize();
    m_compositingPass.deinitialize();

    // deinitialize command buffer
    m_commandBuffer = {};

    // clean up global gltf variable
    kdgpu_ext::gltf_holder::GltfHolderGlobal::instance().deinitialize();
}

void GltfRenderAlbedoEngineLayer::updateScene()
{
    const float currentTime = time();

    m_camera.update();

    m_flightHelmet.update();

    m_albedoPass.update_view_projection_matrices_from_camera(m_camera);
    m_otherChannelPass.updateViewProjectionMatricesFromCamera(m_camera);

    // set time in render-to-screen pass to move the vertical line
    m_compositingPass.update(currentTime);
}

void GltfRenderAlbedoEngineLayer::renderPasses(CommandRecorder &commandRecorder)
{
    // Render Geometry To Texture
    {
        m_albedoPass.render(commandRecorder);
    }

    // Render Other Channel To Texture
    {
        m_otherChannelPass.render(commandRecorder);
    }

    // Render Color Texture To Screen
    {
        // get a reference to the current (back or front) swapchain texture view
        auto &currentSwapchainTextureView = m_swapchainViews.at(m_currentSwapchainImageIndex);
        m_compositingPass.render(commandRecorder, currentSwapchainTextureView);
    }

    // (optional) Render ImGui
    {
        auto &currentSwapchainTextureView = m_swapchainViews.at(m_currentSwapchainImageIndex);
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
               .view = m_albedoPass.resultDepthTextureTarget().textureView()
           }
        });
        // clang-format on
        renderImGuiOverlay(&imGuiPass);
        imGuiPass.end();
    }
}
void GltfRenderAlbedoEngineLayer::render()
{
    // Create a command recorder for rendering
    auto commandRecorder = m_device.createCommandRecorder();

    renderPasses(commandRecorder);

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

void GltfRenderAlbedoEngineLayer::resize()
{
    // render passes
    m_albedoPass.resize(m_swapchainExtent);

    // re-bind to the source texture, the binding is invalid
    m_compositingPass.setInputTextures(
        m_device,
        m_albedoPass.result_color_texture_target(),
        m_otherChannelPass.resultColorTextureTarget()
        );
}

float GltfRenderAlbedoEngineLayer::time()
{
    return engine()->simulationTime().count() / 1.0e9;
}
