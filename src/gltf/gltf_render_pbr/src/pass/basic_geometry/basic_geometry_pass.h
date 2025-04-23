/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <tinygltf_helper/camera.h>

#include <texture_target/texture_target.h>

#include <pass/basic_geometry/uniform_buffer/camera.h>

#include <shader/shader_vertex_fragment.h>
#include <texture/texture_set.h>
#include <render_target/render_target.h>
#include <uniform/uniform_buffer_object.h>
#include <vertex_buffer_object/vertex_buffer_object.h>

namespace pass::basic_geometry {
class BasicGeometryPass
{
public:
    void initialize(TextureTarget& depthTexture, Extent2D swapchainExtent);
    void deinitialize();

    void updateViewProjectionMatricesFromCamera(TinyGltfHelper::Camera& camera);

    void render(CommandRecorder& commandRecorder);

    TextureTarget& result_color_texture_target() { return m_colorTextureTarget; }

    kdgpu_ext::graphics::texture::TextureUniformSet* m_textureSet = nullptr;

    std::vector<glm::vec3>& quadVertices() { return m_quad.vertices;}

private:
    // shader
    kdgpu_ext::graphics::shader::ShaderVertexFragment m_shader;

    // mesh
    kdgpu_ext::graphics::vertex_buffer_object::VertexBufferObject<glm::vec3, glm::vec2> m_quad;

    // uniform buffer for vertex shader, camera
    UniformBufferObject<uniform_buffer::camera, ShaderStageFlagBits::VertexBit> m_cameraUniformBufferObject = {};

    Sampler m_colorOutputSampler;

    // results - textures
    TextureTarget m_colorTextureTarget;
    TextureTarget* m_depthTextureTarget = nullptr;

    // result - render target
    RenderTarget m_renderTarget;

    BindGroup m_bindGroup;
    BindGroupLayout m_bindGroupLayout;
    PipelineLayout m_pipelineLayout;
    GraphicsPipeline m_graphicsPipeline;

};

}
