/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "camera_lens.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace KDBindings;

namespace TinyGltfHelper {

namespace {

glm::mat4 updateOrthographicProjection(float left, float right,
                                       float top, float bottom,
                                       float nearPlane, float farPlane)
{
    return glm::ortho(left, right, bottom, top, nearPlane, farPlane);
}

glm::mat4 updatePerspectiveProjection(float fov, float aspectRatio,
                                      float nearPlane, float farPlane)
{
    auto m = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);

    // Scale the projection matrix y-axis by -1. GLM uses the OpenGL viewport coordinates
    // whereas in Vulkan it goes in the opposite direction.
    m[1][1] = -m[1][1];

    return m;
}

glm::mat4 updateProjectionMatrix(float fov, float aspectRatio,
                                 float left, float right, float top, float bottom,
                                 float nearPlane, float farPlane,
                                 CameraLens::ProjectionType projectionType)
{
    // Based on which property changed, we can deduce the type of
    // projection the user wants
    if (projectionType == CameraLens::ProjectionType::Orthographic) {
        return updateOrthographicProjection(left, right,
                                            top, bottom,
                                            nearPlane, farPlane);
    }
    // Perspective
    return updatePerspectiveProjection(fov, aspectRatio,
                                       nearPlane, farPlane);
}
KDBINDINGS_DECLARE_FUNCTION(updateProjection, updateProjectionMatrix)

} // namespace

CameraLens::CameraLens()
{
    projectionMatrix = makeBoundProperty(
            updateProjection(verticalFieldOfView, aspectRatio,
                             left, right, top, bottom,
                             nearPlane, farPlane, projectionType));

    // Emit changed signal when projectionMatrix changes
    projectionMatrix.valueChanged().connect([this] {
        changed.emit(this);
    });
}

CameraLens::~CameraLens()
{
    destroyed.emit(this);
}

void CameraLens::setOrthographicProjection(float _left, float _right,
                                           float _top, float _bottom,
                                           float _nearPlane, float _farPlane)
{
    projectionType = ProjectionType::Orthographic;
    left = _left;
    right = _right;
    top = _top;
    bottom = _bottom;
    nearPlane = _nearPlane;
    farPlane = _farPlane;
}

void CameraLens::setPerspectiveProjection(float _fieldOfView, float _aspectRatio,
                                          float _nearPlane, float _farPlane)
{
    projectionType = ProjectionType::Perspective;
    verticalFieldOfView = _fieldOfView;
    aspectRatio = _aspectRatio;
    nearPlane = _nearPlane;
    farPlane = _farPlane;
}

} // namespace TinyGltfHelper
