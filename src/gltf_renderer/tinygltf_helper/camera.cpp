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
