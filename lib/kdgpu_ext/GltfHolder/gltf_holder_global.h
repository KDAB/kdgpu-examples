#pragma once

#include <global_resources.h>
#include <uniform/uniform_buffer_object_multi.h>
#include <uniform_buffer/node_transform.h>

namespace kdgpu_ext::gltf_holder {
class GltfHolderGlobal
{
public:
    void initialize()
    {
        using namespace kdgpu_ext::graphics;

        // the binding is set to 0 but can change per-pass anyway
        m_nodeTransformBindGroupLayout = UniformBufferObjectCustomLayout<NodeTransform, KDGpu::ShaderStageFlagBits::VertexBit>::createBindGroupLayout(0);
    }

    void deinitialize()
    {
        m_nodeTransformBindGroupLayout = {};
    }

    const KDGpu::BindGroupLayout &nodeTransformBindGroupLayout() const
    {
        return m_nodeTransformBindGroupLayout;
    }

    static GltfHolderGlobal &instance()
    {
        static GltfHolderGlobal instance;
        return instance;
    }

private:
    KDGpu::BindGroupLayout m_nodeTransformBindGroupLayout;
};
} // namespace kdgpu_ext::gltf_holder
