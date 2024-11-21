/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#version 450
#extension GL_ARB_separate_shader_objects: enable

layout (location = 0) in vec2 texCoord;

layout (location = 0) out vec4 fragColor;

layout (set = 0, binding = 0) uniform sampler2D colorTexture;

layout (push_constant) uniform PushConstants {
    float blurScale;
    float blurStrength;
} pushConstants;

layout (constant_id = 0) const int blurdirection = 0;

void main()
{
    float weight[5];
    weight[0] = 0.227027;
    weight[1] = 0.1945946;
    weight[2] = 0.1216216;
    weight[3] = 0.054054;
    weight[4] = 0.016216;

    vec2 tex_offset = vec2(1.0 / textureSize(colorTexture, 0) * pushConstants.blurScale); // gets size of single texel
    vec3 result = texture(colorTexture, texCoord).rgb * weight[0]; // current fragment's contribution
    for (int i = 1; i < 5; ++i)
    {
        if (blurdirection == 0)
        {
            // Horizontal
            result += texture(colorTexture, texCoord + vec2(tex_offset.x * i, 0.0)).rgb * weight[i] * pushConstants.blurStrength;
            result += texture(colorTexture, texCoord - vec2(tex_offset.x * i, 0.0)).rgb * weight[i] * pushConstants.blurStrength;
        }
        else
        {
            // Vertical
            result += texture(colorTexture, texCoord + vec2(0.0, tex_offset.y * i)).rgb * weight[i] * pushConstants.blurStrength;
            result += texture(colorTexture, texCoord - vec2(0.0, tex_offset.y * i)).rgb * weight[i] * pushConstants.blurStrength;
        }
    }

    fragColor = vec4(result, 1.0);
}
