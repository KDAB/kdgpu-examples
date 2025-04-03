#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_texture_coordinate;

layout(location = 0) out vec4 result;

layout (set = 1, binding = 0) uniform sampler2D gltf_color_map;

layout (set = 3, binding = 0) uniform sampler2D test_color_map;
layout (set = 3, binding = 1) uniform sampler2D test_color_map2;

void main()
{
    vec4 extra_channel = texture(test_color_map, in_texture_coordinate);
    vec4 extra_channel2 = texture(test_color_map2, in_texture_coordinate);

    result = texture(gltf_color_map, in_texture_coordinate) ;
    result.r += extra_channel.r * 0.25;
    result.g += extra_channel.g * 0.25;
    result.r += extra_channel2.r * 0.25;
}
