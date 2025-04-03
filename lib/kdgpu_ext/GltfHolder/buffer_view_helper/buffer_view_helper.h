#pragma once

#include <KDGpu/buffer.h>
#include <KDGpu/device.h>

#include <tiny_gltf.h>

KDGpu::Buffer createBufferForBufferView(
  const tinygltf::Model &model,
  KDGpu::BufferUsageFlags& bufferViewUsage,
  tinygltf::BufferView & gltfBufferView
);
