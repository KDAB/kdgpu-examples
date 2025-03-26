#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoords;

layout(location = 0) out vec2 texCoords;

void main()
{
    texCoords = vertexTexCoords;
    gl_Position = vec4(vertexPosition, 1.0);
}
