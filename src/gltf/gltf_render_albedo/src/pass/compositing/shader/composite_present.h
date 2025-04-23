/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

namespace pass::compositing::shader::composite_present {
// vertex shader
// - input
static constexpr size_t vertexInPositionLocation = 0;
static constexpr size_t vertexInTexCoordLocation = 1;

// fragment shader
// - binding
static constexpr size_t fragmentUniformColorTextureSet = 0;
static constexpr size_t fragmentUniformAlbedoPassTextureBinding = 0;
static constexpr size_t fragmentUniformOtherChannelPassTextureBinding = 1;

// - output
static constexpr size_t fragmentColorOutputLocation = 0;
}
