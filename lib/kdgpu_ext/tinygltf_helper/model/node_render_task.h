#pragma once

#include <KDGpu/handle.h>

#include <uniform_buffer/node_transform.h>
#include <uniform/uniform_buffer_object_multi.h>

struct NodeRenderTask {
    uint32_t nodeIndex;
    uint32_t meshIndex;

    UniformBufferObjectCustomLayout<NodeTransform, KDGpu::ShaderStageFlagBits::VertexBit> transformUniformBufferObject;
};
