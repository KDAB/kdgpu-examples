/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <camera/camera_lens.h>
#include <uniform/uniform_buffer_object.h>
#include <camera/uniform_buffer/camera.h>

#include <glm/glm.hpp>

namespace kdgpu_ext::graphics::camera {

class CameraLens;

// A very simple camera class. No support for being in a hierarchical
// scene graph.
class Camera
{
public:

    void initialize(uint32_t bindingId);

    void deinitialize();

    CameraLens &lens() noexcept { return m_lens; }

    const glm::mat4 &viewMatrix() const;

    const glm::mat4 &projectionMatrix() const;

    void lookAt(const glm::vec3 &position,
                const glm::vec3 &viewCenter,
                const glm::vec3 &up);

    glm::vec3 eyePosition() const { return m_eyePos; }

    void update();

    [[nodiscard]] const KDGpu::BindGroup& bindGroup() const { return m_cameraUniformBufferObject.bindGroup(); }
    [[nodiscard]] const KDGpu::BindGroupLayout& bindGroupLayout() const { return m_cameraUniformBufferObject.bindGroupLayout(); }

private:
    CameraLens m_lens;
    glm::vec3 m_eyePos = {};

    bool m_maintainsUniformBufferObject = false;
    bool m_uniformBufferObjectNeedsUpdate = false;
    UniformBufferObject<uniform_buffer::camera, KDGpu::ShaderStageFlagBits::VertexBit> m_cameraUniformBufferObject = {};
};

} // namespace TinyGltfHelper
