/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGpuExample/simple_example_engine_layer.h>

// passes
#include <pass/gltf_albedo/gltf_albedo_pass.h>
#include <pass/gltf_other_channel/gltf_other_channel_pass.h>
#include <pass/compositing/compositing_pass.h>

#include <texture/texture_set.h>

#include <camera/camera.h>

class GltfRenderAlbedoEngineLayer : public KDGpuExample::SimpleExampleEngineLayer
{
public:
    GltfRenderAlbedoEngineLayer();
    kdgpu_ext::graphics::camera::Camera *camera() { return &m_camera; }

protected:
    void initializeScene() override;
    void cleanupScene() override;
    void updateScene() override;
    void render() override;
    void resize() override;

private:
    float time();
    void renderPasses(CommandRecorder &commandRecorder);

    kdgpu_ext::graphics::camera::Camera m_camera;

    // GLTF
    kdgpu_ext::gltf_holder::GltfHolder m_flightHelmet;

    kdgpu_ext::graphics::texture::TextureUniformSet m_textureSet;
    kdgpu_ext::graphics::texture::TextureUniformSet m_otherChannelTextureSet;

    // render passes
    pass::gltf_albedo::GltfAlbedoPass m_albedoPass;
    pass::gltf_other_channel::GltfOtherChannelPass m_otherChannelPass;
    pass::compositing::CompositingPass m_compositingPass;

    // command buffer
    CommandBuffer m_commandBuffer;
};
