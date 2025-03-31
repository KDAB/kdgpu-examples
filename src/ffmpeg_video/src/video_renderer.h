/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group
  company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <KDGpuKDGui/view.h>

#include <KDGpu/bind_group.h>
#include <KDGpu/buffer.h>
#include <KDGpu/command_buffer.h>
#include <KDGpu/device.h>
#include <KDGpu/fence.h>
#include <KDGpu/gpu_core.h>
#include <KDGpu/gpu_semaphore.h>
#include <KDGpu/graphics_pipeline.h>
#include <KDGpu/instance.h>
#include <KDGpu/queue.h>
#include <KDGpu/render_pass_command_recorder_options.h>
#include <KDGpu/surface.h>
#include <KDGpu/swapchain.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>
#include <KDGpu/sampler.h>
#include <KDGpu/ycbcr_conversion.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

// Prevent C++ symbol mangling of the FFMpeg C library
extern "C" {
struct AVFrame;
}

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

class VideoRenderer : public KDFoundation::Object
{
public:
    KDBindings::Property<bool> running{ true };
    KDBindings::Signal<VideoRenderer *> onUpdate;

    VideoRenderer();
    ~VideoRenderer();

    KDGpuKDGui::View *window() { return m_window.get(); }

    void setVideoInformation(std::pair<size_t, size_t> videoResolution,
                             KDGpu::Format videoFormat);
    void uploadVideoFrameData(AVFrame *frameData, const std::array<size_t, 4> &bufferSizes);

protected:
    void initialize();
    void update();
    void cleanup();

    void event(KDFoundation::EventReceiver *target,
               KDFoundation::Event *ev) override;

    void initializeScene();
    void cleanupScene();
    void updateScene();

    void render();
    void resize();
    void recreateSwapChain();

private:
    KDGpu::Buffer m_vertexBuffer;
    KDGpu::PushConstantRange m_aspectRatioPushConstant;
    KDGpu::BindGroupLayout m_bindGroupLayout;
    KDGpu::PipelineLayout m_pipelineLayout;
    KDGpu::GraphicsPipeline m_pipeline;
    KDGpu::BindGroup m_bindGroup;
    KDGpu::Texture m_videoTexture;
    KDGpu::TextureView m_videoTextureView;
    KDGpu::Sampler m_sampler;
    KDGpu::YCbCrConversion m_ycbcrConversion;
    std::pair<size_t, size_t> m_videoResolution;
    KDGpu::Format m_videoFormat{ KDGpu::Format::UNDEFINED };

    KDGpu::RenderPassCommandRecorderOptions m_opaquePassOptions;

    const KDGpu::Format m_mvColorFormat{ KDGpu::Format::R8G8B8A8_UNORM };
    const KDGpu::Format m_mvDepthFormat{ KDGpu::Format::D24_UNORM_S8_UINT };

    std::shared_ptr<spdlog::logger> m_logger;
    std::unique_ptr<KDGpu::GraphicsApi> m_api;
    std::unique_ptr<KDGpuKDGui::View> m_window;

    KDGpu::Extent2D m_swapchainExtent;
    KDGpu::Instance m_instance;
    KDGpu::Surface m_surface;
    KDGpu::Adapter *m_adapter;
    KDGpu::Device m_device;
    KDGpu::Queue m_queue;
    KDGpu::PresentMode m_presentMode;
    KDGpu::Swapchain m_swapchain;
    std::vector<KDGpu::TextureView> m_swapchainViews;
    KDGpu::Texture m_depthTexture;
    KDGpu::TextureView m_depthTextureView;

    uint32_t m_currentSwapchainImageIndex{ 0 };
    uint32_t m_inFlightIndex{ 0 };
    KDGpu::TextureUsageFlags m_depthTextureUsageFlags{};
    KDGpu::Format m_swapchainFormat{ KDGpu::Format::B8G8R8A8_UNORM };
    KDGpu::Format m_depthFormat;
    std::string m_capabilitiesString;
    bool m_swapchainDirty{ false };
    uint32_t m_frameCounter{ 0 };
    bool m_textureWasInitialized{ false };
    bool m_textureUploadPerformed{ false };

    std::array<KDGpu::Buffer, MAX_FRAMES_IN_FLIGHT> m_videoStagingBuffers;
    std::array<size_t, MAX_FRAMES_IN_FLIGHT> m_videoStagingBufferSizes;
    std::array<KDGpu::Fence, MAX_FRAMES_IN_FLIGHT> m_frameFences;
    std::array<std::vector<KDGpu::CommandBuffer>, MAX_FRAMES_IN_FLIGHT> m_commandBuffers;
    std::array<KDGpu::GpuSemaphore, MAX_FRAMES_IN_FLIGHT> m_uploadCompleteSemaphores;
    std::array<KDGpu::GpuSemaphore, MAX_FRAMES_IN_FLIGHT> m_presentCompleteSemaphores;
    std::array<KDGpu::GpuSemaphore, MAX_FRAMES_IN_FLIGHT> m_renderCompleteSemaphores;
};
