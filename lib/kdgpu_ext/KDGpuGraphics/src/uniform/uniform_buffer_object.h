#pragma once

#include <KDGpu/gpu_core.h>
#include <KDGpu/device.h>
#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/buffer_options.h>

#include <global_resources.h>

/**
 * @brief Models a Uniform Buffer Object for use in a shader, intended for single use
 */
template <class C, KDGpu::ShaderStageFlagBits shaderStateFlagBits>
class UniformBufferObject
{
public:
    C data;

    void init(uint32_t bindingId)
    {
        using namespace kdgpu_ext::graphics;
        m_bindingId = bindingId;

        // Create bind group layout consisting of a single binding holding a UBO
        m_bindGroupLayout = createBindGroupLayout(bindingId);

        m_buffer = GlobalResources::instance().graphicsDevice().createBuffer({
            .size = sizeof(C),
            .usage = KDGpu::BufferUsageFlags(KDGpu::BufferUsageFlagBits::UniformBufferBit),
            .memoryUsage = KDGpu::MemoryUsage::CpuToGpu // So we can map it to CPU address space
        });

        // clang-format off
        const KDGpu::BindGroupOptions bindGroupOptions = {
            .layout = m_bindGroupLayout,
            .resources = {{
                .binding = bindingId,
                .resource = KDGpu::UniformBufferBinding{ .buffer = m_buffer }
            }}
        };
        // clang-format on
        m_bindGroup = GlobalResources::instance().graphicsDevice().createBindGroup(bindGroupOptions);
    }

    void clear()
    {
        m_buffer = {};
        m_bindGroup = {};
        m_bindGroupLayout = {};
    }

    void upload()
    {
        std::memcpy(m_buffer.map(), &data, sizeof(C));
        m_buffer.unmap();
    }

    [[nodiscard]] const uint32_t& bindingId() const { return m_bindingId; }
    [[nodiscard]] const KDGpu::BindGroup& bindGroup() const { return m_bindGroup; }
    [[nodiscard]] const KDGpu::BindGroupLayout& bindGroupLayout() const { return m_bindGroupLayout; }

private:
    KDGpu::BindGroupLayoutOptions createBindGroupLayoutOptions(uint32_t bindingId) const
    {
        return KDGpu::BindGroupLayoutOptions {
            .bindings = {{
                .binding = bindingId,
                .resourceType = KDGpu::ResourceBindingType::UniformBuffer,
                .shaderStages = KDGpu::ShaderStageFlags(shaderStateFlagBits)
            }}
        };
    }

    KDGpu::BindGroupLayout createBindGroupLayout(uint32_t bindingId) const
    {
        using namespace kdgpu_ext::graphics;
        return GlobalResources::instance().graphicsDevice().createBindGroupLayout(createBindGroupLayoutOptions(bindingId));
    }

    uint32_t m_bindingId = 0;
    KDGpu::Buffer m_buffer;
    KDGpu::BindGroup m_bindGroup;
    KDGpu::BindGroupLayout m_bindGroupLayout;

};

