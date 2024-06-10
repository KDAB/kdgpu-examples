#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 normal;
#ifdef TEXCOORD_0_ENABLED
layout(location = 1) in vec2 texCoord;
#endif

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform Material
{
    vec4 baseColorFactor;
    float alphaCutoff;
}
material;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;

// Some hardcoded lighting
const vec3 lightDir = vec3(0.25, 0.5, 1.0);
const vec3 lightColor = vec3(1.0, 1.0, 1.0);
const vec3 ambientColor = vec3(0.1, 0.1, 0.1);

void main()
{
#ifdef TEXCOORD_0_ENABLED
    vec4 baseColor = texture(baseColorTexture, texCoord) * material.baseColorFactor;
#else
    vec4 baseColor = material.baseColorFactor;
#endif

#ifdef ALPHA_CUTOFF_ENABLED
    if (baseColor.a < material.alphaCutoff)
        discard;
#endif

    // An extremely simple directional lighting model, just to give our model some shape.
    vec3 N = normalize(normal);
    vec3 L = normalize(lightDir);
    float NDotL = max(dot(N, L), 0.0);
    vec3 surfaceColor = (baseColor.rgb * ambientColor) + (baseColor.rgb * vec3(NDotL));

    fragColor = vec4(surfaceColor, baseColor.a);
}
