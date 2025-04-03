#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec3 in_vertex_position;
layout(location = 1) in vec2 in_vertex_texture_coordinate;

// outputs
layout(location = 0) out vec2 out_texture_coordinate;
layout(location = 1) out float out_intensity;

// uniforms
layout(set = 2, binding = 0) uniform camera_t
{
    mat4 projection;
    mat4 view;
}
camera;

layout(set = 3, binding = 0) uniform configuration_t
{
    float intensity;
}
configuration;

layout(set = 0, binding = 0) uniform node_transform_t
{
    mat4 model_matrix;
}
node_transform;

void main()
{
    out_intensity = configuration.intensity;
    out_texture_coordinate = in_vertex_texture_coordinate;
    gl_Position = camera.projection * camera.view * node_transform.model_matrix * vec4(in_vertex_position, 1.0);
}
