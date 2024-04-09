/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "buffer_layouts.h"

#include <tinygltf_helper/camera_controller_layer.h>

#include <KDGpuExample/engine.h>

#include <KDGui/gui_application.h>

using namespace KDGui;
using namespace KDGpu;
using namespace KDGpuExample;
using namespace TinyGltfHelper;

int main()
{
    GuiApplication app;
    Engine engine;
    auto exampleLayer = engine.createEngineLayer<BufferLayouts>();
    auto cameraLayer = engine.createEngineLayer<CameraControllerLayer>();
    cameraLayer->window = exampleLayer->window();
    cameraLayer->cameraController().camera = exampleLayer->camera();
    engine.running = true;
    return app.exec();
}
