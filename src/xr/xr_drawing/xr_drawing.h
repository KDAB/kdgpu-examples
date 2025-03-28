/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include "drawing_model.h"
#include "imgui_keyboard.h"

#include <KDGpuExample/xr_example_engine_layer.h>

#include <KDXr/action.h>
#include <KDXr/action_set.h>

#include <KDGpu/bind_group.h>
#include <KDGpu/buffer.h>
#include <KDGpu/graphics_pipeline.h>
#include <KDGpu/render_pass_command_recorder_options.h>

#include <KDGui/gui_events.h>

#include <glm/glm.hpp>

#include <array>
#include <vector>

class XrDrawingScene;
struct ImGuiContext;

namespace KDGpuExample {
class XrQuadImGuiLayer;
class XrCylinderImGuiLayer;
class XrPassthroughLayer;
} // namespace KDGpuExample

using namespace KDGpuExample;

class XrDrawing : public XrExampleEngineLayer
{
protected:
    void onAttached() override;
    void onDetached() override;
    void onInteractionProfileChanged() override;
    void pollActions(KDXr::Time predictedDisplayTime) override;

private:
    void processResetScaleAction();
    void processToggleUndoAddAction();
    void processAddShapeAction();
    void processEditScaleAction();
    void processAimPoseAction(KDXr::Time predictedDisplayTime);
    void processHapticAction();
    void processTogglePassthroughAction();
    void processGrabAction();

    void processImguiInput();

    void drawMenuUI(ImGuiContext *ctx);
    void updateAvailableFiles();

    XrDrawingScene *m_drawingSceneLayer{ nullptr };
    XrQuadImGuiLayer *m_quadImguiLayer{ nullptr };
    XrPassthroughLayer *m_passthroughLayer{ nullptr };

    // Input/output actions
    KDXr::ActionSet m_actionSet;
    KDXr::Action m_undoAddAction;
    KDXr::Action m_resetScaleAction;
    KDXr::Action m_addShapeAction;
    KDXr::Action m_editScaleAction;
    KDXr::Action m_aimPoseAction;
    KDXr::Action m_buzzAction;
    KDXr::Action m_togglePassthroughAction;
    KDXr::Action m_grabAction;

    const std::vector<std::string> m_handPaths{ "/user/hand/left", "/user/hand/right" };

    KDXr::ActionStateBoolean m_addShapeActionState;
    std::array<KDXr::ActionStateBoolean, 2> m_resetScaleActionStates;
    std::array<KDXr::ActionStateBoolean, 2> m_undoAddActionStates;
    KDXr::ActionStateBoolean m_togglePassthroughActionState;
    KDXr::ActionStateBoolean m_grabActionState;

    float m_linearSpeed{ 0.25f };
    float m_pushDistance{ 0.0f };

    static constexpr glm::vec3 defaultScaleInput{ 0.5f, 0.5f, 0.5f };
    glm::vec3 m_scale{ 0.125f, 0.125f, 0.125f };
    glm::vec3 m_scaleInput{ defaultScaleInput };
    glm::vec3 m_grabPosition{ 0.5f, 0.5f, 0.5f };

    std::array<KDXr::ActionStateVector2, 2> m_editScaleActionState;
    std::array<KDXr::ActionStatePose, 2> m_aimPoseActionStates;
    std::array<KDXr::ReferenceSpace, 2> m_aimPoseActionSpaces;
    std::array<KDXr::SpaceState, 2> m_aimPoseActionSpaceStates;
    int32_t m_buzzHand{ -1 };
    std::array<float, 2> m_buzzAmplitudes{ 0.0f, 0.0f };
    bool m_passthroughEnabled = true;
    bool m_pointingAtGui = false;
    KDGui::MouseButton m_currentMouse = KDGui::MouseButton::NoButton;
    int16_t m_currentMouseX = 0;
    int16_t m_currentMouseY = 0;

    enum class UIScreen {
        ShapeOptions,
        Load,
        Save
    };

    ImGuiKeyboard m_imguiKeyboard;
    UIScreen m_currentScreen = UIScreen::ShapeOptions;
    std::string m_fileToLoad;
    std::string m_fileToSave;
    bool m_doSave = false;
    std::vector<std::string> m_availableFiles;
    int m_selectedFileIndex = -1;
};
