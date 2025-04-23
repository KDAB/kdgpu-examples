/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGpuExample/simple_example_engine_layer.h>

// passes
#include <pass/gltf_pbr/gltf_pbr_pass.h>
#include <pass/gltf_other_channel/gltf_other_channel_pass.h>
#include <pass/basic_geometry/basic_geometry_pass.h>
#include <pass/compositing/compositing_pass.h>
#include <pass/gltf_area_light/gltf_area_light_pass.h>

#include <texture/texture_set.h>

#include <tinygltf_helper/camera.h>

class GltfRenderPbrEngineLayer : public KDGpuExample::SimpleExampleEngineLayer
{
public:
    GltfRenderPbrEngineLayer();
    TinyGltfHelper::Camera *camera() { return &m_camera; }

protected:
    void initializeScene() override;
    void cleanupScene() override;
    void updateScene() override;
    void render() override;
    void resize() override;

private:
    float time();
    void render_passes(CommandRecorder &commandRecorder);

    void drawControls(ImGuiContext *ctx);

    TinyGltfHelper::Camera m_camera;

    // GLTF
    kdgpu_ext::gltf_holder::GltfHolder m_flightHelmet;

    kdgpu_ext::graphics::texture::TextureUniformSet m_pbrLutAndEnvironmentTextures;

    kdgpu_ext::graphics::texture::TextureUniformSet m_otherChannelTextureSet;
    kdgpu_ext::graphics::texture::TextureUniformSet m_basicGeometryTextureSet;

    kdgpu_ext::graphics::texture::TextureUniformSet m_areaLightChannels;

    // render passes (in order of rendering)
    pass::gltf_pbr::gltfPbrPass m_pbrPass;
    pass::gltf_other_channel::GltfOtherChannelPass m_otherChannelPass;
    pass::basic_geometry::BasicGeometryPass m_basicGeometryPass;
    pass::gltf_area_light::GltfAreaLightPass m_areaLightPass;
    pass::compositing::CompositingPass m_compositingPass;

    // command buffer
    CommandBuffer m_commandBuffer;

    // user configuration / gui values
    float m_directionalLightIntensity = 1.0f;
    float m_iblIntensity = 0.4f;
    float m_blueDotIntensity = 0.0f;
    float m_generalLightIntensity = 1.0f;
    float m_specularLightIntensity = 1.5f;
};
