/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

namespace ExampleUtility {

inline std::string assetPath()
{
#if defined(GLTF_RENDERER_ASSET_PATH)
  return GLTF_RENDERER_ASSET_PATH;
#else
  return "";
#endif
}

inline std::string gltfModelPath()
{
#if defined(GLTF_RENDERER_MODEL_PATH)
  return GLTF_RENDERER_MODEL_PATH;
#else
  return "";
#endif
}

}
