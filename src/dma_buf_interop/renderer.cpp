/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "renderer.h"

#include <KDGpuExample/kdgpuexample.h>

#include <KDGpu/buffer_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>

#include <glm/gtx/transform.hpp>

#include <example_utility.h>

#include <cstring>

KDGpuRenderer::KDGpuRenderer(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height)
    , m_api(std::make_unique<VulkanGraphicsApi>())
{
    setup();
}

KDGpuRenderer::~KDGpuRenderer()
{
    teardown();
}

void KDGpuRenderer::initializeScene()
{
    struct Vertex {
        glm::vec2 pos;
        glm::vec3 color;
    };

    {
        constexpr std::array<Vertex, 3> vertices = {
            Vertex{ { 0.0, -0.5 }, { 1.0, 0.0, 0.0 } },
            Vertex{ { -0.5, 0.5 }, { 0.0, 0.0, 1.0 } },
            Vertex{ { 0.5, 0.5 }, { 0.0, 1.0, 0.0 } },
        };

        const auto vertexBytes = std::as_bytes(std::span{ vertices });
        const BufferOptions bufferOptions = {
            .size = vertexBytes.size(),
            .usage = BufferUsageFlagBits::VertexBufferBit | BufferUsageFlagBits::TransferDstBit,
            .memoryUsage = MemoryUsage::GpuOnly
        };
        m_vertexBuffer = m_device.createBuffer(bufferOptions);
        const BufferUploadOptions uploadOptions = {
            .destinationBuffer = m_vertexBuffer,
            .dstStages = PipelineStageFlagBit::VertexAttributeInputBit,
            .dstMask = AccessFlagBit::VertexAttributeReadBit,
            .data = vertexBytes.data(),
            .byteSize = vertexBytes.size()
        };
        m_stagingBuffers.emplace_back(m_queue.uploadBufferData(uploadOptions));
    }

    {
        const BufferOptions bufferOptions = {
            .size = sizeof(glm::mat4),
            .usage = BufferUsageFlagBits::UniformBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu
        };
        m_transformBuffer = m_device.createBuffer(bufferOptions);
        m_transform = glm::mat4(1.0f);
        auto bufferData = m_transformBuffer.map();
        std::memcpy(bufferData, &m_transform, sizeof(glm::mat4));
        m_transformBuffer.unmap();
    }

    const auto vertexShaderPath = ExampleUtility::assetPath() + "/shaders/dma_buf_interop/simple.vert.spv";
    auto vertexShader = m_device.createShaderModule(KDGpuExample::readShaderFile(vertexShaderPath));

    const auto fragmentShaderPath = ExampleUtility::assetPath() + "/shaders/dma_buf_interop/simple.frag.spv";
    auto fragmentShader = m_device.createShaderModule(KDGpuExample::readShaderFile(fragmentShaderPath));

    const BindGroupLayoutOptions bindGroupLayoutOptions = {
        .bindings = { { .binding = 0,
                        .resourceType = ResourceBindingType::UniformBuffer,
                        .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit) } }
    };

    const BindGroupLayout bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutOptions);

    const PipelineLayoutOptions pipelineLayoutOptions = {
        .bindGroupLayouts = { bindGroupLayout }
    };
    m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutOptions);

    // clang-format off
    const GraphicsPipelineOptions pipelineOptions = {
        .shaderStages = {
            { .shaderModule = vertexShader, .stage = ShaderStageFlagBits::VertexBit },
            { .shaderModule = fragmentShader, .stage = ShaderStageFlagBits::FragmentBit }
        },
        .layout = m_pipelineLayout,
        .vertex = {
            .buffers = {
                { .binding = 0, .stride = sizeof(Vertex) }
            },
            .attributes = {
                { .location = 0, .binding = 0, .format = Format::R32G32_SFLOAT, .offset = offsetof(Vertex, pos) },
                { .location = 1, .binding = 0, .format = Format::R32G32B32_SFLOAT, .offset = offsetof(Vertex, color) }
            }
        },
        .renderTargets = {{ .format = m_colorFormat }},
        .depthStencil = {
            .format = m_depthFormat,
            .depthWritesEnabled = true,
            .depthCompareOperation = CompareOperation::Less
        },
    };
    // clang-format on
    m_pipeline = m_device.createGraphicsPipeline(pipelineOptions);

    const BindGroupOptions bindGroupOptions = {
        .layout = bindGroupLayout,
        .resources = { { .binding = 0,
                         .resource = UniformBufferBinding{ .buffer = m_transformBuffer } } }
    };

    m_transformBindGroup = m_device.createBindGroup(bindGroupOptions);
}

void KDGpuRenderer::render()
{
    auto commandRecorder = m_device.createCommandRecorder();

    auto renderPass = commandRecorder.beginRenderPass(m_renderPassOptions);

    renderPass.setPipeline(m_pipeline);
    renderPass.setVertexBuffer(0, m_vertexBuffer);
    renderPass.setBindGroup(0, m_transformBindGroup);
    const DrawCommand drawCmd = { .vertexCount = 3 };
    renderPass.draw(drawCmd);
    renderPass.end();
    m_commandBuffer = commandRecorder.finish();

    const SubmitOptions submitOptions = {
        .commandBuffers = { m_commandBuffer }
    };
    m_queue.submit(submitOptions);
    m_queue.waitUntilIdle();

    releaseStagingBuffers();
}

void KDGpuRenderer::setup()
{
    m_instance = m_api->createInstance(InstanceOptions{
            .applicationName = "test",
            .applicationVersion = KDGPU_MAKE_API_VERSION(0, 1, 0, 0) });

    auto adapter = m_instance.selectAdapter(AdapterDeviceType::Default);
    const auto adapterProperties = adapter->properties();
    SPDLOG_INFO("Using adapter: {}", adapterProperties.deviceName);

    m_device = adapter->createDevice(DeviceOptions{ .requestedFeatures = adapter->features() });
    m_queue = m_device.queues()[0];

    createRenderTargets();
}

void KDGpuRenderer::updateScene()
{
    static std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<float, std::milli>(now - lastTime).count();
    lastTime = now;

    static float angle = 0.0f;
    angle += elapsed * 0.1f;

    m_transform = glm::mat4(1.0f);
    m_transform = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));

    auto bufferData = m_transformBuffer.map();
    std::memcpy(bufferData, &m_transform, sizeof(glm::mat4));
    m_transformBuffer.unmap();
}

void KDGpuRenderer::teardown()
{
    m_colorTextureView = {};
    m_colorTexture = {};
    m_depthTextureView = {};
    m_depthTexture = {};
    m_pipeline = {};
    m_pipelineLayout = {};
    m_vertexBuffer = {};
    m_commandBuffer = {};
    m_stagingBuffers.clear();
}

void KDGpuRenderer::createRenderTargets()
{
    // Create a color texture to use as our render target
    TextureOptions colorTextureOptions = {
        .type = TextureType::TextureType2D,
        .format = m_colorFormat,
        .extent = { m_width, m_height, 1 },
        .mipLevels = 1,
        .samples = SampleCountFlagBits::Samples1Bit,
        .usage = TextureUsageFlagBits::ColorAttachmentBit,
        .memoryUsage = MemoryUsage::GpuOnly,
    };

    // Export the texture using DMA-BUF

    const auto *adapter = m_device.adapter();
    const auto modifierProps = adapter->drmFormatModifierProperties(m_colorFormat);
    assert(!modifierProps.empty());

    std::vector<uint64_t> drmFormatModifiers;
    std::ranges::transform(modifierProps, std::back_inserter(drmFormatModifiers), [](const DrmFormatModifierProperties &props) {
        return props.formatModifier;
    });

    colorTextureOptions.tiling = TextureTiling::DrmFormatModifier;
    colorTextureOptions.externalMemoryHandleType = ExternalMemoryHandleTypeFlagBits::DmaBuf;
    colorTextureOptions.drmFormatModifiers = drmFormatModifiers;

    m_colorTexture = m_device.createTexture(colorTextureOptions);
    m_colorTextureView = m_colorTexture.createView();

    m_renderTargetMemoryHandle = m_colorTexture.externalMemoryHandle();
    m_renderTargetDrmFormatModifier = m_colorTexture.drmFormatModifier();

    // Query adapter for DRM format modifier properties to figure out number of memory planes
    const auto planeCount = [this]() -> uint32_t {
        const auto *adapter = m_device.adapter();
        const auto modifierProps = adapter->drmFormatModifierProperties(m_colorFormat);
        auto it = std::ranges::find_if(modifierProps, [this](const DrmFormatModifierProperties &props) {
            return props.formatModifier == m_colorTexture.drmFormatModifier();
        });
        if (it == modifierProps.end()) {
            SPDLOG_CRITICAL("Failed to find format properties for DRM format modifier {:x}?", m_renderTargetDrmFormatModifier);
            return 1;
        }
        return it->planeCount;
    }();
    SPDLOG_INFO("drmFormat = {:x}, planeCount = {}", m_renderTargetDrmFormatModifier, planeCount);

    // Get layout for each memory plane
    const auto memoryPlaneMasks = std::array{
        TextureAspectFlagBits::MemoryPlane0Bit,
        TextureAspectFlagBits::MemoryPlane1Bit,
        TextureAspectFlagBits::MemoryPlane2Bit,
        TextureAspectFlagBits::MemoryPlane3Bit
    };
    assert(planeCount <= memoryPlaneMasks.size());
    for (uint32_t plane = 0; plane < planeCount; ++plane) {
        const auto layout = m_colorTexture.getSubresourceLayout(TextureSubresource{
                .aspectMask = memoryPlaneMasks[plane] });
        SPDLOG_INFO("Plane {} layout: offset = {}, rowPitch = {}, size = {}", plane, layout.offset, layout.rowPitch, layout.size);
        m_renderTargetMemoryPlaneLayouts.push_back(layout);
    }

    // Create a depth texture to use for depth-correct rendering
    const TextureOptions depthTextureOptions = {
        .type = TextureType::TextureType2D,
        .format = m_depthFormat,
        .extent = { m_width, m_height, 1 },
        .mipLevels = 1,
        .usage = TextureUsageFlagBits::DepthStencilAttachmentBit,
        .memoryUsage = MemoryUsage::GpuOnly
    };
    m_depthTexture = m_device.createTexture(depthTextureOptions);
    m_depthTextureView = m_depthTexture.createView();

    m_renderPassOptions = {
        .colorAttachments = {
                { .view = m_colorTextureView,
                  .clearValue = { 0.3f, 0.3f, 0.3f, 1.0f } } },
        .depthStencilAttachment = {
                .view = m_depthTextureView,
        },
    };
}

void KDGpuRenderer::releaseStagingBuffers()
{
    // Loop over any staging buffers and see if the corresponding fence has been signalled.
    // If so, we can dispose of them
    const auto removedCount = std::erase_if(m_stagingBuffers, [](const UploadStagingBuffer &stagingBuffer) {
        return stagingBuffer.fence.status() == FenceStatus::Signalled;
    });
    if (removedCount)
        SPDLOG_INFO("Removed {} staging buffers", removedCount);
}
