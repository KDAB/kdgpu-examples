#pragma once

#include <texture_target/texture_target.h>

/**
 * Render Target is a set of one or two target textures to render to.
 */
class RenderTarget
{
public:

    void createColorTarget();
    void setColorTarget(TextureTarget *target);
    bool hasColorTarget() const;
    TextureTarget* colorTarget() const;

    void createDepthTarget();
    void setDepthTarget(TextureTarget *target);
    bool hasDepthTarget() const;
    const TextureTarget* depthTarget() const;

    bool depthTestEnabled = true;
    bool depthWriteEnabled = true;
    CompareOperation depthCompareOperation = CompareOperation::Less;

private:
    TextureTarget* m_colorTarget = nullptr;
    TextureTarget* m_depthTarget = nullptr;
};
