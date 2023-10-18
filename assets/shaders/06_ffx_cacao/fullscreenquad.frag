#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D colorTexture;

void main()
{
    float ao = texture(colorTexture, texCoord).r;
    fragColor = vec4(vec3(ao), 1.0);
}
