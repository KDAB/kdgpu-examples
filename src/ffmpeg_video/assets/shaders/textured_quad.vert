#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoords;

layout(location = 0) out vec2 texCoords;

layout(push_constant) uniform PushConstants
{
    float xScaling;
    float yScaling;
}
pushConstants;

void main()
{
    texCoords = vertexTexCoords;
    vec2 correctPos = vec2(vertexPosition.xy) * vec2(pushConstants.xScaling, pushConstants.yScaling);
    gl_Position = vec4(vec3(correctPos, vertexPosition.z), 1.0);
}
