#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
#ifdef TEXCOORD_0_ENABLED
layout(location = 2) in vec2 texCoord;
#endif

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform Camera
{
    mat4 projection;
    mat4 view;
}
camera;

layout(set = 2, binding = 0) uniform Material
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
}
material;

layout(set = 2, binding = 1) uniform sampler2D baseColorMap;
layout(set = 2, binding = 2) uniform sampler2D metalRoughMap;
layout(set = 2, binding = 3) uniform sampler2D normalMap;
layout(set = 2, binding = 4) uniform sampler2D ambientOcclusionMap;
layout(set = 2, binding = 5) uniform sampler2D emissiveMap;

layout(set = 3, binding = 0) uniform samplerCube envLightIrradiance;
layout(set = 3, binding = 1) uniform samplerCube envLightSpecular;
layout(set = 3, binding = 2) uniform sampler2D brdfLUT;

void main()
{
#ifdef TEXCOORD_0_ENABLED
    vec4 baseColor = texture(baseColorMap, texCoord) * material.baseColorFactor;
    vec2 metallicRoughness = texture(metalRoughMap, texCoord).zy;
    float metalness = metallicRoughness.x * material.metallicFactor;
    float roughness = metallicRoughness.y * material.roughnessFactor;
    float ambientOcclusion = texture(ambientOcclusionMap, texCoord).r;
    vec4 emissive = texture(emissiveMap, texCoord).rgba * material.emissiveFactor;
#else
    vec4 baseColor = material.baseColorFactor;
    float metalness = material.metallicFactor;
    float roughness = material.roughnessFactor;
    float ambientOcclusion = 1.0f;
    vec4 emissive = material.emissiveFactor;
#endif

#ifdef ALPHA_CUTOFF_ENABLED
    if (baseColor.a < material.alphaCutoff)
        discard;
#endif
    mat4 inverseView = inverse(camera.view); // TODO: Send the inverse or position directly
    vec3 cameraPosition = inverseView[3].xyz;
    vec3 worldView = normalize(cameraPosition - worldPosition);

    vec3 envSpecularColor = textureLod(envLightSpecular, worldNormal, 0.0).rgb;
    vec3 envDiffuseColor = texture(envLightIrradiance, worldNormal).rgb;

    vec3 surfaceColor = baseColor.rgb;

    // Debugging
    float width = 40.0;
    if (worldPosition.x > 4.0 / width)
        surfaceColor = baseColor.rgb;
    else if (worldPosition.x > 3.0 / width)
        surfaceColor = metalness.rrr;
    else if (worldPosition.x > 2.0 / width)
        surfaceColor = roughness.rrr;
    else if (worldPosition.x > 1.0 / width)
        surfaceColor = ambientOcclusion.rrr;
    else if (worldPosition.x > 0.0 / width)
        surfaceColor = envSpecularColor;
    else if (worldPosition.x > -1.0 / width)
        surfaceColor = envDiffuseColor;
    else if (worldPosition.x > -2.0 / width)
        surfaceColor = worldNormal;
    else if (worldPosition.x > -3.0 / width)
        surfaceColor = worldPosition;

    fragColor = vec4(surfaceColor, baseColor.a);
}
