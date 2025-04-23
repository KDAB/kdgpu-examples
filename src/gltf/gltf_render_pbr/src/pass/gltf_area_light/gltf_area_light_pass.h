/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <camera/camera.h>

#include <pass/gltf_area_light/uniform_buffer/configuration.h>

#include <GltfHolder/material/rendering/gltf_materials_for_a_pass.h>
#include <GltfHolder/render_permutation/gltf_render_permutations.h>

#include <texture_target/texture_target.h>
#include <texture/texture_set.h>
#include <shader/shader_vertex_fragment.h>
#include <uniform/uniform_buffer_object.h>

namespace pass::gltf_area_light {
class GltfAreaLightPass
{
public:
    void addGltfHolder(kdgpu_ext::gltf_holder::GltfHolder &holder);
    void initializeShader();

    void initialize(TextureTarget &depth_texture, Extent2D swapchainExtent, const kdgpu_ext::graphics::camera::Camera &camera);
    void resize(Extent2D swapChainExtent);
    void deinitialize();

    void setQuadVertices(std::vector<glm::vec3> &vertices);
    void setLightIntensity(float genericIntensity, float specularIntensity);
    void updateEyePositionFromCamera(kdgpu_ext::graphics::camera::Camera &camera);

    void render(CommandRecorder &commandRecorder, const kdgpu_ext::graphics::camera::Camera &camera);

    TextureTarget &resultColorTextureTarget() { return m_colorTextureTarget; }

    kdgpu_ext::graphics::texture::TextureUniformSet *m_ltcTextureSet = nullptr;

private:
    // shader
    kdgpu_ext::graphics::shader::ShaderVertexFragment m_shader;

    // material
    kdgpu_ext::gltf_holder::shader_specification::GltfShaderTextureChannels m_shaderTextureChannels;

    // object render permutations
    kdgpu_ext::gltf_holder::GltfRenderPermutations m_gltfRenderPermutations;

    // uniform buffer for vertex shader, camera
    UniformBufferObject<uniform_buffer::configuration, ShaderStageFlagBits::FragmentBit> m_configurationUniformBufferObject = {};

    // results - textures
    TextureTarget m_colorTextureTarget;
    TextureTarget *m_depthTextureTarget = nullptr;

    // result - render target
    RenderTarget m_renderTarget;
};

} // namespace pass::gltf_area_light
