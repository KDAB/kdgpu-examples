#pragma once

#include <KDGpu/buffer.h>

namespace kdgpu_ext::gltf_holder::render_mesh_set {
struct BufferAndOffset {
    KDGpu::Handle<KDGpu::Buffer_t> buffer{};
    KDGpu::DeviceSize offset{ 0 };
};
}
