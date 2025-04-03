/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <glm/glm.hpp>

#include <tinygltf_helper/tinygltf_helper_export.h>

namespace TinyGltfHelper {

class TINYGLTF_HELPER_EXPORT CameraLens
{
public:
    CameraLens() = default;
    ~CameraLens() = default;

    enum class ProjectionType {
        Perspective,
        Orthographic,
    };

    // Common
    float nearPlane{ 0.1f };
    float farPlane{ 1000.0f };
    ProjectionType projectionType{ ProjectionType::Perspective };

    // Readonly
    glm::mat4 projectionMatrix{};

    // Perspective
    float verticalFieldOfView{ 25.0f };
    float aspectRatio{ 1.0f };

    // Ortho
    float left{ -0.5f };
    float right{ 0.5f };
    float top{ -0.5f };
    float bottom{ 0.5f };

    void setOrthographicProjection(float left, float right, float top, float bottom, float nearPlane, float farPlane);
    void setPerspectiveProjection(float verticalFieldOfView, float aspectRatio, float nearPlane, float farPlane);
    void update();
};

} // namespace TinyGltfHelper
