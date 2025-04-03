/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

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
    auto exampleLayer = engine.createEngineLayer<GltfRenderAlbedoEngineLayer>();
    auto cameraLayer = engine.createEngineLayer<TinyGltfHelper::CameraControllerLayer>();
    cameraLayer->window = exampleLayer->window();
    cameraLayer->cameraController().pos = glm::vec3(-0.202232, 0.8885231, 2.4);
    cameraLayer->cameraController().yaw = -85.89998;
    cameraLayer->cameraController().pitch = -10.699921;
    cameraLayer->cameraController().camera = exampleLayer->camera();
    engine.running = true;
    return app.exec();
}
