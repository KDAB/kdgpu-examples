#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
#ifdef TEXCOORD_0_ENABLED
layout(location = 2) in vec2 vertexTexCoord;
#endif
layout(location = 3) in vec4 vertexTangent;

layout(location = 0) out vec3 worldPosition;
layout(location = 1) out vec3 worldNormal;
#ifdef TEXCOORD_0_ENABLED
layout(location = 2) out vec2 texCoord;
#endif
layout(location = 3) out vec4 worldTangent;

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
    worldPosition = (entity.model[gl_InstanceIndex] * vec4(vertexPosition, 1.0)).xyz;
    mat3 normalMatrix = mat3(transpose(inverse(entity.model[gl_InstanceIndex])));
    worldNormal = normalize(normalMatrix * vertexNormal);
    worldTangent = vec4(normalize(entity.model[gl_InstanceIndex] * vec4(vertexTangent.xyz, float(0.0))).xyz, vertexTangent.w);
    gl_Position = camera.projection * camera.view * entity.model[gl_InstanceIndex] * vec4(vertexPosition, 1.0);
}
