#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_vertex_position;
layout(location = 1) in vec3 in_vertex_normal;
layout(location = 2) in vec2 in_vertex_tex_coord;
layout(location = 3) in vec4 in_vertex_tangent;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_tex_coord;
layout(location = 3) out vec4 out_tangent;
layout(location = 4) out mat4 out_view_matrix_inversed;

layout(set = 0, binding = 0) uniform Camera
{
    mat4 projection;
    mat4 view;
}
camera;

layout(set = 1, binding = 0) uniform Entity
{
    mat4 model;
}
entity;

void main()
{
    out_view_matrix_inversed = inverse(camera.view);
    out_tex_coord = in_vertex_tex_coord;
    out_position = (entity.model * vec4(in_vertex_position, 1.0)).xyz;
    mat3 normalMatrix = mat3(transpose(inverse(entity.model)));
    out_normal = normalize(normalMatrix * in_vertex_normal);
    out_tangent = vec4(normalize(entity.model * vec4(in_vertex_tangent.xyz, float(0.0))).xyz, in_vertex_tangent.w);
    gl_Position = camera.projection * camera.view * entity.model * vec4(in_vertex_position, 1.0);
}
