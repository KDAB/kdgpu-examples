/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <kdbindings/binding.h>
#include <kdbindings/property.h>

#include <glm/glm.hpp>

#include <tinygltf_helper/tinygltf_helper_export.h>

using namespace KDBindings;

namespace TinyGltfHelper {

class TINYGLTF_HELPER_EXPORT CameraLens
{
public:
    CameraLens();
    ~CameraLens();

    enum class ProjectionType {
        Perspective,
        Orthographic,
    };

    // Common
    Property<float> nearPlane{ 0.1f };
    Property<float> farPlane{ 1000.0f };
    Property<ProjectionType> projectionType{ ProjectionType::Perspective };

    // Readonly
    Property<glm::mat4> projectionMatrix{};

    // Perspective
    Property<float> verticalFieldOfView{ 25.0f };
    Property<float> aspectRatio{ 1.0f };

    // Ortho
    Property<float> left{ -0.5f };
    Property<float> right{ 0.5f };
    Property<float> top{ -0.5f };
    Property<float> bottom{ 0.5f };

    Signal<CameraLens *> destroyed;
    Signal<CameraLens *> changed;

    void setOrthographicProjection(float left, float right, float top, float bottom, float nearPlane, float farPlane);
    void setPerspectiveProjection(float verticalFieldOfView, float aspectRatio, float nearPlane, float farPlane);
};

} // namespace TinyGltfHelper
