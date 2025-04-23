/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "camera_controller.h"

#include <camera/camera.h>

#include <KDGpuExample/engine.h>

#include <KDGui/gui_events.h>
#include <KDFoundation/core_application.h>

#include <kdbindings/binding.h>

#include <spdlog/spdlog.h>

#include <glm/gtc/noise.hpp>

#include <math.h>

using namespace KDGui;
using namespace KDFoundation;

namespace TinyGltfHelper {

CameraController::CameraController()
    : Object()
{
    // When we toggle the enabled property to start taking notice of the
    // input controls we don't want to calculate the mouse offset on the
    // first mouseMoveEvent because the mouse may have moved a long way
    // since we were last enabled.
    enabled.valueChanged().connect([this](const bool &enabled) {
        if (enabled)
            m_ignoreFirstMouseMove = true;
    });

    camera.valueChanged().connect([this](const kdgpu_ext::graphics::camera::Camera *camera) {
        if (camera) {
            this->update();
            this->apply();
        }
    });
}

void CameraController::update()
{
    // Get time since last frame so we can move with consistent speed.
    assert(m_engine);
    const auto dt = m_engine->deltaTimeSeconds();
    const auto t_NS = m_engine->simulationTime();
    const auto t = t_NS.count() * 1.0e-9f;

    // Reduce the trauma by our decay speed
    trauma = std::max(0.0f, trauma() - traumaDecaySpeed() * dt);
    float yawShake = shake() * maxShakeAngle() * glm::simplex(glm::vec2(0.0f, t * shakeSpeed()));
    float pitchShake = shake() * maxShakeAngle() * glm::simplex(glm::vec2(1.0f, t * shakeSpeed()));

    // Update camera yaw and pitch
    glm::vec2 offset = mouseSensitivity() * static_cast<glm::vec2>(m_state.mouseOffset);
    m_state.mouseOffset = glm::i64vec2{ 0, 0 };

    yaw = yaw() + offset.x;

    // Constrain pitch so we can't go above almost straight up or down
    float newPitch = pitch() + offset.y;
    if (newPitch > 89.0f)
        newPitch = 89.0f;
    if (newPitch < -89.0f)
        newPitch = -89.0f;
    pitch = newPitch;

    // Update the camera direction vectors from the new yaw and pitch values
    const float theta = glm::radians(yaw() + yawShake);
    const float phi = glm::radians(pitch() + pitchShake);
    glm::vec3 forward{
        std::cos(theta) * std::cos(phi),
        std::sin(phi),
        std::sin(theta) * std::cos(phi)
    };
    m_forward = glm::normalize(forward);
    m_right = glm::normalize(glm::cross(m_forward, worldUp()));
    m_up = glm::normalize(glm::cross(m_right, m_forward));

    // Update the camera position by moving along it forward, right and up
    // direction vectors.
    const float speed = dt * moveSpeed();
    glm::vec3 deltaPos{ 0.0f };
    if (m_state.forwardKeyPressed)
        deltaPos += speed * m_forward;
    if (m_state.backKeyPressed)
        deltaPos -= speed * m_forward;
    if (m_state.rightKeyPressed)
        deltaPos += speed * m_right;
    if (m_state.leftKeyPressed)
        deltaPos -= speed * m_right;
    if (m_state.upKeyPressed)
        deltaPos += speed * m_up;
    if (m_state.downKeyPressed)
        deltaPos -= speed * m_up;

    pos = pos() + deltaPos;

    apply();
}

void CameraController::apply()
{
    camera()->lookAt(pos(), pos() + m_forward, m_up);
}

void CameraController::event(EventReceiver *target, Event *ev)
{
    if (!enabled())
        return;

    switch (ev->type()) {
    case Event::Type::MouseMove: {
        mouseMoveEvent(static_cast<MouseMoveEvent *>(ev));
        break;
    }

    case Event::Type::KeyPress: {
        keyPressEvent(static_cast<KeyPressEvent *>(ev));
        break;
    }

    case Event::Type::KeyRelease: {
        keyReleaseEvent(static_cast<KeyReleaseEvent *>(ev));
        break;
    }
    default:
        break;
    }
}

void CameraController::mouseMoveEvent(MouseMoveEvent *ev)
{
    if (m_ignoreFirstMouseMove) {
        m_state.previousMousePos = glm::i64vec2(ev->xPos(), ev->yPos());
        m_ignoreFirstMouseMove = false;
        return;
    }

    m_state.mouseOffset = glm::i64vec2(
            ev->xPos() - m_state.previousMousePos.x,
            m_state.previousMousePos.y - ev->yPos());
    m_state.previousMousePos = glm::i64vec2(ev->xPos(), ev->yPos());
}

void CameraController::keyPressEvent(KeyPressEvent *ev)
{
    // TODO: Make key -> action mapping configurable
    switch (ev->key()) {
    case Key_W: {
        m_state.forwardKeyPressed = true;
        break;
    }

    case Key_S: {
        m_state.backKeyPressed = true;
        break;
    }

    case Key_A: {
        m_state.leftKeyPressed = true;
        break;
    }

    case Key_D: {
        m_state.rightKeyPressed = true;
        break;
    }

    case Key_PageUp: {
        m_state.upKeyPressed = true;
        break;
    }

    case Key_PageDown: {
        m_state.downKeyPressed = true;
        break;
    }

    case Key_Space: {
        trauma = std::min(1.0f, trauma() + traumaHitAmount());
        break;
    }

    default: {
        break;
    }
    }
}

void CameraController::keyReleaseEvent(KeyReleaseEvent *ev)
{
    switch (ev->key()) {
    case Key_W: {
        m_state.forwardKeyPressed = false;
        break;
    }

    case Key_S: {
        m_state.backKeyPressed = false;
        break;
    }

    case Key_A: {
        m_state.leftKeyPressed = false;
        break;
    }

    case Key_D: {
        m_state.rightKeyPressed = false;
        break;
    }

    case Key_PageUp: {
        m_state.upKeyPressed = false;
        break;
    }

    case Key_PageDown: {
        m_state.downKeyPressed = false;
        break;
    }

    default: {
        break;
    }
    }
}

} // namespace TinyGltfHelper
