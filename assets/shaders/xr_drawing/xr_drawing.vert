#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

layout(location = 0) out vec3 normal;
layout(location = 1) out vec4 color;

struct CameraData {
    mat4 view;
    mat4 projection;
};

layout(set = 0, binding = 0) uniform Camera
{
    CameraData data;
}
camera;

struct ShapeData {
    mat4 model;
    vec4 color;
};

layout(set = 1, binding = 0) buffer Entity
{
    ShapeData data[];
}
entity;

layout(set = 2, binding = 0) uniform Entity
{
    mat4 modelMatrix;
}
modelMatrix;

void main()
{
    mat4 combinedModel = modelMatrix.modelMatrix * entity.data[gl_InstanceIndex].model;

    normal = normalize((camera.data.view * combinedModel * vec4(vertexNormal, 0.0)).xyz);
    color = entity.data[gl_InstanceIndex].color;
    gl_Position = camera.data.projection * camera.data.view * combinedModel * vec4(vertexPosition, 1.0);
}
