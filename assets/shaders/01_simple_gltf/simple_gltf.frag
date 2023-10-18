#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 normal;

layout(location = 0) out vec4 fragColor;

// Some hardcoded lighting
const vec3 lightDir = vec3(0.25, 0.5, 1.0);
const vec3 lightColor = vec3(1.0, 1.0, 1.0);
const vec3 ambientColor = vec3(0.1, 0.1, 0.1);

void main()
{
    // An extremely simple directional lighting model, just to give our model some shape.
    vec3 N = normalize(normal);
    vec3 L = normalize(lightDir);
    float NDotL = max(dot(N, L), 0.0);
    vec3 surfaceColor = ambientColor + vec3(NDotL);

    fragColor = vec4(surfaceColor, 1.0);
}
