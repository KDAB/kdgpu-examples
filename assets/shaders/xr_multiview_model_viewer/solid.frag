#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    vec3 color;
} pushConstants;

void main()
{
    fragColor = vec4(pushConstants.color, 1.0);
}
