#pragma once

#include <glm/vec4.hpp>

namespace pass::gltf_area_light::uniform_buffer {

struct configuration {
    // Light Configuration
    // x: generic light intensity,
    // y: when > 0, two-sided
    // z: specular intensity
    glm::vec4 lightConfiguration = {1.0f, 1.0f, 1.0f, 0.0f};

    glm::vec4 lightQuadPoints[4];
    glm::vec4 eyePosition;
    glm::vec4 albedoColorMultiplier = {1.0f, 1.0f, 1.0f, 1.0f};
};
}
