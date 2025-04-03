#include "buffer_view_helper.h"

#include <global_resources.h>

#include <KDGpu/buffer_options.h>

KDGpu::Buffer createBufferForBufferView(
    const tinygltf::Model &model,
    KDGpu::BufferUsageFlags& bufferViewUsage,
    tinygltf::BufferView & gltfBufferView
)
{
    const auto bufferSize = static_cast<KDGpu::DeviceSize>(std::ceil(gltfBufferView.byteLength / 4.0) * 4);
    KDGpu::BufferOptions bufferOptions = {
        .size = bufferSize,
        .usage = bufferViewUsage,
        .memoryUsage = KDGpu::MemoryUsage::CpuToGpu // So we can map it to CPU address space
    };
    KDGpu::Buffer buffer = kdgpu_ext::graphics::GlobalResources::instance().graphicsDevice().createBuffer(bufferOptions);

    const tinygltf::Buffer &gltfBuffer = model.buffers.at(gltfBufferView.buffer);

    auto bufferData = static_cast<uint8_t *>(buffer.map());
    std::memcpy(bufferData, gltfBuffer.data.data() + gltfBufferView.byteOffset, gltfBufferView.byteLength);

    buffer.unmap();

    return buffer;
}
