#pragma once

namespace pass::gltf_pbr::uniform_buffer {

// control over material properties
struct configuration {
    glm::vec4 baseColorFactor = {1.0, 1.0, 1.0, 1.0};
    glm::vec4 emissionFactor = {1.0, 1.0, 1.0, 1.0};
    float metallicFactor = 1.0;
    float roughnessFactor = 1.0;
    float iblIntensity = 0.4;
    float directionalLightIntensity = 0.5;
};
} // namespace pass::geometry::uniform_buffer
