#pragma once

#include <KDGpu/graphics_pipeline_options.h>

#include <KDGpuExample/kdgpuexample.h>
#include <shader/shader_base.h>

#include <global_resources.h>

namespace kdgpu_ext::graphics::shader {
class ShaderVertexFragment : public ShaderBase
{
public:
    void setBaseDirectory(const std::string &base_directory)
    {
        m_baseDirectory = base_directory;
        if (m_baseDirectory.back() != '/')
            m_baseDirectory.push_back('/');
    }

    void loadVertexShader(const std::string &filename)
    {
        auto &device = GlobalResources::instance().graphicsDevice();
        auto shaderFile = KDGpuExample::assetDir().file(m_baseDirectory + filename);
        m_vertexShader = device.createShaderModule(KDGpuExample::readShaderFile(shaderFile));
    }

    void loadFragmentShader(const std::string &filename)
    {
        auto &device = GlobalResources::instance().graphicsDevice();
        auto shaderFile = KDGpuExample::assetDir().file(m_baseDirectory + filename);
        m_fragmentShader = device.createShaderModule(KDGpuExample::readShaderFile(shaderFile));
    }

    std::vector<ShaderStage> shaderStages() override
    {
        return {
            { .shaderModule = m_vertexShader,
              .stage = ShaderStageFlagBits::VertexBit },
            { .shaderModule = m_fragmentShader,
              .stage = ShaderStageFlagBits::FragmentBit }
        };
    }

    void deinitialize()
    {
        m_vertexShader = {};
        m_fragmentShader = {};
    }

private:
    std::string m_baseDirectory;
    ShaderModule m_vertexShader;
    ShaderModule m_fragmentShader;
};
} // namespace kdgpu_ext::graphics::shader
