#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_texture_coordinate;

layout(location = 0) out vec4 result;

layout (set = 1, binding = 0) uniform sampler2D gltf_other_channel_map;

void main()
{
    vec4 color = texture(gltf_other_channel_map, in_texture_coordinate * 5.0);
    result = vec4(0.0, 0.0, color.r, 1.0);
}
