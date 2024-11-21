#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexColor;

layout(location = 0) out vec3 color;

layout(set = 0, binding = 0) uniform model_transform
{
    mat4 modelMatrix;
}
transform;

void main()
{
    color = vertexColor;
    gl_Position = transform.modelMatrix * vec4(vertexPosition, 1.0);
}
