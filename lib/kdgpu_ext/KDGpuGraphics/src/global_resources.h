#pragma once

#include <KDGpu/device.h>

namespace kdgpu_ext::graphics {
class GlobalResources
{
public:
    void setGraphicsDevice(KDGpu::Device &graphicsDevice)
    {
        m_graphicsDevice = &graphicsDevice;
    }

    KDGpu::Device& graphicsDevice() const
    {
        assert(m_graphicsDevice != nullptr);
        return *m_graphicsDevice;
    }

    static GlobalResources &instance()
    {
        static GlobalResources inst;
        return inst;
    }

private:
    KDGpu::Device *m_graphicsDevice = nullptr;
};
} // namespace kdgpu_ext::graphics
