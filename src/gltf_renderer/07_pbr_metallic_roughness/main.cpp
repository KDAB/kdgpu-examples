#include "pbr_metallic_roughness.h"

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
    auto exampleLayer = engine.createEngineLayer<PbrMetallicRoughness>();
    auto cameraLayer = engine.createEngineLayer<CameraControllerLayer>();
    cameraLayer->window = exampleLayer->window();
    auto &controller = cameraLayer->cameraController();
    controller.enabled = true;
    controller.pos = glm::vec3(0.0f, 0.7f, 2.0f);
    controller.yaw = -90.0f;
    controller.pitch = -10.0f;
    controller.moveSpeed = 1.0f;
    controller.camera = exampleLayer->camera();
    engine.running = true;
    return app.exec();
}
