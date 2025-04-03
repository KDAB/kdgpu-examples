#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_texture_coordinate;

layout(location = 0) out vec4 result;

layout (set = 0, binding = 0) uniform sampler2D gltf_other_channel_map;

void main()
{
    vec4 color = texture(gltf_other_channel_map, in_texture_coordinate);
    result = vec4(color.rgb, 1.0);
    result.r += 0.1;
}
