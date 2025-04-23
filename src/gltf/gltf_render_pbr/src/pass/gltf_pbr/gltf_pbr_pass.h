/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <camera/camera.h>

#include <pass/gltf_pbr/uniform_buffer/configuration.h>

#include <GltfHolder/material/rendering/gltf_materials_for_a_pass.h>
#include <GltfHolder/render_permutation/gltf_render_permutations.h>

#include <texture_target/texture_target.h>
#include <texture/texture_set.h>
#include <shader/shader_vertex_fragment.h>
#include <uniform/uniform_buffer_object.h>

namespace pass::gltf_pbr {
class gltfPbrPass
{
public:
    void addGltfHolder(kdgpu_ext::gltf_holder::GltfHolder &holder);
    void initializeShader();

    void initialize(Format depthFormat, Extent2D swapchainExtent, const kdgpu_ext::graphics::camera::Camera &camera);
    void resize(Extent2D swapChainExtent);
    void deinitialize();

    void setDirectionalLightIntensity(float intensity);
    void setIblIntensity(float intensity);
    void updateConfiguration();

    void render(CommandRecorder& commandRecorder, const kdgpu_ext::graphics::camera::Camera& camera);

    TextureTarget& resultColorTextureTarget() { return m_colorTextureTarget; }
    TextureTarget& resultDepthTextureTarget() { return m_depthTextureTarget; }

    kdgpu_ext::graphics::texture::TextureUniformSet* m_pbrTextureSet = nullptr;

private:
    // shader
    kdgpu_ext::graphics::shader::ShaderVertexFragment m_shader;

    // material
    kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels m_shaderTextureChannels;

    // object render permutations
    kdgpu_ext::gltf_holder::GltfRenderPermutations m_gltfRenderPermutations;

    // uniform buffer for vertex shader, camera
    UniformBufferObject<uniform_buffer::configuration, ShaderStageFlagBits::FragmentBit> m_materialUniformBufferObject = {};

    // results - textures
    TextureTarget m_colorTextureTarget;
    TextureTarget m_depthTextureTarget;

    // result - render target
    RenderTarget m_renderTarget;
};

}
