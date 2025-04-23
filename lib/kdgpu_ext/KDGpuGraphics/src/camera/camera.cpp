/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace kdgpu_ext::graphics::camera {

void Camera::initialize(const uint32_t bindingId)
{
    m_cameraUniformBufferObject.init(bindingId);
    m_maintainsUniformBufferObject = true;
}

void Camera::deinitialize()
{
    m_cameraUniformBufferObject = {};
}

const glm::mat4 &Camera::viewMatrix() const
{
    return m_cameraUniformBufferObject.data.view;
}

const glm::mat4 &Camera::projectionMatrix() const
{
    return m_cameraUniformBufferObject.data.projection;
}

void Camera::lookAt(const glm::vec3 &position,
                    const glm::vec3 &viewCenter,
                    const glm::vec3 &up)
{
    m_eyePos = position;
    m_cameraUniformBufferObject.data.view = glm::lookAt(position, viewCenter, up);
    if (m_maintainsUniformBufferObject)
        m_uniformBufferObjectNeedsUpdate = true;
}

void Camera::update()
{
    m_lens.update(m_cameraUniformBufferObject.data.projection);
    if (m_uniformBufferObjectNeedsUpdate) {
        m_cameraUniformBufferObject.upload();
        m_uniformBufferObjectNeedsUpdate = false;
    }
}

} // namespace TinyGltfHelper
