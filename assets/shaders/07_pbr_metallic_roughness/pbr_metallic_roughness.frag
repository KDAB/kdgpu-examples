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
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
}
material;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 2, binding = 2) uniform sampler2D occlusionRoughnessMetallicTexture;

layout (set = 3, binding = 0) uniform samplerCube environmentLightDiffuse;
layout (set = 3, binding = 1) uniform samplerCube environmentLightSpecular;
layout (set = 3, binding = 2) uniform sampler2D lutGGX;

void main()
{
#ifdef TEXCOORD_0_ENABLED
    vec4 baseColor = texture(baseColorTexture, texCoord) * material.baseColorFactor;
    vec3 occlusionRoughnessMetallic = texture(occlusionRoughnessMetallicTexture, texCoord).xyz;
    occlusionRoughnessMetallic.y *= material.roughnessFactor;
    occlusionRoughnessMetallic.z *= material.metallicFactor;
#else
    vec4 baseColor = material.baseColorFactor;
#endif

#ifdef ALPHA_CUTOFF_ENABLED
    if (baseColor.a < material.alphaCutoff)
        discard;
#endif
    mat4 inverseView = inverse(camera.view); // TODO: Send the inverse or position directly
    vec3 cameraPosition = inverseView[3].xyz;

    vec3 worldView = normalize(cameraPosition - worldPosition);
    vec3 surfaceColor = worldPosition;

    surfaceColor = textureLod(environmentLightSpecular, worldNormal, 0.0).rgb;
    //surfaceColor = texture(environmentLightDiffuse, worldNormal).rgb;
    //surfaceColor = worldNormal;
    //surfaceColor = baseColor.rgb;
#ifdef TEXCOORD_0_ENABLED
    //surfaceColor = occlusionRoughnessMetallic.bbb;
#endif
    fragColor = vec4(surfaceColor, baseColor.a);
}
