/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <glm/mat4x4.hpp>

namespace pass::basic_geometry::uniform_buffer {
struct camera {
  glm::mat4 projection;
  glm::mat4 view;
};
} // namespace pass::geometry::uniform_buffer
