#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <KDGpuExample/simple_example_engine_layer.h>

#include <shader/shader_vertex_fragment.h>
#include <texture_target/texture_target.h>
#include <vertex_buffer_object/vertex_buffer_object.h>

namespace pass::compositing {
class CompositingPass
{
public:
    void initialize(Format swapchainFormat);
    void deinitialize();

    void update(float time);

    void setInputTextures(
            TextureTarget &pbrPassTexture,
            TextureTarget &otherChannelColorTexture,
            TextureTarget &basicGeometryTexture,
            TextureTarget &areaLightTexture);

    void render(CommandRecorder &commandRecorder, TextureView &swapchainView);

private:
    // shader
    kdgpu_ext::graphics::shader::ShaderVertexFragment m_shader;

    // mesh
    kdgpu_ext::graphics::vertex_buffer_object::VertexBufferObject<glm::vec3, glm::vec2> m_quad;

    TextureTarget *m_pbrPassTexture = nullptr;
    TextureTarget *m_sourceOtherChannelColorTexture = nullptr;
    TextureTarget *m_basicGeometryTexture = nullptr;
    TextureTarget *m_areaLightTexture = nullptr;

    // Internal Render Pass setup
    Sampler m_colorOutputSampler;
    PipelineLayout m_pipelineLayout;
    GraphicsPipeline m_graphicsPipeline;
    BindGroup m_bindGroup;
    BindGroupLayout m_bindGroupLayout;
};
} // namespace pass::compositing
