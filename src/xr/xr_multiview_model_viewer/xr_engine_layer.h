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
    void processScaleAction();
    void processTranslateAction();
    void processPalmPoseAction(KDXr::Time predictedDisplayTime);
    void processHapticAction();

    ModelScene *m_projectionLayer{ nullptr };
    XrQuadImGuiLayer *m_quadImguiLayer{ nullptr };
    XrCylinderImGuiLayer *m_cylinderImguiLayer{ nullptr };

    // Input/output actions
    KDXr::ActionSet m_actionSet;
    KDXr::Action m_scaleAction;
    KDXr::Action m_translateAction;

    const std::vector<std::string> m_handPaths{ "/user/hand/left", "/user/hand/right" };

    float m_scaleSpeed{ 0.1f };
    KDXr::ActionStateVector2 m_scaleActionState;
    float m_linearSpeed{ 1.0f };
    KDXr::ActionStateVector2 m_translateActionState;
};
