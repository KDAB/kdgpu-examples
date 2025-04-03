/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

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
