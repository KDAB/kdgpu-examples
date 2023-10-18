#pragma once

#include <tinygltf_helper/camera_controller.h>
#include <tinygltf_helper/tinygltf_helper_export.h>

#include <KDGpuExample/engine_layer.h>

#include <KDGui/window.h>

#include <kdbindings/property.h>

namespace TinyGltfHelper {

class TINYGLTF_HELPER_EXPORT CameraControllerLayer : public KDGpuExample::EngineLayer
{
public:
    KDBindings::Property<KDGui::Window *> window{ nullptr };

    CameraController &cameraController() { return m_cameraController; }

protected:
    void onAttached() override;
    void onDetached() override;
    void update() override;

    void event(KDFoundation::EventReceiver *target, KDFoundation::Event *ev) override;

private:
    CameraController m_cameraController;
};

} // namespace TinyGltfHelper
