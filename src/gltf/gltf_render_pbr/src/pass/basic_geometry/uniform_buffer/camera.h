#pragma once

#include <glm/mat4x4.hpp>

namespace pass::basic_geometry::uniform_buffer {
struct camera {
  glm::mat4 projection;
  glm::mat4 view;
};
} // namespace pass::geometry::uniform_buffer
