#pragma once

#include <KDGpu/buffer.h>

namespace kdgpu_ext::gltf_holder::render_mesh_set {
struct IndexedDraw {
    KDGpu::Handle<KDGpu::Buffer_t> indexBuffer;
    KDGpu::DeviceSize offset{ 0 };
    uint32_t indexCount{ 0 };
    KDGpu::IndexType indexType{ KDGpu::IndexType::Uint32 };
};
}
