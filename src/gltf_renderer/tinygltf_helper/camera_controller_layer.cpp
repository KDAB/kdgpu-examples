#include "camera_controller_layer.h"

#include <KDGui/window.h>

#include <KDGui/gui_events.h>

#include <spdlog/spdlog.h>

namespace TinyGltfHelper {

void CameraControllerLayer::onAttached()
{
    m_cameraController.setEngine(engine());
}

void CameraControllerLayer::onDetached()
{
    m_cameraController.enabled = false;
    m_cameraController.setEngine(nullptr);
}

void CameraControllerLayer::update()
{
    if (m_cameraController.enabled())
        m_cameraController.update();
}

void CameraControllerLayer::event(KDFoundation::EventReceiver *target, KDFoundation::Event *ev)
{
    if (ev->type() == KDFoundation::Event::Type::KeyPress) {
        auto keyPressEvent = static_cast<KDGui::KeyPressEvent *>(ev);
        if (keyPressEvent->key() == KDGui::Key_QuoteLeft) {
            m_cameraController.enabled = !m_cameraController.enabled();
            if (m_cameraController.enabled() == true)
                SPDLOG_INFO("Camera controller enabled");
            else
                SPDLOG_INFO("Camera controller disabled");

            window()->cursorEnabled = !window()->cursorEnabled();
            window()->rawMouseInputEnabled = !window()->rawMouseInputEnabled();

            SPDLOG_INFO("Camera: pos = ({}, {}, {}), yaw = {}, pitch = {}",
                        m_cameraController.pos().x,
                        m_cameraController.pos().y,
                        m_cameraController.pos().z,
                        m_cameraController.yaw(),
                        m_cameraController.pitch());

            keyPressEvent->setAccepted(true);
        }
    }

    if (m_cameraController.enabled())
        m_cameraController.event(target, ev);
}

} // namespace TinyGltfHelper
