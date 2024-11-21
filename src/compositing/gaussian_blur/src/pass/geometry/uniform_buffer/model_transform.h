/*
This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <glm/glm.hpp>

namespace pass::geometry::uniform_buffer {
struct ModelTransform {
  glm::mat4 modelMatrix;
};
}
