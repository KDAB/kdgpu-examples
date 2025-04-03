#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_uv_coord;

layout(location = 0) out vec4 out_fragment_color;

layout(set = 0, binding = 0) uniform sampler2D albedo_pass_texture;
layout(set = 0, binding = 1) uniform sampler2D other_channel_pass_texture;

void main()
{
    vec4 color_albedo = texture(albedo_pass_texture, in_uv_coord);
    vec4 color_other_channel = texture(other_channel_pass_texture, in_uv_coord);

    out_fragment_color = vec4(
        color_albedo.r + color_other_channel.r,
        color_albedo.g + color_other_channel.g,
        color_albedo.b + color_other_channel.b,
        1.0
    );
}
