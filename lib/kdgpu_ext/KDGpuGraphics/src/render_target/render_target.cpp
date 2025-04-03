#include "render_target.h"

void RenderTarget::createColorTarget()
{
    assert(!m_colorTarget);
    m_colorTarget = new TextureTarget();
}

void RenderTarget::setColorTarget(TextureTarget *target)
{
    m_colorTarget = target;
}

bool RenderTarget::hasColorTarget() const
{
    return m_colorTarget != nullptr;
}

TextureTarget *RenderTarget::colorTarget() const
{
    return m_colorTarget;
}

void RenderTarget::createDepthTarget()
{
    assert(!m_depthTarget);
    m_depthTarget = new TextureTarget();
}

void RenderTarget::setDepthTarget(TextureTarget *target)
{
    m_depthTarget = target;
}

bool RenderTarget::hasDepthTarget() const
{
    return m_depthTarget != nullptr;
}

const TextureTarget *RenderTarget::depthTarget() const
{
    return m_depthTarget;
}
