#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
#ifdef TEXCOORD_0_ENABLED
layout(location = 2) in vec2 texCoord;
#endif
layout(location = 3) in vec4 worldTangent;

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

const int MAX_LIGHTS = 8;
const int TYPE_POINT = 0;
const int TYPE_DIRECTIONAL = 1;
const int TYPE_SPOT = 2;

struct Light {
    vec4 color;

    vec3 worldDirection;
    float intensity;

    int type;
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;

    vec3 localDirection;
    float cutOffAngle;

    vec3 worldPosition;
    float padding;
};

const float M_PI = 3.141592653589793;

vec3 sRGBtoLinear(vec3 srgb) { return pow(srgb, vec3(2.2)); }


void calculateLightingCoeffs(const in Light light,
                   const in vec3 wPosition,
                   const in vec3 n,
                   const in vec3 v,
                   out vec3 s,
                   out vec3 h,
                   out float sDotN,
                   out float att)
{
    att = 1.0f;

    if (light.type != TYPE_DIRECTIONAL) {
        // Point and Spot lights
        vec3 sUnnormalized = vec3(light.worldPosition) - wPosition;
        s = normalize(sUnnormalized);

        // Calculate the attenuation factor
        sDotN = dot(s, n);
        if (sDotN > 0.0) {
            float dist = length(sUnnormalized);
            float d2 = dist * dist;
            if (light.quadraticAttenuation > 0.0)
            {
                // recommended attenuation with range based on KHR_lights_punctual extension
                float d2OverR2 = d2/(light.quadraticAttenuation * light.quadraticAttenuation);
                att = max( min( 1.0 - ( d2OverR2 * d2OverR2 ), 1.0 ), 0.0 ) / d2;
            }
            else {
               att = 1.0 / d2;
            }
            att = clamp(att, 0.0, 1.0);
        }
        if (light.type == TYPE_SPOT) {
            // Calculate angular attenuation of spotlight, between 0 and 1.
            // yields 1 inside innerCone, 0 outside outerConeAngle, and value interpolated
            // between 1 and 0 between innerConeAngle and outerConeAngle
            float cd = dot(-light.localDirection, s);
            float angularAttenuation = clamp(cd * radians(light.cutOffAngle), 0.0, 1.0);
            angularAttenuation *= angularAttenuation;
            att *= angularAttenuation;
        }
    }
    else {
        // Directional lights
        // The light direction is in world space already
        s = normalize(-light.worldDirection);
        sDotN = dot(s, n);
    }

    h = normalize(s + v);
}

int mipLevelCount(const in samplerCube cube)
{
   int baseSize = textureSize(cube, 0).x;
   int nMips = int(log2(float(baseSize > 0 ? baseSize : 1))) + 1;
   return nMips;
}

float remapRoughness(const in float roughness)
{
    // As per page 14 of
    // http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
    // we remap the roughness to give a more perceptually linear response
    // of "bluriness" as a function of the roughness specified by the user.
    // r = roughness^2
    const float maxSpecPower = 999999.0;
    const float minRoughness = sqrt(2.0 / (maxSpecPower + 2));
    return max(roughness * roughness, minRoughness);
}

float alphaToMipLevel(float alpha)
{
    float specPower = 2.0 / (alpha * alpha) - 2.0;

    // We use the mip level calculation from Lys' default power drop, which in
    // turn is a slight modification of that used in Marmoset Toolbag. See
    // https://docs.knaldtech.com/doku.php?id=specular_lys for details.
    // For now we assume a max specular power of 999999 which gives
    // maxGlossiness = 1.
    const float k0 = 0.00098;
    const float k1 = 0.9921;
    float glossiness = (pow(2.0, -10.0 / sqrt(specPower)) - k0) / k1;

    // TODO: Optimize by doing this on CPU and set as
    // uniform int envLightSpecularMipLevels say (if present in shader).
    // Lookup the number of mips in the specular envmap
    int mipLevels = mipLevelCount(envLightSpecular);

    // Offset of smallest miplevel we should use (corresponds to specular
    // power of 1). I.e. in the 32x32 sized mip.
    const float mipOffset = 5.0;

    // The final factor is really 1 - g / g_max but as mentioned above g_max
    // is 1 by definition here so we can avoid the division. If we make the
    // max specular power for the spec map configurable, this will need to
    // be handled properly.
    float mipLevel = (mipLevels - 1.0 - mipOffset) * (1.0 - glossiness);
    return mipLevel;
}

float normalDistribution(const in vec3 n, const in vec3 h, const in float alpha)
{
    // See http://graphicrants.blogspot.co.uk/2013/08/specular-brdf-reference.html
    // for a good reference on NDFs and geometric shadowing models.
    // GGX
    float alphaSq = alpha * alpha;
    float nDotH = dot(n, h);
    float factor = nDotH * nDotH * (alphaSq - 1.0) + 1.0;
    return alphaSq / (3.14159 * factor * factor);
}

vec3 fresnelFactor(const in vec3 F0, const in vec3 F90, const in float cosineFactor)
{
    // Calculate the Fresnel effect value
    vec3 F = F0 + (F90 - F0) * pow(clamp(1.0 - cosineFactor, 0.0, 1.0), 5.0);
    return clamp(F, vec3(0.0), vec3(1.0));
}

float geometricModel(const in float lDotN,
                     const in float vDotN,
                     const in float alpha)
{
    // Smith GGX
    float alphaSq = alpha * alpha;
    float termSq = alphaSq + (1.0 - alphaSq) * vDotN * vDotN;
    return 2.0 * vDotN / (vDotN + sqrt(termSq));
}

vec3 diffuseBRDF(const in vec3 diffuseColor)
{
    return diffuseColor / M_PI;
}

float specularBRDF(const in vec3 n,
                   const in vec3 h,
                   const in float sDotN, const in float vDotN,
                   const in float alpha)
{
    float G = geometricModel(sDotN, vDotN, alpha);
    float D = normalDistribution(n, h, alpha);
    return G * D / (4.0 * sDotN * vDotN);
}

mat3 calcWorldSpaceToTangentSpaceMatrix(const in vec3 wNormal, const in vec4 wTangent)
{
    // Make the tangent truly orthogonal to the normal by using Gram-Schmidt.
    // This allows to build the tangentMatrix below by simply transposing the
    // tangent -> eyespace matrix (which would now be orthogonal)
    vec3 wFixedTangent = normalize(wTangent.xyz - dot(wTangent.xyz, wNormal) * wNormal);

    // Calculate binormal vector. No "real" need to renormalize it,
    // as built by crossing two normal vectors.
    // To orient the binormal correctly, use the fourth coordinate of the tangent,
    // which is +1 for a right hand system, and -1 for a left hand system.
    vec3 wBinormal = cross(wNormal, wFixedTangent.xyz) * wTangent.w;

    // Construct matrix to transform from world space to tangent space
    // This is the transpose of the tangentToWorld transformation matrix
    mat3 tangentToWorldMatrix = mat3(wFixedTangent, wBinormal, wNormal);
    mat3 worldToTangentMatrix = transpose(tangentToWorldMatrix);
    return worldToTangentMatrix;
}

vec3 pbrModel(const in Light light,
              const in vec3 wPosition,
              const in vec3 wNormal,
              const in vec3 wView,
              const in vec3 baseColor,
              const in float metalness,
              const in float alpha,
              const in float ambientOcclusion)
{
    // Calculate some useful quantities
    vec3 n = wNormal;
    vec3 s = vec3(0.0);
    vec3 v = wView;
    vec3 h = vec3(0.0);

    float vDotN = dot(v, n);
    float sDotN = 0.0;
    float att = 1.0;

    calculateLightingCoeffs(light, wPosition, n, v, s, h, sDotN, att);

    // This light doesn't contribute anything
    if (sDotN <= 0.0)
        return vec3(0.0);

    vec3 dielectricColor = vec3(0.04);
    vec3 diffuseColor = baseColor * (vec3(1.0) - dielectricColor);
    diffuseColor *= (1.0 - metalness);
    vec3 F0 = mix(dielectricColor, baseColor, metalness); // = specularColor

    // Compute reflectance.
    float reflectance = max(max(F0.r, F0.g), F0.b);
    vec3 F90 = clamp(reflectance * 25.0, 0.0, 1.0) * vec3(1.0);

    // Compute shading terms
    float vDotH = dot(v, h);
    vec3 F = fresnelFactor(F0, F90, vDotH);

    // Analytical (punctual) lighting
    vec3 diffuseContrib = (vec3(1.0) - F) * diffuseBRDF(diffuseColor);
    vec3 specularContrib = F * specularBRDF(n, h, sDotN, vDotN, alpha);

    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    vec3 color = att * sDotN * light.intensity * light.color.rgb
                 * (diffuseContrib + specularContrib);

    return color;
}

vec3 pbrIblModel(const in vec3 wNormal,
                 const in vec3 wView,
                 const in vec3 baseColor,
                 const in float metalness,
                 const in float alpha,
                 const in float ambientOcclusion)
{
    // Calculate reflection direction of view vector about surface normal
    // vector in world space. This is used in the fragment shader to sample
    // from the environment textures for a light source. This is equivalent
    // to the l vector for punctual light sources. Armed with this, calculate
    // the usual factors needed
    vec3 n = wNormal;
    vec3 l = -reflect(wView, n);
    vec3 v = wView;
    float vDotN = clamp(dot(v, n), 0.0, 1.0);

    // Calculate diffuse and specular (F0) colors
    vec3 dielectricColor = vec3(0.04);
    vec3 diffuseColor = baseColor * (vec3(1.0) - dielectricColor);
    diffuseColor *= (1.0 - metalness);
    vec3 F0 = mix(dielectricColor, baseColor, metalness); // = specularColor

    vec2 brdfUV = clamp(vec2(vDotN, 1.0 - sqrt(alpha)), vec2(0.0), vec2(1.0));
    vec2 brdf = texture(brdfLUT, brdfUV).rg;

    vec3 diffuseLight = texture(envLightIrradiance, l).rgb;
    float lod = alphaToMipLevel(alpha);
//#define DEBUG_SPECULAR_LODS
#ifdef DEBUG_SPECULAR_LODS
    if (lod > 7.0)
        return vec3(1.0, 0.0, 0.0);
    else if (lod > 6.0)
        return vec3(1.0, 0.333, 0.0);
    else if (lod > 5.0)
        return vec3(1.0, 1.0, 0.0);
    else if (lod > 4.0)
        return vec3(0.666, 1.0, 0.0);
    else if (lod > 3.0)
        return vec3(0.0, 1.0, 0.666);
    else if (lod > 2.0)
        return vec3(0.0, 0.666, 1.0);
    else if (lod > 1.0)
        return vec3(0.0, 0.0, 1.0);
    else if (lod > 0.0)
        return vec3(1.0, 0.0, 1.0);
#endif
    vec3 specularLight = textureLod(envLightSpecular, l, lod).rgb;

    vec3 diffuse = diffuseLight * diffuseColor;
    vec3 specular = specularLight * (F0 * brdf.x + brdf.y);

    return diffuse + specular;
}

vec3 pbrMetalRoughFunction(const in vec4 baseColor,
                              const in float metalness,
                              const in float roughness,
                              const in float ambientOcclusion,
                              const in vec4 emissive,
                              const in vec3 worldPosition,
                              const in vec3 worldView,
                              const in vec3 worldNormal)
{
    vec3 cLinear = vec3(0.0);

    // Remap roughness for a perceptually more linear correspondence
    float alpha = remapRoughness(roughness);

    // Add up the contributions from image based lighting
    cLinear += pbrIblModel(worldNormal,
                                worldView,
                                baseColor.rgb,
                                metalness,
                                alpha,
                                ambientOcclusion);

    // A hardcoded light
    Light light;
    light.color = vec4(1.0, 1.0f, 1.0, 1.0);
    light.worldDirection = normalize(vec3(0.2, -1.0, 0.0));
    light.intensity = 5.0;
    light.type = TYPE_DIRECTIONAL;
    light.quadraticAttenuation = 1.0;
    light.linearAttenuation = 0.0;
    light.constantAttenuation = 0.0;

    cLinear += pbrModel(light,
                        worldPosition,
                        worldNormal,
                        worldView,
                        baseColor.rgb,
                        metalness,
                        alpha,
                        ambientOcclusion);


    // Apply ambient occlusion and emissive channels
    cLinear *= ambientOcclusion;
    cLinear += emissive.rgb;

    return cLinear;
}

vec3 reinhartTonemap(in vec3 color, in float gamma) {
    return pow(color / (color + vec3(1.0)), vec3(1.0 / gamma));
}

void main()
{
#ifdef TEXCOORD_0_ENABLED
    vec4 baseColor = texture(baseColorMap, texCoord) * material.baseColorFactor;
    vec2 metallicRoughness = texture(metalRoughMap, texCoord).zy;
    float metalness = metallicRoughness.x * material.metallicFactor;
    float roughness = metallicRoughness.y * material.roughnessFactor;
    float ambientOcclusion = texture(ambientOcclusionMap, texCoord).r;
    vec4 emissive = texture(emissiveMap, texCoord).rgba * material.emissiveFactor;
    vec3 normal = normalize(texture(normalMap, texCoord).rgb * float(2.0) - vec3(1.0,1.0,1.0));
    normal = normalize(normal * calcWorldSpaceToTangentSpaceMatrix(worldNormal, worldTangent));
#else
    vec4 baseColor = material.baseColorFactor;
    float metalness = material.metallicFactor;
    float roughness = material.roughnessFactor;
    float ambientOcclusion = 1.0f;
    vec4 emissive = material.emissiveFactor;
    vec3 normal = worldNormal;
#endif

#ifdef ALPHA_CUTOFF_ENABLED
    if (baseColor.a < material.alphaCutoff)
        discard;
#endif
    mat4 inverseView = inverse(camera.view); // TODO: Send the inverse or position directly
    vec3 cameraPosition = inverseView[3].xyz;
    vec3 worldView = normalize(cameraPosition - worldPosition);

    vec3 envSpecularColor = textureLod(envLightSpecular, normal, 0.0).rgb;
    vec3 envDiffuseColor = texture(envLightIrradiance, normal).rgb;

    vec3 surfaceColor = baseColor.rgb;

    // Debugging
    /*
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
    */

    surfaceColor = pbrMetalRoughFunction(
                         baseColor,
                         metalness,
                         roughness,
                         ambientOcclusion,
                         emissive,
                         worldPosition,
                         worldView,
                         normal);

    surfaceColor = reinhartTonemap(surfaceColor, 2.2);

    fragColor = vec4(surfaceColor, baseColor.a);
}
