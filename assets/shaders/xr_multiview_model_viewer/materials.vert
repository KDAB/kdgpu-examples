#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
#ifdef TEXCOORD_0_ENABLED
layout(location = 2) in vec2 vertexTexCoord;
#endif

layout(location = 0) out vec3 normal;
#ifdef TEXCOORD_0_ENABLED
layout(location = 1) out vec2 texCoord;
#endif

struct CameraData {
    mat4 view;
    mat4 projection;
};

layout(set = 0, binding = 0) uniform Camera
{
    CameraData data[2];
}
camera;

layout(set = 1, binding = 0) buffer Entity
{
    mat4 model[];
}
entity;

layout(push_constant) uniform PushConstants
{
    mat4 modelOffset;
}
pushConstants;

void main()
{
#ifdef TEXCOORD_0_ENABLED
    texCoord = vertexTexCoord;
#endif
    normal = normalize((camera.data[gl_ViewIndex].view * entity.model[gl_InstanceIndex] * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = camera.data[gl_ViewIndex].projection * camera.data[gl_ViewIndex].view * pushConstants.modelOffset * entity.model[gl_InstanceIndex] * vec4(vertexPosition, 1.0);
}
