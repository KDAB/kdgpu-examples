/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <cinttypes>

namespace pass::basic_geometry::shader::basic_geometry {
// VERTEX SHADER

// inputs
static constexpr uint32_t vertexInVertexPositionLocation = 0;
static constexpr uint32_t vertexInVertexTextureCoordinateLocation = 1;

// camera uniform buffer object
static constexpr size_t vertexUniformPassCameraSet = 1;
static constexpr size_t vertexUniformPassCameraBinding = 0;

// FRAGMENT SHADER

// other channel textures
static constexpr size_t fragmentUniformPassColorTextureSet = 0;
static constexpr size_t fragmentUniformPassColorTextureBinding = 0;

// output
static constexpr size_t fragmentOutPassColorLocation = 0;

} // namespace pass::basic_geometry::shader::basic_geometry
