/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "xr_drawing.h"
#include "xr_drawing_scene.h"

#include <KDGpuExample/engine.h>
#include <KDGpuExample/xr_compositor/xr_cylinder_imgui_layer.h>
#include <KDGpuExample/xr_compositor/xr_quad_imgui_layer.h>
#include <KDGpuExample/xr_compositor/xr_passthrough_layer.h>
#include <KDGpuExample/imgui_item.h>

#include <KDGui/gui_events.h>
#include <KDGui/gui_application.h>
#include <KDUtils/dir.h>
#include <KDUtils/file.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <KDXr/session.h>

#include <imgui.h>

namespace {
constexpr float s_guiScale = 2.0f;
constexpr ImVec2 s_guiSize = { 600.0f, 640.0f };
constexpr ImVec2 s_guiSizeScaled = { s_guiSize.x * s_guiScale, s_guiSize.x *s_guiScale };
} // namespace

void XrDrawing::onAttached()
{
    XrExampleEngineLayer::onAttached();
    if (!m_isInitialized)
        return;

    const XrPassthroughLayerOptions passthroughLayerOptions = {
        .device = &m_device,
        .queue = &m_queue,
        .session = &m_session
    };
    m_passthroughLayer = createCompositorLayer<XrPassthroughLayer>(passthroughLayerOptions);

    // Create a projection layer to render the 3D scene
    const XrProjectionLayerOptions projectionLayerOptions = {
        .device = &m_device,
        .queue = &m_queue,
        .session = &m_session,
        .colorSwapchainFormat = m_colorSwapchainFormat,
        .depthSwapchainFormat = m_depthSwapchainFormat,
        .samples = m_samples.get(),
        .requestMultiview = false
    };
    m_drawingSceneLayer = createCompositorLayer<XrDrawingScene>(projectionLayerOptions);
    m_drawingSceneLayer->setReferenceSpace(m_referenceSpace);
    m_drawingSceneLayer->handScale = m_scale;

    // Create a quad layer to render the ImGui drawing options
    const XrQuadLayerOptions quadLayerOptions = {
        .device = &m_device,
        .queue = &m_queue,
        .session = &m_session,
        .colorSwapchainFormat = m_colorSwapchainFormat,
        .depthSwapchainFormat = m_depthSwapchainFormat,
        .samples = m_samples.get()
    };
    m_quadImguiLayer = createCompositorLayer<XrQuadImGuiLayer>(quadLayerOptions);
    m_quadImguiLayer->setReferenceSpace(m_referenceSpace);
    m_quadImguiLayer->position = { -1.0f, 0.2f, -1.5f };
    m_quadImguiLayer->resolution = { static_cast<uint32_t>(s_guiSizeScaled.x), static_cast<uint32_t>(s_guiSizeScaled.y) };
    m_quadImguiLayer->worldSize = KDGpu::Extent2Df{ 0.25f, 0.25f * m_quadImguiLayer->resolution().height / m_quadImguiLayer->resolution().width };
    m_quadImguiLayer->backgroundColor = KDGpu::ColorClearValue{ .float32 = {0.0f, 0.0f, 0.0f, 0.0f }};
    m_quadImguiLayer->clearImGuiOverlayDrawFunctions();
    m_quadImguiLayer->registerImGuiOverlayDrawFunction([this](ImGuiContext *ctx) {
        drawMenuUI(ctx);
    });
    m_quadImguiLayer->registerImGuiOverlayDrawFunction(XrQuadImGuiLayer::drawMouseCursor);

    // Use higher resolution text since we can hold it near our face
    m_quadImguiLayer->overlay().updateScale(s_guiScale);

    // Create an action set and actions
    m_actionSet = m_xrInstance.createActionSet({ .name = "default", .localizedName = "Default" });
    m_undoAddAction = m_actionSet.createAction({ .name = "undo",
                                                 .localizedName = "Undo Add",
                                                 .type = KDXr::ActionType::BooleanInput,
                                                 .subactionPaths = m_handPaths });
    m_resetScaleAction = m_actionSet.createAction({ .name = "reset_scale",
                                                    .localizedName = "Reset Scale",
                                                    .type = KDXr::ActionType::BooleanInput,
                                                    .subactionPaths = m_handPaths });
    m_addShapeAction = m_actionSet.createAction({ .name = "add_shape",
                                                  .localizedName = "Add Shape",
                                                  .type = KDXr::ActionType::BooleanInput,
                                                  .subactionPaths = { m_handPaths[1] } });
    m_editScaleAction = m_actionSet.createAction({ .name = "edit_scale",
                                                   .localizedName = "Edit Scale",
                                                   .type = KDXr::ActionType::Vector2Input,
                                                   .subactionPaths = { m_handPaths } });
    m_aimPoseAction = m_actionSet.createAction({ .name = "aim_pose",
                                                 .localizedName = "Aim Pose",
                                                 .type = KDXr::ActionType::PoseInput,
                                                 .subactionPaths = m_handPaths });
    m_buzzAction = m_actionSet.createAction({ .name = "buzz",
                                              .localizedName = "Buzz",
                                              .type = KDXr::ActionType::VibrationOutput,
                                              .subactionPaths = m_handPaths });
    m_togglePassthroughAction = m_actionSet.createAction({ .name = "passthrough",
                                                           .localizedName = "Toggle Passthrough",
                                                           .type = KDXr::ActionType::BooleanInput,
                                                           .subactionPaths = { m_handPaths[1] } });
    m_grabAction = m_actionSet.createAction({ .name = "grab_scene",
                                              .localizedName = "Grab Scene",
                                              .type = KDXr::ActionType::BooleanInput,
                                              .subactionPaths = { m_handPaths[1] } });

    // Create action spaces for the aim poses. Default is no offset from the aim pose. If you wish to
    // apply an offset, you can do so by setting the poseInActionSpace member of the ActionSpaceOptions.
    for (uint32_t i = 0; i < 2; ++i)
        m_aimPoseActionSpaces[i] = m_session.createActionSpace({ .action = m_aimPoseAction, .subactionPath = m_handPaths[i] });

    // Suggest some bindings for the actions. NB: This assumes we are using a Meta Quest. If you are using a different
    // device, you will need to change the suggested bindings.
    const auto bindingOptions = KDXr::SuggestActionBindingsOptions{
        .interactionProfile = "/interaction_profiles/oculus/touch_controller",
        .suggestedBindings = {
                { .action = m_undoAddAction, .binding = "/user/hand/right/input/b/click" },
                { .action = m_undoAddAction, .binding = "/user/hand/left/input/y/click" },
                { .action = m_resetScaleAction, .binding = "/user/hand/left/input/x/click" },
                { .action = m_resetScaleAction, .binding = "/user/hand/right/input/a/click" },
                { .action = m_addShapeAction, .binding = "/user/hand/right/input/trigger/value" },
                { .action = m_editScaleAction, .binding = "/user/hand/left/input/thumbstick" },
                { .action = m_editScaleAction, .binding = "/user/hand/right/input/thumbstick" },
                { .action = m_aimPoseAction, .binding = "/user/hand/left/input/grip/pose" },
                { .action = m_aimPoseAction, .binding = "/user/hand/right/input/aim/pose" },
                { .action = m_buzzAction, .binding = "/user/hand/left/output/haptic" },
                { .action = m_buzzAction, .binding = "/user/hand/right/output/haptic" },
                { .action = m_togglePassthroughAction, .binding = "/user/hand/right/input/thumbstick/click" },
                { .action = m_grabAction, .binding = "/user/hand/right/input/squeeze/value" },
        }
    };
    if (m_xrInstance.suggestActionBindings(bindingOptions) != KDXr::SuggestActionBindingsResult::Success) {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to suggest action bindings.");
    }

    // Attach the action set to the session
    const auto attachOptions = KDXr::AttachActionSetsOptions{ .actionSets = { m_actionSet } };
    if (m_session.attachActionSets(attachOptions) != KDXr::AttachActionSetsResult::Success) {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to attach action set.");
    }
}

void XrDrawing::onDetached()
{
    m_aimPoseActionSpaces[0] = {};
    m_aimPoseActionSpaces[1] = {};

    m_buzzAction = {};
    m_aimPoseAction = {};
    m_editScaleAction = {};
    m_addShapeAction = {};
    m_undoAddAction = {};
    m_resetScaleAction = {};
    m_togglePassthroughAction = {};
    m_grabAction = {};
    m_actionSet = {};

    clearCompositorLayers();
    m_quadImguiLayer = nullptr;
    m_drawingSceneLayer = nullptr;
    XrExampleEngineLayer::onDetached();
}

void XrDrawing::onInteractionProfileChanged()
{
    if (!m_session.isValid())
        return;
    SPDLOG_LOGGER_INFO(m_logger, "Interaction Profile Changed.");

    auto profileState = m_session.getInteractionProfile({ .topLevelUserPath = m_handPaths[0] });
    if (profileState.result == KDXr::GetInteractionProfileResult::Success) {
        SPDLOG_LOGGER_INFO(m_logger, "Interaction Profile for {}: {}", m_handPaths[0], profileState.interactionProfile);
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get interaction profile.");
    }

    profileState = m_session.getInteractionProfile({ .topLevelUserPath = m_handPaths[1] });
    if (profileState.result == KDXr::GetInteractionProfileResult::Success) {
        SPDLOG_LOGGER_INFO(m_logger, "Interaction Profile for {}: {}", m_handPaths[1], profileState.interactionProfile);
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get interaction profile.");
    }
}

void XrDrawing::pollActions(KDXr::Time predictedDisplayTime)
{
    // Sync the action set
    const auto syncActionOptions = KDXr::SyncActionsOptions{ .actionSets = { { m_actionSet } } };
    const auto syncActionResult = m_session.syncActions(syncActionOptions);
    if (syncActionResult != KDXr::SyncActionsResult::Success) {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to sync action set.");
        return;
    }

    if (!m_fileToLoad.empty()) {
        m_drawingSceneLayer->load(m_fileToLoad);
        m_fileToLoad.clear();
    }

    if (m_doSave) {
        const auto app = KDGui::GuiApplication::instance();
        auto dir = app->standardDir(KDFoundation::StandardDir::ApplicationDataLocal);
        dir.mkdir({ .createParentDirectories = true });
        auto fileToSave = dir.absoluteFilePath(m_fileToSave + ".json");
        m_drawingSceneLayer->save(fileToSave);
        m_doSave = false;
    }

    // Poll the actions and do something with the results
    processResetScaleAction();
    processToggleUndoAddAction();
    processEditScaleAction();
    processAimPoseAction(predictedDisplayTime);
    processHapticAction();
    processTogglePassthroughAction();

    processImguiInput();
    processGrabAction();
    processAddShapeAction();
}

void XrDrawing::processResetScaleAction()
{
    bool doScaleReset{ false };
    for (uint32_t i = 0; i < 2; ++i) {
        // Query the reset scale action
        const auto resetScaleResult = m_session.getBooleanState(
                { .action = m_resetScaleAction, .subactionPath = m_handPaths[i] },
                m_resetScaleActionStates[i]);
        if (resetScaleResult == KDXr::GetActionStateResult::Success) {
            if (m_resetScaleActionStates[i].currentState &&
                m_resetScaleActionStates[i].changedSinceLastSync &&
                m_resetScaleActionStates[i].active) {
                doScaleReset = true;
                m_buzzHand = i;
            }
        } else {
            SPDLOG_LOGGER_ERROR(m_logger, "Failed to get toggle animation action state.");
        }
    }

    // If the reset scale action was pressed, reset the scale to the default value
    if (doScaleReset) {
        m_scaleInput = defaultScaleInput;

        m_scale = { m_scaleInput.x * m_scaleInput.x * m_scaleInput.x,
                    m_scaleInput.y * m_scaleInput.y * m_scaleInput.y,
                    m_scaleInput.z * m_scaleInput.z * m_scaleInput.z };

        m_drawingSceneLayer->handScale = m_scale;

        m_pushDistance = 0.0f;
    }
}

void XrDrawing::processToggleUndoAddAction()
{
    bool undoAdd{ false };
    for (uint32_t i = 0; i < 2; ++i) {
        // Query the undo add action
        const auto toggleAnimationResult = m_session.getBooleanState(
                { .action = m_undoAddAction, .subactionPath = m_handPaths[i] },
                m_resetScaleActionStates[i]);
        if (toggleAnimationResult == KDXr::GetActionStateResult::Success) {
            if (m_resetScaleActionStates[i].currentState &&
                m_resetScaleActionStates[i].changedSinceLastSync &&
                m_resetScaleActionStates[i].active) {
                undoAdd = true;
                m_buzzHand = i;
                break;
            }
        } else {
            SPDLOG_LOGGER_ERROR(m_logger, "Failed to get toggle animation action state.");
        }
    }

    // If the undo add was pressed, remove and discard the last added shape
    if (undoAdd) {
        // Buzz on deletion
        m_buzzAmplitudes[m_buzzHand] = 1.0f;
        m_drawingSceneLayer->removeLastShape();
        SPDLOG_LOGGER_INFO(m_logger, "Removed a shape");
    }
}

void XrDrawing::processAddShapeAction()
{
    // Query the add shape action from the left trigger value
    const auto addShapeResult = m_session.getBooleanState({ .action = m_addShapeAction, .subactionPath = m_handPaths[1] }, m_addShapeActionState);
    if (addShapeResult == KDXr::GetActionStateResult::Success) {
        if (
                m_addShapeActionState.changedSinceLastSync &&
                m_addShapeActionState.active) {

            if (m_addShapeActionState.currentState && !m_pointingAtGui) {
                auto pose = m_aimPoseActionSpaceStates[1].pose;

                glm::vec3 posePosition = { pose.position.x, pose.position.y, pose.position.z };
                glm::quat poseOrientation = { pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z };
                glm::mat4 orientationMatrix = glm::toMat4(poseOrientation);
                glm::vec3 positionOffset = orientationMatrix * glm::vec4(0.0f, 0.0f, m_pushDistance, 1.0f);

                DrawnShape handShape = {
                    .color = m_drawingSceneLayer->shapeColor(),
                    .position = posePosition + positionOffset - m_drawingSceneLayer->modelOffset(),
                    .scale = m_scale,
                    .rotation = poseOrientation,
                    .shape = m_drawingSceneLayer->shape()
                };

                m_drawingSceneLayer->addShape(handShape);
                SPDLOG_LOGGER_INFO(m_logger, "Added a new shape");

                // Do a slightly more gentle buzz for creation than deletion
                m_buzzHand = 1;
                m_buzzAmplitudes[m_buzzHand] = 0.75f;
            } else if (m_pointingAtGui && m_addShapeActionState.currentState) {
                m_currentMouse = KDGui::MouseButton::LeftButton;

                KDGui::MousePressEvent ev{ 0, m_currentMouse, m_currentMouse, m_currentMouseX, m_currentMouseY };

                m_quadImguiLayer->overlay().event(nullptr, &ev);
            } else if (m_pointingAtGui && !m_addShapeActionState.currentState) {
                m_currentMouse = KDGui::MouseButton::NoButton;

                KDGui::MouseReleaseEvent ev{ 0, KDGui::MouseButton::LeftButton, m_currentMouse, m_currentMouseX, m_currentMouseY };

                m_quadImguiLayer->overlay().event(nullptr, &ev);
            }
        }
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get add shape action state.");
    }
}

void XrDrawing::processGrabAction()
{
    // Query the grab action from the left squeeze (lower trigger) value
    const auto grabResult = m_session.getBooleanState({ .action = m_grabAction, .subactionPath = m_handPaths[1] }, m_grabActionState);
    if (grabResult == KDXr::GetActionStateResult::Success) {
        const auto posePos = m_aimPoseActionSpaceStates[1].pose.position;
        const auto grabPos = glm::vec3{ posePos.x, posePos.y, posePos.z };

        if (m_grabActionState.changedSinceLastSync &&
            m_grabActionState.active) {
            m_grabPosition = grabPos;
        }

        if (m_grabActionState.active && m_grabActionState.currentState) {
            auto grabOffset = grabPos - m_grabPosition;
            m_drawingSceneLayer->modelOffset = m_drawingSceneLayer->modelOffset() + grabOffset;
            m_buzzAmplitudes[1] = 0.1f;
            m_grabPosition = grabPos;
        }

    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get add shape action state.");
    }
}

void XrDrawing::processEditScaleAction()
{
    // Query the scale and offset actions from both thumbsticks
    const float dt = engine()->deltaTimeSeconds();

    const auto translateResultLeft = m_session.getVector2State(
            { .action = m_editScaleAction, .subactionPath = m_handPaths[0] }, m_editScaleActionState[0]);
    const auto translateResultRight = m_session.getVector2State(
            { .action = m_editScaleAction, .subactionPath = m_handPaths[1] }, m_editScaleActionState[1]);
    if (translateResultLeft == KDXr::GetActionStateResult::Success && translateResultRight == KDXr::GetActionStateResult::Success) {

        static constexpr float deadzone = 0.2f;

        if (std::abs(m_editScaleActionState[0].currentState.x) > deadzone)
            m_scaleInput.x = std::clamp(m_scaleInput.x + m_editScaleActionState[0].currentState.x * dt, 0.01f, 1.0f);
        if (std::abs(m_editScaleActionState[0].currentState.y) > deadzone)
            m_scaleInput.y = std::clamp(m_scaleInput.y + m_editScaleActionState[0].currentState.y * dt, 0.01f, 1.0f);
        if (std::abs(m_editScaleActionState[1].currentState.x) > deadzone)
            m_scaleInput.z = std::clamp(m_scaleInput.z + m_editScaleActionState[1].currentState.x * dt, 0.01f, 1.0f);

        m_scale = { m_scaleInput.x * m_scaleInput.x * m_scaleInput.x,
                    m_scaleInput.y * m_scaleInput.y * m_scaleInput.y,
                    m_scaleInput.z * m_scaleInput.z * m_scaleInput.z };

        m_drawingSceneLayer->handScale = m_scale;

        if (std::abs(m_editScaleActionState[1].currentState.y) > deadzone) {
            auto delta = dt * m_editScaleActionState[1].currentState.y;
            delta *= m_linearSpeed;

            m_pushDistance = std::clamp(m_pushDistance - delta, -1.0f, 1.0f);
        }

    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get translate action state.");
    }
}

void XrDrawing::processImguiInput()
{
    auto intersetctionTest = m_quadImguiLayer->rayIntersection(m_aimPoseActionSpaceStates[1].pose);
    if (intersetctionTest.has_value()) {
        auto intersection = intersetctionTest.value();
        m_currentMouseX = intersection.x;
        m_currentMouseY = intersection.y;
        m_pointingAtGui = intersection.withinBounds;

        KDGui::MouseMoveEvent ev{ 0, m_currentMouse, m_currentMouseX, m_currentMouseY };

        m_quadImguiLayer->overlay().event(nullptr, &ev);
    }
}

void XrDrawing::processAimPoseAction(KDXr::Time predictedDisplayTime)
{
    for (uint32_t i = 0; i < 2; ++i) {
        // Query the aim pose action
        const auto aimPoseResult = m_session.getPoseState(
                { .action = m_aimPoseAction, .subactionPath = m_handPaths[i] }, m_aimPoseActionStates[i]);
        if (aimPoseResult == KDXr::GetActionStateResult::Success) {
            if (m_aimPoseActionStates[i].active) {
                // Update the action space for the aim pose
                const auto locateSpaceResult = m_aimPoseActionSpaces[i].locateSpace(
                        { .baseSpace = m_referenceSpace, .time = predictedDisplayTime }, m_aimPoseActionSpaceStates[i]);
                if (locateSpaceResult == KDXr::LocateSpaceResult::Success) {
                    // Update the pose of the projection layer
                    if (i == 0) {
                        glm::quat orientationOffset(glm::vec3(glm::radians(-135.0f), 0.0f, 0.0f));
                        glm::quat aimPoseOrientation(m_aimPoseActionSpaceStates[i].pose.orientation.w,
                                                     m_aimPoseActionSpaceStates[i].pose.orientation.x,
                                                     m_aimPoseActionSpaceStates[i].pose.orientation.y,
                                                     m_aimPoseActionSpaceStates[i].pose.orientation.z);
                        const auto layerOrientation = aimPoseOrientation * orientationOffset;
                        m_quadImguiLayer->orientation = { layerOrientation.x, layerOrientation.y, layerOrientation.z, layerOrientation.w };

                        glm::mat4 orientationMatrix = glm::toMat4(layerOrientation);
                        glm::vec3 positionOffset = orientationMatrix * glm::vec4(0.0f, 0.10f, 0.05f, 1.0f);
                        glm::vec3 aimPosePos = { m_aimPoseActionSpaceStates[i].pose.position.x,
                                                 m_aimPoseActionSpaceStates[i].pose.position.y,
                                                 m_aimPoseActionSpaceStates[i].pose.position.z };

                        const auto layerPosition = aimPosePos + positionOffset;
                        m_quadImguiLayer->position = { layerPosition.x, layerPosition.y, layerPosition.z };
                    } else {
                        auto pose = m_aimPoseActionSpaceStates[i].pose;
                        glm::vec3 posePosition = { pose.position.x, pose.position.y, pose.position.z };
                        glm::quat poseOrientation = { pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z };
                        glm::mat4 orientationMatrix = glm::toMat4(poseOrientation);
                        glm::vec3 positionOffset = orientationMatrix * glm::vec4(0.0f, 0.0f, m_pushDistance, 1.0f);

                        pose.position = { posePosition.x + positionOffset.x, posePosition.y + positionOffset.y, posePosition.z + positionOffset.z };

                        m_drawingSceneLayer->rightAimPose = pose;
                    }
                } else {
                    SPDLOG_LOGGER_ERROR(m_logger, "Failed to locate space for aim pose.");
                }
            }
        } else {
            SPDLOG_LOGGER_ERROR(m_logger, "Failed to get aim pose action state.");
        }
    }
}

void XrDrawing::processHapticAction()
{
    // Apply any haptic feedback
    for (uint32_t i = 0; i < 2; ++i) {
        if (m_buzzAmplitudes[i] > 0.0f) {
            const auto buzzOptions = KDXr::VibrationOutputOptions{
                .action = m_buzzAction,
                .subactionPath = m_handPaths[i],
                .amplitude = m_buzzAmplitudes[i],
            };
            m_session.vibrateOutput(buzzOptions);

            // Decay the amplitude
            m_buzzAmplitudes[i] *= 0.5f;
            if (m_buzzAmplitudes[i] < 0.01f)
                m_buzzAmplitudes[i] = 0.0f;
        }
    }
}

void XrDrawing::processTogglePassthroughAction()
{
    bool togglePassthrough{ false };
    // Query the toggle passthrough action
    const auto togglePassthroughResult = m_session.getBooleanState(
            { .action = m_togglePassthroughAction, .subactionPath = m_handPaths[1] },
            m_togglePassthroughActionState);
    if (togglePassthroughResult == KDXr::GetActionStateResult::Success) {
        if (m_togglePassthroughActionState.currentState &&
            m_togglePassthroughActionState.changedSinceLastSync &&
            m_togglePassthroughActionState.active) {
            togglePassthrough = true;
            m_buzzHand = 1;
        }
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get toggle passthrough action state.");
    }

    if (togglePassthrough) {
        m_passthroughEnabled = !m_passthroughEnabled;

        if (m_passthroughLayer)
            m_passthroughLayer->setRunning(m_passthroughEnabled);

        SPDLOG_LOGGER_INFO(m_logger, "Passthrough enabled = {}", m_passthroughEnabled);
    }
}

void XrDrawing::drawMenuUI(ImGuiContext *ctx)
{
    ImGui::SetCurrentContext(ctx);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(s_guiSizeScaled, ImGuiCond_FirstUseEver);

    const float buttonHeight = ImGui::GetFrameHeight() * 2.0f;
    const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float availableWidth = s_guiSizeScaled.x - ImGui::GetStyle().WindowPadding.x * 2.0f;
    const float availableHeight = s_guiSizeScaled.y - ImGui::GetStyle().WindowPadding.y * 2.0f;
    const float buttonWidthTwo = (availableWidth - itemSpacing) / 2.0f;
    const float buttonWidthThree = (availableWidth - 2 * itemSpacing) / 3.0f;

    switch (m_currentScreen) {
    case UIScreen::ShapeOptions: {
        ImGui::Begin(
                "Drawing UI",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        if (ImGui::Button("Sphere", ImVec2(buttonWidthThree, buttonHeight))) {
            m_drawingSceneLayer->shape = ShapeType::Sphere;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cube", ImVec2(buttonWidthThree, buttonHeight))) {
            m_drawingSceneLayer->shape = ShapeType::Cube;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cone", ImVec2(buttonWidthThree, buttonHeight))) {
            m_drawingSceneLayer->shape = ShapeType::Cone;
        }

        constexpr auto colorPickerFlags = ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs;
        auto shapeColor = glm::vec4{ m_drawingSceneLayer->shapeColor(), 1.0f };
        ImGui::SetNextItemWidth(availableWidth * 0.75);
        ImGui::Indent(availableWidth * 0.125);
        if (ImGui::ColorPicker3("##picker", glm::value_ptr(shapeColor), colorPickerFlags)) {
            m_drawingSceneLayer->shapeColor = shapeColor;
        }
        ImGui::Unindent(availableWidth * 0.125);

        if (ImGui::Button("Clear##ClearDrawing", ImVec2(buttonWidthThree, buttonHeight))) {
            m_drawingSceneLayer->clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load##LoadScreen", ImVec2(buttonWidthThree, buttonHeight))) {

            updateAvailableFiles();

            m_currentScreen = UIScreen::Load;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save##SaveScreen", ImVec2(buttonWidthThree, buttonHeight))) {

            updateAvailableFiles();
            m_fileToSave = "drawing_" + std::to_string(m_availableFiles.size());
            m_currentScreen = UIScreen::Save;
        }

        ImGui::End();
    } break;
    case UIScreen::Load: {
        ImGui::Begin(
                "Load Screen",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        const auto fileListHeight = availableHeight - ImGui::GetStyle().ItemSpacing.y - buttonHeight;
        ImGui::BeginChild("FileList", ImVec2(availableWidth, fileListHeight), true);
        for (int i = 0; i < m_availableFiles.size(); ++i) {
            std::filesystem::path p(m_availableFiles[i]);
            if (ImGui::Selectable(p.filename().string().c_str(), m_selectedFileIndex == i)) {
                m_selectedFileIndex = i;
            }
        }
        ImGui::EndChild();

        if (ImGui::Button("Load##LoadFile", ImVec2(buttonWidthTwo, buttonHeight))) {
            if (m_selectedFileIndex >= 0 && m_selectedFileIndex < m_availableFiles.size()) {
                m_fileToLoad = m_availableFiles[m_selectedFileIndex];
                m_currentScreen = UIScreen::ShapeOptions;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##CancelLoad", ImVec2(buttonWidthTwo, buttonHeight))) {
            m_currentScreen = UIScreen::ShapeOptions;
        }
        ImGui::End();
    } break;
    case UIScreen::Save: {
        ImGui::Begin(
                "Save Screen",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        // Render text box
        char fileNameBuf[255];
        std::strncpy(fileNameBuf, m_fileToSave.c_str(), 254);
        fileNameBuf[254] = '\0';
        ImGui::InputText("##TextBox", fileNameBuf, 255, ImGuiInputTextFlags_ReadOnly);

        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            m_fileToSave.clear();
        }

        // Render the virtual keyboard
        m_imguiKeyboard.renderKeyboard(m_fileToSave);

        // Haptic keypress feedback
        if (m_imguiKeyboard.pressedKey().has_value()) {
            m_buzzAmplitudes[1] = 0.5f;

            if (m_imguiKeyboard.pressedKey().value() == KDGui::Key::Key_Enter) {
                m_doSave = true;
                m_currentScreen = UIScreen::ShapeOptions;
            }
        }

        if (ImGui::Button("Save##SaveFile", ImVec2(buttonWidthTwo, buttonHeight))) {
            if (!m_fileToSave.empty()) {
                m_doSave = true;
                m_currentScreen = UIScreen::ShapeOptions;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##CancelSave", ImVec2(buttonWidthTwo, buttonHeight))) {
            m_currentScreen = UIScreen::ShapeOptions;
        }
        ImGui::End();
    } break;
    }
}

void XrDrawing::updateAvailableFiles()
{
    m_availableFiles.clear();

    const auto app = KDGui::GuiApplication::instance();
    auto dir = app->standardDir(KDFoundation::StandardDir::ApplicationDataLocal);
    dir.mkdir({ .createParentDirectories = true });

    for (const auto &entry : std::filesystem::directory_iterator(dir.path())) {
        if (entry.path().extension() == ".json") {
            m_availableFiles.push_back(entry.path().string());
        }
    }

    m_selectedFileIndex = -1;
}
