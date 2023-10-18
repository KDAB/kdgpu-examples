#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
#ifdef TEXCOORD_0_ENABLED
layout(location = 2) in vec2 vertexTexCoord;
#endif

layout(location = 0) out vec3 normal;
#ifdef TEXCOORD_0_ENABLED
layout(location = 1) out vec2 texCoord;
#endif

layout(set = 0, binding = 0) uniform Camera
{
    mat4 projection;
    mat4 view;
}
camera;

layout(set = 1, binding = 0) buffer Entity
{
    mat4 model[];
}
entity;

void main()
{
#ifdef TEXCOORD_0_ENABLED
    texCoord = vertexTexCoord;
#endif
    normal = normalize((camera.view * entity.model[gl_InstanceIndex] * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = camera.projection * camera.view * entity.model[gl_InstanceIndex] * vec4(vertexPosition, 1.0);
}
