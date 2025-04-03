#pragma once

namespace pass::geometry::uniform_buffer {
struct camera {
    glm::mat4 projection;
    glm::mat4 view;
};
} // namespace pass::geometry::uniform_buffer
