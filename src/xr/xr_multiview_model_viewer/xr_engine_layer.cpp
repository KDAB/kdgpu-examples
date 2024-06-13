/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "xr_engine_layer.h"
#include "model_scene.h"

#include <KDGpuExample/engine.h>
#include <KDGpuExample/xr_compositor/xr_cylinder_imgui_layer.h>
#include <KDGpuExample/xr_compositor/xr_quad_imgui_layer.h>

#include <KDXr/session.h>

void XrEngineLayer::onAttached()
{
    XrExampleEngineLayer::onAttached();
    if (!m_isInitialized)
        return;

    // Create a projection layer to render the 3D scene
    const XrProjectionLayerOptions projectionLayerOptions = {
        .device = &m_device,
        .queue = &m_queue,
        .session = &m_session,
        .colorSwapchainFormat = m_colorSwapchainFormat,
        .depthSwapchainFormat = m_depthSwapchainFormat,
        .samples = m_samples.get()
    };
    m_projectionLayer = createCompositorLayer<ModelScene>(projectionLayerOptions);
    m_projectionLayer->setReferenceSpace(m_referenceSpace);

    // Create an action set and actions
    m_actionSet = m_xrInstance.createActionSet({ .name = "default", .localizedName = "Default" });
    m_scaleAction = m_actionSet.createAction({ .name = "scale",
                                               .localizedName = "Scale",
                                               .type = KDXr::ActionType::Vector2Input,
                                               .subactionPaths = { m_handPaths[1] } });
    m_translateAction = m_actionSet.createAction({ .name = "translate",
                                                   .localizedName = "Translate",
                                                   .type = KDXr::ActionType::Vector2Input,
                                                   .subactionPaths = { m_handPaths[0] } });
    m_toggleTranslateModeAction = m_actionSet.createAction({ .name = "toggle_translate_mode_action",
                                                             .localizedName = "toggleTranslateModeAction",
                                                             .type = KDXr::ActionType::BooleanInput,
                                                             .subactionPaths = { m_handPaths[0] } });
    m_palmPoseAction = m_actionSet.createAction({ .name = "palm_pose",
                                                  .localizedName = "Palm Pose",
                                                  .type = KDXr::ActionType::PoseInput,
                                                  .subactionPaths = m_handPaths });
    m_toggleRightRayAction = m_actionSet.createAction({ .name = "toggle_right_ray",
                                                        .localizedName = "Toggle Right Ray",
                                                        .type = KDXr::ActionType::FloatInput,
                                                        .subactionPaths = { m_handPaths[1] } });
    m_toggleLeftRayAction = m_actionSet.createAction({ .name = "toggle_left_ray",
                                                       .localizedName = "Toggle Left Ray",
                                                       .type = KDXr::ActionType::FloatInput,
                                                       .subactionPaths = { m_handPaths[0] } });
    m_buzzAction = m_actionSet.createAction({ .name = "buzz",
                                              .localizedName = "Buzz",
                                              .type = KDXr::ActionType::VibrationOutput,
                                              .subactionPaths = m_handPaths });

    // Create action spaces for the palm poses. Default is no offset from the palm pose. If you wish to
    // apply an offset, you can do so by setting the poseInActionSpace member of the ActionSpaceOptions.
    for (uint32_t i = 0; i < 2; ++i)
        m_palmPoseActionSpaces[i] = m_session.createActionSpace({ .action = m_palmPoseAction, .subactionPath = m_handPaths[i] });

    // Suggest some bindings for the actions. NB: This assumes we are using a Meta Quest. If you are using a different
    // device, you will need to change the suggested bindings.
    const auto bindingOptions = KDXr::SuggestActionBindingsOptions{
        .interactionProfile = "/interaction_profiles/oculus/touch_controller",
        .suggestedBindings = {
                { .action = m_scaleAction, .binding = "/user/hand/right/input/thumbstick" },
                { .action = m_translateAction, .binding = "/user/hand/left/input/thumbstick" },
                { .action = m_buzzAction, .binding = "/user/hand/left/output/haptic" },
                { .action = m_buzzAction, .binding = "/user/hand/right/output/haptic" },
                { .action = m_palmPoseAction, .binding = "/user/hand/left/input/aim/pose" },
                { .action = m_palmPoseAction, .binding = "/user/hand/right/input/aim/pose" },
                { .action = m_toggleRightRayAction, .binding = "/user/hand/right/input/squeeze/value" },
                { .action = m_toggleLeftRayAction, .binding = "/user/hand/left/input/squeeze/value" },
                { .action = m_toggleTranslateModeAction, .binding = "/user/hand/left/input/x/click" } }
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

void XrEngineLayer::onDetached()
{
    m_translateAction = {};
    m_toggleTranslateModeAction = {};
    m_palmPoseAction = {};
    m_buzzAction = {};
    m_toggleLeftRayAction = {};
    m_toggleRightRayAction = {};
    m_scaleAction = {};
    m_actionSet = {};

    clearCompositorLayers();
    XrExampleEngineLayer::onDetached();
}

void XrEngineLayer::onInteractionProfileChanged()
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

void XrEngineLayer::pollActions(KDXr::Time predictedDisplayTime)
{
    // Sync the action set
    const auto syncActionOptions = KDXr::SyncActionsOptions{ .actionSets = { { m_actionSet } } };
    const auto syncActionResult = m_session.syncActions(syncActionOptions);
    if (syncActionResult != KDXr::SyncActionsResult::Success) {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to sync action set.");
        return;
    }

    // Poll the actions and do something with the results
    processScaleAction();
    processToggleLeftRay();
    processToggleRightRay();
    processTranslateAction();
    processToggleTranslateAction();
    processPalmPoseAction(predictedDisplayTime);
    processHapticAction();
}

void XrEngineLayer::processScaleAction()
{
    // Query the translate action from the left thumbstick
    const float dt = engine()->deltaTimeSeconds();
    float delta{ 0.0f };
    const auto scaleResult = m_session.getVector2State(
            { .action = m_scaleAction, .subactionPath = m_handPaths[1] }, m_scaleActionState);
    if (scaleResult == KDXr::GetActionStateResult::Success) {
        if (m_scaleActionState.active) {
            delta = dt * m_scaleActionState.currentState.y;
            delta *= m_linearSpeed * m_projectionLayer->scale();
        }
        const float newScale = std::clamp(m_projectionLayer->scale() + delta, 0.005f, 5.0f);
        m_projectionLayer->scale = newScale;
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get translate action state.");
    }
}

void XrEngineLayer::processTranslateAction()
{
    // Query the translate action from the left thumbstick
    const float dt = engine()->deltaTimeSeconds();
    glm::vec3 delta{ 0.0f, 0.0f, 0.0f };
    const auto translateResult = m_session.getVector2State(
            { .action = m_translateAction, .subactionPath = m_handPaths[0] }, m_translateActionState);
    if (translateResult == KDXr::GetActionStateResult::Success) {
        if (m_translateActionState.active) {
            switch (m_translationMode) {
            case TranslationMode::Vertical:
                delta = dt * glm::vec3(m_translateActionState.currentState.x, m_translateActionState.currentState.y, 0.0f);
                break;

            case TranslationMode::Horizontal:
                delta = dt * glm::vec3(m_translateActionState.currentState.x, 0.0f, -m_translateActionState.currentState.y);
                break;
            }
            delta *= m_linearSpeed;
        }
        m_projectionLayer->translation = m_projectionLayer->translation() + delta;
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get translate action state.");
    }
}
void XrEngineLayer::processToggleTranslateAction()
{
    bool toggleTranslationMode{ false };
    // Query the toggle animation action
    const auto toggleTranslationModeResult = m_session.getBooleanState(
            { .action = m_toggleTranslateModeAction, .subactionPath = m_handPaths[0] },
            m_toggleTranaslateModeActionState);
    if (toggleTranslationModeResult == KDXr::GetActionStateResult::Success) {
        if (m_toggleTranaslateModeActionState.currentState &&
            m_toggleTranaslateModeActionState.changedSinceLastSync &&
            m_toggleTranaslateModeActionState.active) {
            toggleTranslationMode = true;
            m_buzzHand = 0;
        }
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get toggle translate mode action state.");
    }

    // If the toggle animation action was pressed, toggle the animation and buzz the controller
    if (toggleTranslationMode) {
        if (m_translationMode == TranslationMode::Horizontal) {
            m_translationMode = TranslationMode::Vertical;
        } else {
            m_translationMode = TranslationMode::Horizontal;
        }

        m_buzzAmplitudes[m_buzzHand] = 1.0f;
        SPDLOG_LOGGER_INFO(m_logger, "Toggled translate mode = {}", static_cast<uint8_t>(m_translationMode));
    }
}
void XrEngineLayer::processToggleRightRay()
{
    // Query the toggle ray action from the right squeeze value
    float squeeze = 0.0f;
    static bool armed = true;
    const auto squeezeResult = m_session.getFloatState({ .action = m_toggleRightRayAction, .subactionPath = m_handPaths[1] }, m_toggleRightRayActionState);
    if (squeezeResult == KDXr::GetActionStateResult::Success) {
        if (m_scaleActionState.active) {
            squeeze = m_toggleRightRayActionState.currentState;
            if (armed && squeeze > 0.9f) {
                m_projectionLayer->toggleRay(ModelScene::Hand::Right);
                armed = false;
            }

            if (!armed && squeeze == 0.0f)
                armed = true;
        }
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get scale action state.");
    }
}

void XrEngineLayer::processToggleLeftRay()
{
    // Query the toggle ray action from the left squeeze value
    float squeeze = 0.0f;
    static bool armed = true;
    const auto squeezeResult = m_session.getFloatState({ .action = m_toggleLeftRayAction, .subactionPath = m_handPaths[0] }, m_toggleLeftRayActionState);
    if (squeezeResult == KDXr::GetActionStateResult::Success) {
        if (m_scaleActionState.active) {
            squeeze = m_toggleLeftRayActionState.currentState;
            if (armed && squeeze > 0.9f) {
                m_projectionLayer->toggleRay(ModelScene::Hand::Left);
                armed = false;
            }

            if (!armed && squeeze == 0.0f)
                armed = true;
        }
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get scale action state.");
    }
}

void XrEngineLayer::processHapticAction()
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

void XrEngineLayer::processPalmPoseAction(KDXr::Time predictedDisplayTime)
{
    for (uint32_t i = 0; i < 2; ++i) {
        // Query the palm pose action
        const auto palmPoseResult = m_session.getPoseState(
                { .action = m_palmPoseAction, .subactionPath = m_handPaths[i] }, m_palmPoseActionStates[i]);
        if (palmPoseResult == KDXr::GetActionStateResult::Success) {
            if (m_palmPoseActionStates[i].active) {
                // Update the action space for the palm pose
                const auto locateSpaceResult = m_palmPoseActionSpaces[i].locateSpace(
                        { .baseSpace = m_referenceSpace, .time = predictedDisplayTime }, m_palmPoseActionSpaceStates[i]);
                if (locateSpaceResult == KDXr::LocateSpaceResult::Success) {
                    // Update the pose of the projection layer
                    if (i == 0) {
                        m_projectionLayer->leftPalmPose = m_palmPoseActionSpaceStates[i].pose;
                    } else {
                        m_projectionLayer->rightPalmPose = m_palmPoseActionSpaceStates[i].pose;
                    }
                } else {
                    SPDLOG_LOGGER_ERROR(m_logger, "Failed to locate space for palm pose.");
                }
            }
        } else {
            SPDLOG_LOGGER_ERROR(m_logger, "Failed to get palm pose action state.");
        }
    }
}
