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
    m_toggleTranslateAction = m_actionSet.createAction({ .name = "toggletranslateaction",
                                                         .localizedName = "toggleTranslateAction",
                                                         .type = KDXr::ActionType::BooleanInput,
                                                         .subactionPaths = { m_handPaths[0] } });

    // Suggest some bindings for the actions. NB: This assumes we are using a Meta Quest. If you are using a different
    // device, you will need to change the suggested bindings.
    const auto bindingOptions = KDXr::SuggestActionBindingsOptions{
        .interactionProfile = "/interaction_profiles/oculus/touch_controller",
        .suggestedBindings = {
                { .action = m_scaleAction, .binding = "/user/hand/right/input/thumbstick" },
                { .action = m_translateAction, .binding = "/user/hand/left/input/thumbstick" },
                { .action = m_toggleTranslateAction, .binding = "/user/hand/left/input/x/click" } }
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
    m_toggleTranslateAction = {};
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
    processTranslateAction();
    processToggleTranslateAction();
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
            delta = dt * glm::vec3(m_translateActionState.currentState.x, m_translateActionState.currentState.y, 0.0f);
            delta *= m_linearSpeed;
        }
        m_projectionLayer->translation = m_projectionLayer->translation() + delta;
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get translate action state.");
    }
}
void XrEngineLayer::processToggleTranslateAction()
{
    // Query the translate action from the left thumbstick
    const float dt = engine()->deltaTimeSeconds();
    glm::vec3 delta{ 0.0f, 0.0f, 0.0f };
    const auto translateResult = m_session.getVector2State(
            { .action = m_translateAction, .subactionPath = m_handPaths[0] }, m_translateActionState);
    if (translateResult == KDXr::GetActionStateResult::Success) {
        if (m_translateActionState.active) {
            delta = dt * glm::vec3(m_translateActionState.currentState.x, m_translateActionState.currentState.y, 0.0f);
            delta *= m_linearSpeed;
        }
        m_projectionLayer->translation = m_projectionLayer->translation() + delta;
    } else {
        SPDLOG_LOGGER_ERROR(m_logger, "Failed to get translate action state.");
    }
}
