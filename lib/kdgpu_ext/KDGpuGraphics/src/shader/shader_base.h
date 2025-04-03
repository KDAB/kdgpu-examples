#pragma once

namespace kdgpu_ext::graphics::shader {
class ShaderBase
{
public:
    virtual ~ShaderBase() = default;
    virtual std::vector<ShaderStage> shaderStages() = 0;
};
}
