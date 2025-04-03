#pragma once

#include <optional>

#include <KDGpu/graphics_pipeline.h>
#include <KDGpu/render_pass_command_recorder_options.h>

#include "buffer_and_offset.h"
#include "indexed_draw.h"

namespace kdgpu_ext::gltf_holder::render_mesh_set {
struct PrimitiveData {
    KDGpu::Handle<KDGpu::GraphicsPipeline_t> pipeline;
    std::vector<BufferAndOffset> vertexBuffers;
    std::optional<size_t> materialIndex;
                                 
    enum class DrawType {
        NonIndexed = 0,
        Indexed = 1
    };

    DrawType drawType{ DrawType::NonIndexed };

    union {
        uint32_t vertexCount;
        IndexedDraw indexedDraw;
    } drawData;
};
}
