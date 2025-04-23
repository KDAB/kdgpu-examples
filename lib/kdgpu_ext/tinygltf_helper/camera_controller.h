/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDFoundation/object.h>

#include <kdbindings/binding.h>
#include <kdbindings/property.h>

#include <tinygltf_helper/tinygltf_helper_export.h>

#include <glm/glm.hpp>

#include <chrono>

namespace KDGpuExample {
class Engine;
}

namespace KDGui {
class KeyPressEvent;
class KeyReleaseEvent;
class MouseMoveEvent;
} // namespace KDGui

namespace kdgpu_ext::graphics::camera {
class Camera;
}

namespace TinyGltfHelper {


class TINYGLTF_HELPER_EXPORT CameraController : public KDFoundation::Object
{
public:
    // Camera shake looks more realistic if the amount of shake is proportional to
    // the square (or cube) of the magnitude of the impact. This stems from
    // the spring-like decay we are familiar with.
    KDBindings::Property<float> trauma{ 0.0f };
    const KDBindings::Property<float> shake{ KDBindings::makeBoundProperty(trauma * trauma) };

    KDBindings::Property<float> traumaHitAmount{ 0.5f };
    KDBindings::Property<float> traumaDecaySpeed{ 0.9f };
    KDBindings::Property<float> maxShakeAngle{ 2.5f }; // Degrees
    KDBindings::Property<float> shakeSpeed{ 8.0f }; // Speed we scale simplex noise by

    KDBindings::Property<kdgpu_ext::graphics::camera::Camera *> camera{ nullptr };
    KDBindings::Property<bool> enabled{ false };
    KDBindings::Property<float> moveSpeed{ 10.0f };
    KDBindings::Property<float> mouseSensitivity{ 0.1f };
    KDBindings::Property<glm::vec3> worldUp{ glm::vec3(0.0f, 1.0f, 0.0f) };

    KDBindings::Property<glm::vec3> pos{ glm::vec3(0.0f, 0.0f, 400.0f) };
    KDBindings::Property<float> yaw{ -90.0f }; // Rotation about up-axis
    KDBindings::Property<float> pitch{ 0.0f }; // Rotation above the "ground plane"

    CameraController();

    void setEngine(const KDGpuExample::Engine *engine) { m_engine = engine; }

    void update();
    void apply();

    void event(KDFoundation::EventReceiver *target, KDFoundation::Event *ev) override;

protected:
    void mouseMoveEvent(KDGui::MouseMoveEvent *ev);
    void keyPressEvent(KDGui::KeyPressEvent *ev);
    void keyReleaseEvent(KDGui::KeyReleaseEvent *ev);

    struct IoState {
        bool leftKeyPressed{ false };
        bool rightKeyPressed{ false };
        bool forwardKeyPressed{ false };
        bool backKeyPressed{ false };
        bool upKeyPressed{ false };
        bool downKeyPressed{ false };
        glm::i64vec2 previousMousePos{ 0, 0 };
        glm::i64vec2 mouseOffset{ 0, 0 };
    };

    IoState m_state;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTime;
    bool m_ignoreFirstMouseMove{ true };

    // Begin looking down the -ve z-axis with y as up (as common in OpenGL)
    glm::vec3 m_forward{ glm::vec3(0.0f, 0.0f, -1.0f) };
    glm::vec3 m_right{ glm::vec3(1.0f, 0.0f, 0.0f) };
    glm::vec3 m_up{ glm::vec3(0.0f, 1.0f, 0.0f) };

    const KDGpuExample::Engine *m_engine{ nullptr };
};

} // namespace TinyGltfHelper
