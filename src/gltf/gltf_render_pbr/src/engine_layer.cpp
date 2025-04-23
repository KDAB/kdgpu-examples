/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "engine_layer.h"

#include <global_resources.h>
#include <imgui.h>

#include <KDGpuExample/engine.h>

#include <KDUtils/dir.h>

#include <GltfHolder/gltf_holder_global.h>

GltfRenderPbrEngineLayer::GltfRenderPbrEngineLayer()
    : SimpleExampleEngineLayer()
{
}

void GltfRenderPbrEngineLayer::initializeScene()
{
    // set a closer far plane
    camera()->lens().farPlane = 50.0f;

    // set up global resources (device)
    kdgpu_ext::graphics::GlobalResources::instance().setGraphicsDevice(m_device);

    // initialize global gltf variables
    kdgpu_ext::gltf_holder::GltfHolderGlobal::instance().initialize();

    // asset path
    std::string baseDir = "gltf/gltf_render_pbr/";
    auto assetDir = KDGpuExample::assetDir();

    // imgui
    registerImGuiOverlayDrawFunction([this](ImGuiContext *ctx){drawControls(ctx);});

    // load textures
    // pbr environment
    {
        m_pbrLutAndEnvironmentTextures.loadAddTexture(*this, assetDir.file(baseDir + "environment/diffuse.ktx2"));
        m_pbrLutAndEnvironmentTextures.loadAddTexture(*this, assetDir.file(baseDir + "environment/specular.ktx2"));
        m_pbrLutAndEnvironmentTextures.loadAddTexture(*this,  assetDir.file(baseDir + "pbr/lut_ggx.ktx2"));
        m_pbrLutAndEnvironmentTextures.initializeCreateBindGroups();
    }

    // blue dots extra pass texture
    {
        m_otherChannelTextureSet.loadAddTexture(*this, assetDir.file(baseDir + "small_red_blobs_on_black.png"));
        m_otherChannelTextureSet.initializeCreateBindGroups();
    }

    // textures for basic geometry
    {
        m_basicGeometryTextureSet.loadAddTexture(*this, assetDir.file(baseDir + "area_light_textured_light/stained_glass.png"));
        m_basicGeometryTextureSet.initializeCreateBindGroups();
    }

    // area light channels
    {
        m_areaLightChannels.loadBinaryTexture(
            *this,
            assetDir.file(baseDir + "area_light/ltc_amp_rg32f_32x32.bin"),
            32,
            32,
            2,
            sizeof(float),
            KDGpu::Format::R32G32_SFLOAT);
        m_areaLightChannels.loadBinaryTexture(
            *this,
            assetDir.file(baseDir + "area_light/ltc_mat_rgba32f_32x32.bin"),
            32,
            32,
            4,
            sizeof(float),
            KDGpu::Format::R32G32B32A32_SFLOAT);
        m_areaLightChannels.loadAddTexture(
                *this,
                assetDir.file(baseDir + "area_light_textured_light/stained_glass_filtered.png"));
        m_areaLightChannels.initializeCreateBindGroups();
    }

    // load gltf object(s)
    {
        m_flightHelmet.load(assetDir.file(baseDir + "FlightHelmet.gltf").path(), m_queue);

        // set the per-node uniform buffer object binding to 0
        m_flightHelmet.setNodeTransformShaderBinding(0);
    }



    // set camera aspect ratio
    m_camera.lens().aspectRatio = float(m_window->width()) / float(m_window->height());

    // initialize the albedo render into texture pass
    // - This pass renders a triangle and the output is stored in the m_geometryTextureTarget texture
    {
        m_pbrPass.m_pbrTextureSet = &m_pbrLutAndEnvironmentTextures;
        m_pbrPass.initializeShader();
        m_pbrPass.addGltfHolder(m_flightHelmet);
        m_pbrPass.initialize(m_depthFormat, m_swapchainExtent);
    }

    // initialize "other channel" pass
    {
        m_otherChannelPass.m_textureSet = &m_otherChannelTextureSet;
        m_otherChannelPass.initializeShader();
        m_otherChannelPass.addGltfHolder(m_flightHelmet);
        m_otherChannelPass.initialize(m_pbrPass.resultDepthTextureTarget(), m_swapchainExtent);
    }

    // initialize "basic geometry" pass
    {
        m_basicGeometryPass.m_textureSet = &m_basicGeometryTextureSet;
        m_basicGeometryPass.initialize(m_pbrPass.resultDepthTextureTarget(), m_swapchainExtent);
    }

    // initialize area light pass
    {
        m_areaLightPass.m_ltcTextureSet = &m_areaLightChannels;
        m_areaLightPass.initializeShader();
        m_areaLightPass.addGltfHolder(m_flightHelmet);
        m_areaLightPass.initialize(m_pbrPass.resultDepthTextureTarget(), m_swapchainExtent);
    }

    // initialize render texture to the screen pass (swapchain)
    {
        m_compositingPass.initialize(
                m_swapchainFormat // format of the swap chain (final destination)
        );

        // read from this source texture
        m_compositingPass.setInputTextures(
            m_pbrPass.resultColorTextureTarget(),
            m_otherChannelPass.resultColorTextureTarget(),
            m_basicGeometryPass.result_color_texture_target(),
            m_areaLightPass.resultColorTextureTarget()
        );
    }
}

void GltfRenderPbrEngineLayer::cleanupScene()
{
    m_pbrLutAndEnvironmentTextures.deinitialize();
    m_otherChannelTextureSet.deinitialize();
    m_flightHelmet.deinitialize();
    m_areaLightChannels.deinitialize();
    m_basicGeometryTextureSet.deinitialize();

    // deinitialize render passes
    m_pbrPass.deinitialize();
    m_otherChannelPass.deinitialize();
    m_compositingPass.deinitialize();
    m_basicGeometryPass.deinitialize();
    m_areaLightPass.deinitialize();

    // deinitialize command buffer
    m_commandBuffer = {};

    // clean up global gltf variable
    kdgpu_ext::gltf_holder::GltfHolderGlobal::instance().deinitialize();
}

void GltfRenderPbrEngineLayer::updateScene()
{
    const float currentTime = time();

    m_camera.update();

    m_flightHelmet.update();

    m_pbrPass.updateViewProjectionMatricesFromCamera(m_camera);
    m_otherChannelPass.updateViewProjectionMatricesFromCamera(m_camera);
    m_basicGeometryPass.updateViewProjectionMatricesFromCamera(m_camera);

    // area light pass
    {
        m_areaLightPass.setQuadVertices(m_basicGeometryPass.quadVertices());
        m_areaLightPass.updateViewProjectionMatricesFromCamera(m_camera);
    }

    // set time in render-to-screen pass to move the vertical line
    m_compositingPass.update(currentTime);
}

void GltfRenderPbrEngineLayer::render_passes(CommandRecorder &commandRecorder)
{
    // Render GLTF Geometry using PBR shader to Texture
    {
        m_pbrPass.render(commandRecorder);
    }

    // Render GLTF Geometry with another (blue dots) texture channel To Texture
    {
        m_otherChannelPass.render(commandRecorder);
    }

    // Render Basic Geometry pass (tasked with rendering the textured quad for the area light)
    {
        m_basicGeometryPass.render(commandRecorder);
    }

    // Render Area Light pass (diffuse and specular)
    {
        m_areaLightPass.render(commandRecorder);
    }


    // Composite the textures from the previous render passes and render to screen
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
               .view = m_pbrPass.resultDepthTextureTarget().textureView()
           }
        });
        // clang-format on
        renderImGuiOverlay(&imGuiPass);
        imGuiPass.end();
    }
}

void GltfRenderPbrEngineLayer::drawControls(ImGuiContext *ctx)
{
    ImGui::SetCurrentContext(ctx);
    ImGui::SetNextWindowPos(ImVec2(10, 170), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin(
            "Controls",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);

    ImGui::Text("gltf_pbr pass:");
    ImGui::Text("This pass renders the GLTF with Physically Based Rendering.");
    ImGui::SliderFloat("Image Based Lighting (IBL) Intensity/Contribution", &m_iblIntensity, 0.0f, 1.0f);
    ImGui::SliderFloat("Directional Light Intensity", &m_directionalLightIntensity, 0.0f, 10.0f);

    ImGui::Text("gltf_other_channel pass:");
    ImGui::Text("This pass illustrates how to render GLTF with a totally different set of texture(s).");
    ImGui::SliderFloat("Blue dot intensity", &m_blueDotIntensity, 0.0f, 1.0f);

    ImGui::Text("gltf_area_light pass:");
    ImGui::Text("This pass calculates diffuse and specular area light contribution.");
    ImGui::SliderFloat("Diffuse Intensity", &m_generalLightIntensity, 0.0f, 10.0f);
    ImGui::SliderFloat("Specular Intensity", &m_specularLightIntensity, 0.0f, 10.0f);
    ImGui::End();

    m_pbrPass.setIblIntensity(m_iblIntensity);
    m_pbrPass.setDirectionalLightIntensity(m_directionalLightIntensity);
    m_areaLightPass.setLightIntensity(m_generalLightIntensity, m_specularLightIntensity);
    m_otherChannelPass.setIntensity(m_blueDotIntensity);
}
void GltfRenderPbrEngineLayer::render()
{
    // Create a command recorder for rendering
    auto commandRecorder = m_device.createCommandRecorder();

    render_passes(commandRecorder);

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

void GltfRenderPbrEngineLayer::resize()
{
    // render passes
    m_pbrPass.resize(m_swapchainExtent);

    // re-bind to the source texture, the binding is invalid
    m_compositingPass.setInputTextures(
        m_pbrPass.resultColorTextureTarget(),
        m_otherChannelPass.resultColorTextureTarget(),
        m_basicGeometryPass.result_color_texture_target(),
        m_areaLightPass.resultColorTextureTarget()
        );
}

float GltfRenderPbrEngineLayer::time()
{
    return engine()->simulationTime().count() / 1.0e9;
}
