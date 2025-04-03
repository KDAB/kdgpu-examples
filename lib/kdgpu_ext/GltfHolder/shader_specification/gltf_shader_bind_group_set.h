#pragma once

#include <KDGpu/bind_group_layout.h>
#include <GltfHolder/shader_specification/gltf_shader_bind_group_type.h>

namespace kdgpu_ext::gltf_holder::shader_specification {
struct GltfShaderBindGroupSet
{
    // the first two here should be specified by the shader
    uint8_t shaderSet = 0;
    GltfShaderBindGroupType type = GltfShaderBindGroupType::PassOwnedUniformBuffer;

    // this can be added after all the sets have been defined, see set_bind_group_layout_for_bind_group
    const KDGpu::BindGroupLayout *bindGroupLayout = nullptr;
};

}
