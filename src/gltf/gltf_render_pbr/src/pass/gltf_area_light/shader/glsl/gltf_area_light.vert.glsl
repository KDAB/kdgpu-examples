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

layout(set = 0, binding = 0) uniform Camera
{
    mat4 projection;
    mat4 view;
}
camera;

layout(set = 1, binding = 0) uniform NodeTransform
{
    mat4 model;
}
node_transform;

void main()
{
    out_tex_coord = in_vertex_tex_coord;
    out_position = (node_transform.model * vec4(in_vertex_position, 1.0)).xyz;
    mat3 normalMatrix = mat3(transpose(inverse(node_transform.model)));
    out_normal = normalize(normalMatrix * in_vertex_normal);
    out_tangent = vec4(normalize(node_transform.model * vec4(in_vertex_tangent.xyz, float(0.0))).xyz, in_vertex_tangent.w);
    gl_Position = camera.projection * camera.view * node_transform.model * vec4(in_vertex_position, 1.0);
}
