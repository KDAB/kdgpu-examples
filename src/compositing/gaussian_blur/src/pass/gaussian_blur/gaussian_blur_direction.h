/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/
#pragma once

// this enum will be used to permute the shader - look for `constant_id` in the fragment program
enum class GaussianBlurDirection : int
{
    shaderGaussianBlurHorizontal = 0,
    shaderGaussianBlurVertical = 1,
};
