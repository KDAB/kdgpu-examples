#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_world_normal;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec4 in_world_tangent;

layout(location = 0) out vec4 fragment_color;

// TODO: later on this can be a regular buffer containing more lights
layout(set = 4, binding = 0) uniform Configuration {
    // Light Configuration
    // x: generic light intensity,
    // y: when > 0, two-sided
    // z: specular intensity
    uniform vec4 light_configuration;

    uniform vec4 light_quad_points[4];
    uniform vec4 eye_position;
    uniform vec4 albedo_color_multiplier;
} configuration;

// PBR channels
layout(set = 2, binding = 0) uniform sampler2D base_color_map;
layout(set = 2, binding = 1) uniform sampler2D metallic_roughness_map;
layout(set = 2, binding = 2) uniform sampler2D normal_map;

// Linear Cosine Transform LUTs and Textured light
layout(set = 3, binding = 0) uniform sampler2D ltc_amplification; // ltc_amp
layout(set = 3, binding = 1) uniform sampler2D ltc_matrix; // ltc_mat
layout(set = 3, binding = 2) uniform sampler2D filtered_map_texture;

mat3 mat3_from_columns(vec3 c0, vec3 c1, vec3 c2)
{
    mat3 m = mat3(c0, c1, c2);
    return m;
}

mat3 mat3_from_rows(vec3 c0, vec3 c1, vec3 c2)
{
    mat3 m = mat3(c0, c1, c2);
    m = transpose(m);
    return m;
}

float integrate_edge(vec3 v1, vec3 v2)
{
    float cosTheta = dot(v1, v2);
    cosTheta = clamp(cosTheta, -0.9999, 0.9999);

    float theta = acos(cosTheta);
    float res = cross(v1, v2).z * theta / sin(theta);

    return res;
}

void ClipQuadToHorizon(inout vec3 L[5], out int n)
{
    // detect clipping config
    int config = 0;
    if (L[0].z > 0.0) config += 1;
    if (L[1].z > 0.0) config += 2;
    if (L[2].z > 0.0) config += 4;
    if (L[3].z > 0.0) config += 8;

    // clip
    n = 0;

    if (config == 0)
    {
        // clip all
    }
    else if (config == 1) // V1 clip V2 V3 V4
    {
        n = 3;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[3].z * L[0] + L[0].z * L[3];
    }
    else if (config == 2) // V2 clip V1 V3 V4
    {
        n = 3;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    }
    else if (config == 3) // V1 V2 clip V3 V4
    {
        n = 4;
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
        L[3] = -L[3].z * L[0] + L[0].z * L[3];
    }
    else if (config == 4) // V3 clip V1 V2 V4
    {
        n = 3;
        L[0] = -L[3].z * L[2] + L[2].z * L[3];
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
    }
    else if (config == 5) // V1 V3 clip V2 V4) impossible
    {
        n = 0;
    }
    else if (config == 6) // V2 V3 clip V1 V4
    {
        n = 4;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    }
    else if (config == 7) // V1 V2 V3 clip V4
    {
        n = 5;
        L[4] = -L[3].z * L[0] + L[0].z * L[3];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    }
    else if (config == 8) // V4 clip V1 V2 V3
    {
        n = 3;
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
        L[1] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = L[3];
    }
    else if (config == 9) // V1 V4 clip V2 V3
    {
        n = 4;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[2].z * L[3] + L[3].z * L[2];
    }
    else if (config == 10) // V2 V4 clip V1 V3) impossible
    {
        n = 0;
    }
    else if (config == 11) // V1 V2 V4 clip V3
    {
        n = 5;
        L[4] = L[3];
        L[3] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    }
    else if (config == 12) // V3 V4 clip V1 V2
    {
        n = 4;
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
    }
    else if (config == 13) // V1 V3 V4 clip V2
    {
        n = 5;
        L[4] = L[3];
        L[3] = L[2];
        L[2] = -L[1].z * L[2] + L[2].z * L[1];
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
    }
    else if (config == 14) // V2 V3 V4 clip V1
    {
        n = 5;
        L[4] = -L[0].z * L[3] + L[3].z * L[0];
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
    }
    else if (config == 15) // V1 V2 V3 V4
    {
        n = 4;
    }

    if (n == 3)
    {
        L[3] = L[0];
    }

    if (n == 4)
    {
        L[4] = L[0];
    }
}

vec3 FetchDiffuseFilteredTexture(sampler2D texLightFiltered, vec3 p1, vec3 p2, vec3 p3, vec3 p4)
{
    // area light plane basis
    vec3 V1 = p2 - p1;
    vec3 V2 = p4 - p1;
    vec3 planeOrtho = (cross(V1, V2));
    float planeAreaSquared = dot(planeOrtho, planeOrtho);
    float planeDistxPlaneArea = dot(planeOrtho, p1);

    // orthonormal projection of (0,0,0) in area light space
    vec3 P = planeDistxPlaneArea * planeOrtho / planeAreaSquared - p1;

    // find tex coords of P
    float dot_V1_V2 = dot(V1, V2);
    float inv_dot_V1_V1 = 1.0 / dot(V1, V1);
    vec3 V2_ = V2 - V1 * dot_V1_V2 * inv_dot_V1_V1;
    vec2 Puv;
    Puv.y = dot(V2_, P) / dot(V2_, V2_);
    Puv.x = dot(V1, P) * inv_dot_V1_V1 - dot_V1_V2 * inv_dot_V1_V1 * Puv.y;

    // LOD
    float d = abs(planeDistxPlaneArea) / pow(planeAreaSquared, 0.75);

    return textureLod(texLightFiltered, vec2(0.125, 0.125) + 0.75 * Puv, log(2048.0 * d) / log(3.0)).rgb;
}

vec2 LTC_Coords(float cosTheta, float roughness)
{
    float theta = acos(cosTheta);
    vec2 coords = vec2(roughness, theta / (0.5 * 3.14159));

    const float LUT_SIZE = 32.0;

    // scale and bias coordinates, for correct filtered lookup
    coords = coords * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;

    return coords;
}

mat3 get_ltc_matrix(vec2 coord)
{
    // load inverse matrix
    vec4 c = texture(ltc_matrix, coord);

    // When written, each pixel was written as:
    //   float a = m[0][0];
    //   float b = m[0][2];
    //   float c = m[1][1];
    //   float d = m[2][0];

    //   c.x =  a;
    //   c.y = -b;
    //   c.z = (a - b * d) / c;
    //   c.w = -d;

    // When written, the mat3 m was stored as:
    // a 0 b   inverse   1      0      -b
    // 0 c 0     ==>     0 (a - b*d)/c  0
    // d 0 1            -d      0       a

    // This is reconstructed here
    mat3 Minv = mat3_from_columns(
        vec3(1.0, 0.0, c.y),
        vec3(0.0, c.z, 0.0),
        vec3(c.w, 0.0, c.x)
    );

    return Minv;
}

vec3 FetchNormal(sampler2D nmlSampler, vec2 texcoord, mat3 t2w)
{
    vec3 n = texture(nmlSampler, texcoord).wyz * 2.0 - vec3(1.0,1.0,1.0);

    // Recover z
    n.z = sqrt(max(1.0 - n.x * n.x - n.y * n.y, 0.0));

    return normalize(t2w * n);
}

// See section 3.7 of
// "Linear Efficient Antialiased Displacement and Reflectance Mapping: Supplemental Material"
vec3 CorrectNormal(vec3 n, vec3 v)
{
    if (dot(n, v) < 0.0)
    n = normalize(n - 1.01 * v * dot(n, v));
    return n;
}

// Linearly Transformed Cosine evaluation
vec3 LTC_Evaluate(
    vec3 N, vec3 V, vec3 P, mat3 Minv, vec4 points[4], bool twoSided, sampler2D texFilteredMap)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, R) basis
    Minv = Minv * mat3_from_rows(T1, T2, N);

    // polygon (allocate 5 vertices for clipping)
    vec3 L[5];
    L[0] = (Minv * (points[0].xyz - P));
    L[1] = (Minv * (points[1].xyz - P));
    L[2] = (Minv * (points[2].xyz - P));
    L[3] = (Minv * (points[3].xyz - P));
    L[4] = L[3]; // avoid warning

    vec3 texture_light = vec3(1, 1, 1);
    texture_light = FetchDiffuseFilteredTexture(texFilteredMap, L[0], L[1], L[2], L[3]);

    int n;
    ClipQuadToHorizon(L, n);

    if (n == 0)
    {
        return vec3(0, 0, 0);
    }

    // project onto sphere
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);
    L[4] = normalize(L[4]);

    // integrate
    float sum = 0.0;

    sum += integrate_edge(L[0], L[1]);
    sum += integrate_edge(L[1], L[2]);
    sum += integrate_edge(L[2], L[3]);
    if (n >= 4)
    sum += integrate_edge(L[3], L[4]);
    if (n == 5)
    sum += integrate_edge(L[4], L[0]);

    // note: negated due to winding order
    sum = twoSided ? abs(sum) : max(0.0, -sum);

    // Outgoing radiance (solid angle) for the entire polygon
    vec3 Lo_i = vec3(sum, sum, sum);

    // scale by filtered light color
    Lo_i *= texture_light;

    return Lo_i;
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

void main()
{
    // CONFIGURATION
    float specular_color_monochromatic = 1.0;//configuration.light_configuration.z;

    // two sided?
    bool twoSided;
    twoSided = (configuration.light_configuration.y > 0.0);

    // BASIC MATH
    vec3 posToEye = normalize((configuration.eye_position.xyz - in_world_position));

    vec3 normal_tangent_cross = cross(in_world_normal, in_world_tangent.xyz);
    mat3 tangent_space_matrix = mat3_from_columns(in_world_tangent.xyz, normal_tangent_cross, in_world_normal);


    vec3 normal = normalize(texture(normal_map, in_tex_coord).rgb * float(2.0) - vec3(1.0,1.0,1.0));
    vec3 corrected_normal = normalize(normal * calcWorldSpaceToTangentSpaceMatrix(in_world_normal, in_world_tangent));

    // another way of getting the normal
    //vec3 fetched_normal = FetchNormal(normal_map, in_tex_coord, tangent_space_matrix);
    //vec3 corrected_normal = CorrectNormal(fetched_normal, posToEye);

    vec3 metallic_roughness = texture(metallic_roughness_map, in_tex_coord).rgb;

    float metallic = metallic_roughness.r;
    float roughness = metallic_roughness.g;

    // material colors
    vec3 base_color = texture(base_color_map, in_tex_coord).rgb;
    vec3 diffuse_color = base_color * (1.0 - metallic);
    vec3 specular_color = mix (vec3(specular_color_monochromatic, specular_color_monochromatic, specular_color_monochromatic), base_color, metallic);

    mat3 identity_matrix;
    identity_matrix[0] = vec3(1.0, 0.0, 0.0);
    identity_matrix[1] = vec3(0.0, 1.0, 0.0);
    identity_matrix[2] = vec3(0.0, 0.0, 1.0);


    vec3 evaluated_diffuse_contribution = LTC_Evaluate(
        corrected_normal, posToEye, in_world_position,
        identity_matrix, configuration.light_quad_points, twoSided, filtered_map_texture);

    // scale by general light intensity
    vec3 diffuse_contribution = evaluated_diffuse_contribution * configuration.light_configuration.x;

    // normalize
    diffuse_contribution = (diffuse_contribution / 6.283185);

    // --- SPECULAR ---
    vec2 inverse_matrix_coords = LTC_Coords(dot(corrected_normal, posToEye), roughness);
    mat3 inverse_matrix_specular = get_ltc_matrix(inverse_matrix_coords);
    vec3 specular_contribution = LTC_Evaluate(
        corrected_normal, posToEye,
        in_world_position, inverse_matrix_specular,
        configuration.light_quad_points, twoSided, filtered_map_texture);

    // scale by general light intensity
    specular_contribution = (specular_contribution * configuration.light_configuration.z);// * light_intensity);

    // apply BRDF scale terms (BRDF magnitude and Schlick Fresnel)
    vec2 schlick = texture(ltc_amplification, inverse_matrix_coords).xy;
    specular_contribution *= specular_color * schlick.x + (1.0 - specular_color) * schlick.y;

    // normalize
    specular_contribution = (specular_contribution / 6.283185);

    // write resulting color
    fragment_color = vec4(diffuse_contribution + specular_contribution, 1.0);
}

