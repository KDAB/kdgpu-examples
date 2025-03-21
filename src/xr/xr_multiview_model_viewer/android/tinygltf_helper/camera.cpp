/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace TinyGltfHelper {

Camera::Camera()
{
}

void Camera::lookAt(const glm::vec3 &position,
                    const glm::vec3 &viewCenter,
                    const glm::vec3 &up)
{
    *(const_cast<Property<glm::mat4> *>(&viewMatrix)) = glm::lookAt(position, viewCenter, up);
}

} // namespace TinyGltfHelper
