#pragma once

namespace ExampleUtility {

std::string assetPath()
{
#if defined(GLTF_RENDERER_ASSET_PATH)
  return GLTF_RENDERER_ASSET_PATH;
#else
  return "";
#endif
}

std::string gltfModelPath()
{
#if defined(GLTF_RENDERER_MODEL_PATH)
  return GLTF_RENDERER_MODEL_PATH;
#else
  return "";
#endif
}

}