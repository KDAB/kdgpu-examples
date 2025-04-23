/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "engine_layer.h"

#include <KDGpuExample/engine.h>

#include <KDGui/gui_application.h>
#include <glm/gtc/quaternion.hpp>
#include <tinygltf_helper/camera_controller_layer.h>

using namespace KDGui;
using namespace KDGpu;
using namespace KDGpuExample;

int main()
{
    GuiApplication app;
    Engine engine;
    auto exampleLayer = engine.createEngineLayer<GltfRenderPbrEngineLayer>();
    auto cameraLayer = engine.createEngineLayer<TinyGltfHelper::CameraControllerLayer>();
    cameraLayer->window = exampleLayer->window();
    cameraLayer->cameraController().pos = glm::vec3(-2.228947, 0.7191802, 0.28127024);
    cameraLayer->cameraController().yaw = -0.49999297;
    cameraLayer->cameraController().pitch = -6.999943;
    cameraLayer->cameraController().camera = exampleLayer->camera();
    cameraLayer->cameraController().moveSpeed = 1.0f;
    engine.running = true;
    return app.exec();
}
