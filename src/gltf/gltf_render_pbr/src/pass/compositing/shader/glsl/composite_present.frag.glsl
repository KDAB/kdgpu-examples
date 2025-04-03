#version 450
#extension GL_ARB_separate_shader_objects: enable

layout (location = 0) in vec2 in_uv_coord;

layout (location = 0) out vec4 out_fragment_color;

layout (set = 0, binding = 0) uniform sampler2D pbr_pass_texture;
layout (set = 0, binding = 1) uniform sampler2D other_channel_pass_texture;
layout (set = 0, binding = 2) uniform sampler2D basic_geometry_pass_texture;
layout (set = 0, binding = 3) uniform sampler2D area_light_pass_texture;
layout (set = 0, binding = 4) uniform sampler2D transmission_pass_texture;

// another way of doing gamma correction
vec3 reinhartTonemap(in vec3 color, in float gamma) {
    return pow(color / (color + vec3(1.0)), vec3(1.0 / gamma));
}

// basic exponent-based gamma correction
vec3 gamma_correction(in vec3 color, in float exponent, in float multiplier)
{
    return vec3(
    pow(color.r, exponent) * multiplier,
    pow(color.g, exponent) * multiplier,
    pow(color.b, exponent) * multiplier
    )
    ;
}

void main()
{
    vec4 color_pbr_channel = texture(pbr_pass_texture, in_uv_coord);
    vec4 color_other_channel = texture(other_channel_pass_texture, in_uv_coord);
    vec4 color_basic_geometry = texture(basic_geometry_pass_texture, in_uv_coord);
    vec4 color_area_light = texture(area_light_pass_texture, in_uv_coord);

    vec3 sum_of_all_channels =
    color_pbr_channel.rgb +
    color_other_channel.rgb +
    color_basic_geometry.rgb +
    color_area_light.rgb
    ;
    vec3 corrected_color = gamma_correction(sum_of_all_channels, 1.2, 2.0);
    out_fragment_color = vec4(corrected_color.rgb, 1.0);
}
