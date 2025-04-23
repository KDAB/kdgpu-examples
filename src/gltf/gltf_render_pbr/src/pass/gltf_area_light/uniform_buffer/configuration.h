/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

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
