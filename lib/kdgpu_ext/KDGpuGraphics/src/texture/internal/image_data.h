#pragma once

namespace kdgpu_ext::graphics::texture::internal {
struct ImageData {
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    uint8_t *pixelData{ nullptr };
    KDGpu::DeviceSize byteSize{ 0 };
    KDGpu::Format format{ KDGpu::Format::R8G8B8A8_UNORM };
};

}
