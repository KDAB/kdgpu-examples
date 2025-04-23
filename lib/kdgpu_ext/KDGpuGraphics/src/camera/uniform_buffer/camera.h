#pragma once

namespace kdgpu_ext::graphics::camera::uniform_buffer {
struct camera {
    glm::mat4 projection;
    glm::mat4 view;
};

}
