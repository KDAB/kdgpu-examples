#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 texCoords;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D videoTexture;

void main()
{
    vec3 color = texture(videoTexture, texCoords).rgb;
    fragColor = vec4(color, 1.0);
}
