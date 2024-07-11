/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGpuExample/xr_example_engine_layer.h>

#include <KDXr/action.h>
#include <KDXr/action_set.h>

#include <KDGpu/bind_group.h>
#include <KDGpu/buffer.h>
#include <KDGpu/graphics_pipeline.h>
#include <KDGpu/render_pass_command_recorder_options.h>

#include <glm/glm.hpp>

#include <array>
#include <stdint.h>
#include <vector>

class ModelScene;

namespace KDGpuExample {
class XrQuadImGuiLayer;
class XrCylinderImGuiLayer;
} // namespace KDGpuExample

using namespace KDGpuExample;

class XrEngineLayer : public XrExampleEngineLayer
{
protected:
    void onAttached() override;
    void onDetached() override;
    void onInteractionProfileChanged() override;
    void pollActions(KDXr::Time predictedDisplayTime) override;

private:
    void processToggleAnimationAction();
    void processScaleRotateAction();
    void processTranslateAction();
    void processToggleTranslateAction();
    void processToggleScaleRotateAction();
    void processToggleRightRay();
    void processToggleLeftRay();
    void processPalmPoseAction(KDXr::Time predictedDisplayTime);
    void processHapticAction();

    ModelScene *m_projectionLayer{ nullptr };
    XrQuadImGuiLayer *m_quadImguiLayer{ nullptr };
    XrCylinderImGuiLayer *m_cylinderImguiLayer{ nullptr };

    enum class TranslationMode : std::uint8_t {
        Horizontal = 0,
        Vertical
    };
    TranslationMode m_translationMode{ TranslationMode::Horizontal };

    enum class ScaleRotateMode : std::uint8_t {
        Scale = 0,
        Rotation
    };
    ScaleRotateMode m_scaleMode{ ScaleRotateMode::Scale };

    // Input/output actions
    KDXr::ActionSet m_actionSet;
    KDXr::Action m_scaleRotateAction;
    KDXr::Action m_translateAction;
    KDXr::Action m_toggleTranslateModeAction;
    KDXr::Action m_toggleScaleRotateModeAction;
    KDXr::Action m_buzzAction;
    KDXr::Action m_palmPoseAction;
    KDXr::Action m_toggleRightRayAction;
    KDXr::Action m_toggleLeftRayAction;

    const std::vector<std::string> m_handPaths{ "/user/hand/left", "/user/hand/right" };

    float m_scaleSpeed{ 0.1f };
    KDXr::ActionStateVector2 m_scaleRotateActionState;
    float m_linearSpeed{ 1.0f };
    float m_angularSpeed{ 30.0f };
    KDXr::ActionStateVector2 m_translateActionState;
    KDXr::ActionStateBoolean m_toggleTranslateModeActionState;
    KDXr::ActionStateBoolean m_toggleScaleRotateModeActionState;
    int32_t m_buzzHand{ -1 };
    std::array<float, 2> m_buzzAmplitudes{ 0.0f, 0.0f };
    KDXr::ActionStateFloat m_toggleRightRayActionState;
    KDXr::ActionStateFloat m_toggleLeftRayActionState;

    std::array<KDXr::ActionStatePose, 2> m_palmPoseActionStates;
    std::array<KDXr::ReferenceSpace, 2> m_palmPoseActionSpaces;
    std::array<KDXr::SpaceState, 2> m_palmPoseActionSpaceStates;
};
