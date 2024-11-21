/*
This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGpuExample/simple_example_engine_layer.h>

#include <texture_target/texture_target.h>

#include <uniform/uniform_buffer_object.h>
#include <pass/geometry/uniform_buffer/model_transform.h>
#include <shader/shader_vertex_fragment.h>

class RenderGeometryToTextureRenderPass
{
public:
    void initialize(Device &device, Format depthFormat, TextureTarget *targetTexture);
    void resize(TextureTarget *targetTexture);
    void deinitialize();

    void update();
    void render(CommandRecorder& commandRecorder, TextureView& depthTextureView);

private:
    TextureTarget *m_targetTexture = nullptr;

    kdgpu_ext::graphics::shader::ShaderVertexFragment m_shader;

    // Internal Render Pass setup
    Buffer m_meshVertexBuffer;
    Buffer m_meshIndexBuffer;

    // uniform buffer for vertex shader
    UniformBufferObject<pass::geometry::uniform_buffer::ModelTransform, ShaderStageFlagBits::VertexBit> m_modelTransformObject = {};

    PipelineLayout m_pipelineLayout;
    GraphicsPipeline m_regularGraphicsPipeline;
};
