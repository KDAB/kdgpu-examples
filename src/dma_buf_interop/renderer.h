/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGpu/instance.h>

#include <glm/glm.hpp>

using namespace KDGpu;

class KDGpuRenderer
{
public:
    explicit KDGpuRenderer(uint32_t width, uint32_t height);
    ~KDGpuRenderer();

    void initializeScene();
    void updateScene();

    void render();

    uint32_t renderTargetWidth() const
    {
        return m_width;
    }

    uint32_t renderTargetHeight() const
    {
        return m_height;
    }

    MemoryHandle renderTargetMemoryHandle() const
    {
        return m_renderTargetMemoryHandle;
    }

    uint64_t renderTargetDrmFormatModifier() const
    {
        return m_renderTargetDrmFormatModifier;
    }

    std::span<const SubresourceLayout> renderTargetMemoryPlaneLayouts() const
    {
        return m_renderTargetMemoryPlaneLayouts;
    }

private:
    void setup();
    void teardown();
    void createRenderTargets();
    void releaseStagingBuffers();

    static constexpr Format m_colorFormat{ Format::R8G8B8A8_UNORM };
    static constexpr Format m_depthFormat{ Format::D24_UNORM_S8_UINT };

    uint32_t m_width{ 0 };
    uint32_t m_height{ 0 };

    std::unique_ptr<GraphicsApi> m_api;
    Instance m_instance;
    Device m_device;
    Queue m_queue;
    std::vector<UploadStagingBuffer> m_stagingBuffers;

    Texture m_colorTexture;
    TextureView m_colorTextureView;
    Texture m_depthTexture;
    TextureView m_depthTextureView;

    Buffer m_vertexBuffer;

    GraphicsPipeline m_pipeline;
    PipelineLayout m_pipelineLayout;
    RenderPassCommandRecorderOptions m_renderPassOptions;
    CommandBuffer m_commandBuffer;

    glm::mat4 m_transform;
    Buffer m_transformBuffer;
    BindGroup m_transformBindGroup;

    MemoryHandle m_renderTargetMemoryHandle{ 0 };
    uint64_t m_renderTargetDrmFormatModifier{ 0 };
    std::vector<SubresourceLayout> m_renderTargetMemoryPlaneLayouts;
};
