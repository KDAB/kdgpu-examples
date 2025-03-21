/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <tinygltf_helper/camera_lens.h>
#include <tinygltf_helper/tinygltf_helper_export.h>

#include <glm/glm.hpp>

#include <kdbindings/property.h>

namespace TinyGltfHelper {

class CameraLens;

// A very simple camera class. No support for being in a hierarchical
// scene graph.
class TINYGLTF_HELPER_EXPORT Camera
{
public:
    Camera();

    CameraLens &lens() noexcept { return m_lens; }

    const KDBindings::Property<glm::mat4> viewMatrix{ glm::mat4(1.0f) };

    void lookAt(const glm::vec3 &position,
                const glm::vec3 &viewCenter,
                const glm::vec3 &up);

private:
    CameraLens m_lens;
};

} // namespace TinyGltfHelper
