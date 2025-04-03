#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec3 in_vertex_position;
layout(location = 1) in vec2 in_vertex_texture_coordinate;

// outputs
layout(location = 0) out vec2 out_texture_coordinate;

// uniforms
layout(set = 1, binding = 0) uniform camera_t
{
    mat4 projection;
    mat4 view;
}
camera;


void main()
{
    out_texture_coordinate = in_vertex_texture_coordinate;
    gl_Position = camera.projection * camera.view * vec4(in_vertex_position, 1.0);
}
