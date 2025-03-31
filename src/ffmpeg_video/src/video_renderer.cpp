/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group
  company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "video_renderer.h"

#include <KDFoundation/core_application.h>
#include <KDGui/gui_application.h>

#include <KDUtils/bytearray.h>
#include <KDUtils/dir.h>
#include <KDUtils/file.h>

#include <KDGpu/buffer_options.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/swapchain_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/sampler_options.h>

#include <fstream>
#include <string>

// Prevent C++ symbol mangling of the FFMpeg C library
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
}

using namespace KDGpu;

namespace {

auto readShaderFile =
        [](const std::string &filename) -> std::vector<uint32_t> {
    using namespace KDUtils;

    File file(File::exists(filename)
                      ? filename
                      : Dir::applicationDir().absoluteFilePath(filename));

    if (!file.open(std::ios::in | std::ios::binary)) {
        SPDLOG_LOGGER_CRITICAL(KDGpu::Logger::logger(), "Failed to open file {}",
                               filename);
        throw std::runtime_error("Failed to open file");
    }

    const ByteArray fileContent = file.readAll();
    std::vector<uint32_t> buffer(fileContent.size() / 4);
    std::memcpy(buffer.data(), fileContent.data(), fileContent.size());

    return buffer;
};

struct Vertex {
    std::array<float, 3> position;
    std::array<float, 2> texCoords;
};

struct Scaling {
    float xScaling;
    float yScaling;
};

Scaling computeScaling(float displayWidth, float displayHeight, float videoWidth, float videoHeight)
{
    float xScaling = 1.0f;
    float yScaling = 1.0f;
    const float videoAspectRatio = videoWidth / videoHeight;

    float resizeWidth = displayWidth;
    float resizeHeight = displayHeight;

    resizeWidth = displayHeight * videoAspectRatio;
    resizeHeight = resizeWidth / videoAspectRatio;

    if (resizeWidth > displayWidth) {
        resizeWidth = displayWidth;
        resizeHeight = resizeWidth / videoAspectRatio;
    }

    if (resizeHeight > displayHeight) {
        resizeHeight = displayHeight;
        resizeWidth = resizeHeight * videoAspectRatio;
    }

    yScaling = resizeHeight / displayHeight;
    xScaling = resizeWidth / displayWidth;

    return Scaling{
        .xScaling = xScaling,
        .yScaling = yScaling,
    };
}

} // namespace

VideoRenderer::VideoRenderer()
{
    initialize();
}

VideoRenderer::~VideoRenderer()
{
    running = false;

    cleanup();
}

void VideoRenderer::initialize()
{
    m_api = std::make_unique<VulkanGraphicsApi>();

    m_window = std::make_unique<KDGpuKDGui::View>();
    m_window->title = KDGui::GuiApplication::instance()->applicationName();

    // Request an instance of the api with whatever layers and extensions we wish
    // to request.

    m_instance = m_api->createInstance(InstanceOptions{
            .applicationName = KDGui::GuiApplication::instance()->applicationName(),
            .applicationVersion = KDGPU_MAKE_API_VERSION(0, 1, 0, 0),
    });

    // Create a drawable surface
    m_surface = m_window->createSurface(m_instance);

    // Create a device and a queue to use
    auto defaultDevice = m_instance.createDefaultDevice(m_surface);
    m_adapter = defaultDevice.adapter;
    m_device = std::move(defaultDevice.device);
    m_queue = m_device.queues()[0];

    if (!m_adapter->features().samplerYCbCrConversion) {
        SPDLOG_ERROR("Adapter has no support for YUV sampler conversions");
    }

    const AdapterSwapchainProperties swapchainProperties =
            m_device.adapter()->swapchainProperties(m_surface);

    // Choose a presentation mode from the ones supported
    constexpr std::array<PresentMode, 4> preferredPresentModes = {
        PresentMode::Mailbox, PresentMode::FifoRelaxed, PresentMode::Fifo,
        PresentMode::Immediate
    };
    const auto &availableModes = swapchainProperties.presentModes;
    for (const auto &presentMode : preferredPresentModes) {
        const auto it =
                std::find(availableModes.begin(), availableModes.end(), presentMode);
        if (it != availableModes.end()) {
            m_presentMode = presentMode;
            break;
        }
    }

    // Try to ensure that the chosen format is supported
    m_swapchainFormat = [this, &swapchainProperties]() {
        for (const auto &availableFormat : swapchainProperties.formats) {
            if (availableFormat.format == m_swapchainFormat &&
                availableFormat.colorSpace == ColorSpace::SRgbNonlinear) {
                return availableFormat.format;
            }
        }
        // Fallback to first listed available format
        return swapchainProperties.formats[0].format;
    }();

    // Choose a depth format from the ones supported
    constexpr std::array<Format, 5> preferredDepthFormat = {
        Format::D24_UNORM_S8_UINT,
        Format::D16_UNORM_S8_UINT,
        Format::D32_SFLOAT_S8_UINT,
        Format::D16_UNORM,
        Format::D32_SFLOAT,
    };
    for (const auto &depthFormat : preferredDepthFormat) {
        const FormatProperties formatProperties =
                defaultDevice.adapter->formatProperties(depthFormat);
        if (formatProperties.optimalTilingFeatures &
            FormatFeatureFlagBit::DepthStencilAttachmentBit) {
            m_depthFormat = depthFormat;
            break;
        }
    }

    // Create the present complete and render complete semaphores
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_presentCompleteSemaphores[i] = m_device.createGpuSemaphore(GpuSemaphoreOptions{ .label = std::format("PresentSem[{}]", i) });
        m_uploadCompleteSemaphores[i] = m_device.createGpuSemaphore(GpuSemaphoreOptions{ .label = std::format("UploadSem[{}]", i) });
        m_renderCompleteSemaphores[i] = m_device.createGpuSemaphore(GpuSemaphoreOptions{ .label = std::format("RenderSem[{}]", i) });
        m_frameFences[i] = m_device.createFence(FenceOptions{ .createSignalled = true });
        m_videoStagingBufferSizes[i] = 0;
    }

    recreateSwapChain();

    initializeScene();

    // Start Render Loop
    KDFoundation::CoreApplication::instance()->postEvent(
            this, std::make_unique<KDFoundation::UpdateEvent>());
}

void VideoRenderer::event(KDFoundation::EventReceiver *target,
                          KDFoundation::Event *ev)
{
    // Handle events we care about
    if (ev->type() == KDFoundation::Event::Type::Resize && m_device.isValid())
        m_swapchainDirty = true;

    if (target == this && ev->type() == KDFoundation::Event::Type::Update) {
        // Do stuff for this frame
        update();

        // Request the next frame
        if (running())
            KDFoundation::CoreApplication::instance()->postEvent(
                    this, std::make_unique<KDFoundation::UpdateEvent>());

        ev->setAccepted(true);
    }

    Object::event(target, ev);
}

void VideoRenderer::cleanup()
{
    m_device.waitUntilIdle();

    cleanupScene();

    m_commandBuffers = {};
    m_videoStagingBuffers = {};
    m_videoStagingBufferSizes = {};
    m_frameFences = {};
    m_presentCompleteSemaphores = {};
    m_renderCompleteSemaphores = {};
    m_uploadCompleteSemaphores = {};
    m_depthTextureView = {};
    m_depthTexture = {};
    m_swapchainViews.clear();
    m_swapchain = {};
    m_queue = {};
    m_device = {};
    m_surface = {};
    m_instance = {};
    m_window = {};
    m_api = {};
}

void VideoRenderer::initializeScene()
{
    // Create a buffer to hold triangle vertex data
    {
        m_vertexBuffer = m_device.createBuffer(BufferOptions{
                .size =
                        6 *
                        sizeof(Vertex), // 6 vertices * 2 attributes
                .usage = BufferUsageFlagBits::VertexBufferBit,
                .memoryUsage =
                        MemoryUsage::CpuToGpu // So we can map it to CPU address space
        });

        std::array<Vertex, 6> vertexData;
        vertexData[0] = {
            .position = { -1.0f, -1.0f, 0.0f },
            .texCoords = { 0.0f, 0.0f },
        }; // top-left, red
        vertexData[1] = {
            .position = { -1.0f, 1.0f, 0.0f },
            .texCoords = { 0.0f, 1.0f },
        }; // bottom-left, green
        vertexData[2] = {
            .position = { 1.0f, 1.0f, 0.0f },
            .texCoords = { 1.0f, 1.0f },
        }; // bottom-right, blue
        vertexData[3] = {
            .position = { 1.0f, 1.0f, 0.0f },
            .texCoords = { 1.0f, 1.0f },
        }; // bottom-right, blue
        vertexData[4] = {
            .position = { 1.0f, -1.0f, 0.0f },
            .texCoords = { 1.0f, 0.0f },
        }; // top-right, red
        vertexData[5] = {
            .position = { -1.0f, -1.0f, 0.0f },
            .texCoords = { 0.0f, 0.0f },
        }; // top-left, red

        auto bufferData = m_vertexBuffer.map();
        std::memcpy(bufferData, vertexData.data(),
                    vertexData.size() * sizeof(Vertex));
        m_vertexBuffer.unmap();
    }

    m_opaquePassOptions = {
        .colorAttachments = {
                { .view = {}, // Not setting the swapchain texture view just yet
                  .clearValue = { 0.3f, 0.3f, 0.3f, 1.0f },
                  .finalLayout = TextureLayout::PresentSrc },
        },
        .depthStencilAttachment = { .view = m_depthTextureView },
    };

    // Pipeline and Texture will only be created once we know the video information
}

void VideoRenderer::setVideoInformation(std::pair<size_t, size_t> videoResolution,
                                        Format videoFormat)
{
    m_videoResolution = videoResolution;
    m_videoFormat = videoFormat;

    // Allocate texture onto which we will upload frame data
    m_videoTexture = m_device.createTexture(TextureOptions{
            .type = TextureType::TextureType2D,
            .format = m_videoFormat,
            .extent = { .width = uint32_t(m_videoResolution.first), .height = uint32_t(m_videoResolution.second), .depth = 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit,
            .memoryUsage = MemoryUsage::GpuOnly,
            .initialLayout = TextureLayout::Undefined,
            .createFlags = TextureCreateFlagBits::MutableFormatBit,
    });

    assert(m_videoFormat == KDGpu::Format::G8_B8_R8_3PLANE_420_UNORM ||
           m_videoFormat == KDGpu::Format::G8_B8R8_2PLANE_420_UNORM);

    // Create  YUV -> RGB conversion for YUV based formats
    m_ycbcrConversion = m_device.createYCbCrConversion(YCbCrConversionOptions{
            .format = m_videoFormat,
            .model = SamplerYCbCrModelConversion::YCbCr709, // We want to convert from YCbCr Rec709 to RGB
            .components = ComponentMapping{
                    // Given G8_B8_R8_3PLANE_420, then G = Y, B = Cb, R = Cr
                    // We want to map [Y][Cb][Cr] -> [G][B][R]
                    .r = ComponentSwizzle::R, // Chroma Red -> R
                    .g = ComponentSwizzle::G, // Luma -> G
                    .b = ComponentSwizzle::B, // Chroma Blue -> B
            },
            .xChromaOffset = ChromaLocation::MidPoint,
            .yChromaOffset = ChromaLocation::MidPoint,
            .chromaFilter = FilterMode::Linear,
            .forceExplicitReconstruction = false,
    });

    // Create Samplers and TextureView using YCbCrConversion
    m_videoTextureView = m_videoTexture.createView(TextureViewOptions{
            .viewType = ViewType::ViewType2D,
            .format = m_videoFormat,
            .range = TextureSubresourceRange{
                    .aspectMask = TextureAspectFlagBits::ColorBit,
            },
            .yCbCrConversion = m_ycbcrConversion,
    });

    m_sampler = m_device.createSampler(SamplerOptions{
            .magFilter = FilterMode::Linear,
            .minFilter = FilterMode::Linear,
            .u = AddressMode::ClampToEdge,
            .v = AddressMode::ClampToEdge,
            .w = AddressMode::ClampToEdge,
            .yCbCrConversion = m_ycbcrConversion,
    });

    m_textureWasInitialized = false;

    // Create a vertex shader and fragment shader (spir-v only for now)
    const auto vertexShaderPath = "shaders/textured_quad.vert.spv";
    auto vertexShader =
            m_device.createShaderModule(readShaderFile(vertexShaderPath));

    const auto fragmentShaderPath = "shaders/textured_quad.frag.spv";
    auto fragmentShader =
            m_device.createShaderModule(readShaderFile(fragmentShaderPath));

    // Create bind group layout consisting of a single binding holding a TextureView and Sampler
    // The Sampler since it uses a YUV Conversion needs to be set as an immutable sampler
    m_bindGroupLayout = m_device.createBindGroupLayout(BindGroupLayoutOptions{
            .bindings = {
                    {
                            .binding = 0,
                            .resourceType = ResourceBindingType::CombinedImageSampler,
                            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit),
                            .immutableSamplers = { m_sampler },
                    },
            },
    });

    // Create a pipeline layout (array of bind group layouts)
    m_aspectRatioPushConstant = PushConstantRange{
        .offset = 0,
        .size = 2 * sizeof(float),
        .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit)
    };

    m_pipelineLayout = m_device.createPipelineLayout(PipelineLayoutOptions{
            .bindGroupLayouts = { m_bindGroupLayout },
            .pushConstantRanges = { m_aspectRatioPushConstant },
    });

    m_pipeline = m_device.createGraphicsPipeline(GraphicsPipelineOptions{
            .shaderStages = { { .shaderModule = vertexShader,
                                .stage = ShaderStageFlagBits::VertexBit },
                              { .shaderModule = fragmentShader,
                                .stage = ShaderStageFlagBits::FragmentBit } },
            .layout = m_pipelineLayout,
            .vertex = {
                    .buffers = {
                            { .binding = 0, .stride = sizeof(Vertex) },
                    },
                    .attributes = {
                            { .location = 0, .binding = 0, .format = Format::R32G32B32_SFLOAT }, // Position
                            { .location = 1, .binding = 0, .format = Format::R32G32_SFLOAT, .offset = 3 * sizeof(float) }, // texCoords
                    },
            },
            .renderTargets = {
                    { .format = m_swapchainFormat },
            },
            .depthStencil = {
                    .format = m_depthFormat,
                    .depthWritesEnabled = true,
                    .depthCompareOperation = CompareOperation::Less,
            },
    });

    m_bindGroup = m_device.createBindGroup(BindGroupOptions{
            .layout = m_bindGroupLayout,
            .resources = {
                    {
                            .binding = 0,
                            // No Sampler since it it an immutable sampler set on the bindGroupLayout
                            .resource = TextureViewSamplerBinding{ .textureView = m_videoTextureView },
                    },
            },
    });
}

void VideoRenderer::cleanupScene()
{
    m_pipeline = {};
    m_bindGroupLayout = {};
    m_pipelineLayout = {};
    m_vertexBuffer = {};
    m_bindGroup = {};
    m_videoTextureView = {};
    m_videoTexture = {};
    m_sampler = {};
    m_ycbcrConversion = {};
}

void VideoRenderer::update()
{
    // Increase Total Frame Counter
    ++m_frameCounter;

    // Compute In Flight Index
    m_inFlightIndex = m_frameCounter % MAX_FRAMES_IN_FLIGHT;

    // Wait for Fence to be signal (should be done by the queue submission)
    m_frameFences[m_inFlightIndex].wait();

    if (m_swapchainDirty) {
        // We need to recreate swapchain
        recreateSwapChain();
        // Handle any changes that would be needed when a swapchain resize occurs
        resize();
        m_swapchainDirty = false;
    }

    // Obtain swapchain image view
    const auto result = m_swapchain.getNextImageIndex(m_currentSwapchainImageIndex,
                                                      m_presentCompleteSemaphores[m_inFlightIndex]);

    if (result == AcquireImageResult::OutOfDate) {
        // We need to recreate swapchain
        recreateSwapChain();
        // Handle any changes that would be needed when a swapchain resize occurs
        resize();
        // Early return as we need to retry to retrieve the image index
        return;
    } else if (result != AcquireImageResult::Success) {
        // Something went wrong and we can't recover from it
        return;
    }

    // Reset Fence so that we can submit it again
    m_frameFences[m_inFlightIndex].reset();

    m_textureUploadPerformed = false;

    // Update Scene State, potentially perform Video Texture Upload
    updateScene();

    // Call render() function to record and submit drawing commands
    render();

    // Present the swapchain image
    // clang-format off
    PresentOptions presentOptions = {
        .waitSemaphores = { m_renderCompleteSemaphores[m_inFlightIndex] },
        .swapchainInfos = {{
            .swapchain = m_swapchain,
            .imageIndex = m_currentSwapchainImageIndex
        }}
    };
    // clang-format on
    m_queue.present(presentOptions);
}

void VideoRenderer::uploadVideoFrameData(AVFrame *frameData, const std::array<size_t, 4> &bufferSizes)
{
    // Early return if texture has yet to be created
    if (!m_videoTexture.isValid())
        return;

    const uint32_t hRes = m_videoResolution.first;
    const uint32_t vRes = m_videoResolution.second;

    CommandRecorder commandRecorder = m_device.createCommandRecorder(CommandRecorderOptions{ .queue = m_queue });

    // We first need to transition the texture into the TextureLayout::TransferDstOptimal layout
    const TextureMemoryBarrierOptions toTransferDstOptimal = {
        .srcStages = PipelineStageFlags(PipelineStageFlagBit::TopOfPipeBit),
        .dstStages = PipelineStageFlags(PipelineStageFlagBit::TransferBit),
        .dstMask = AccessFlags(AccessFlagBit::TransferWriteBit),
        .oldLayout = m_textureWasInitialized ? TextureLayout::ShaderReadOnlyOptimal : TextureLayout::Undefined,
        .newLayout = TextureLayout::TransferDstOptimal,
        .texture = m_videoTexture,
        .range = {
                .aspectMask = TextureAspectFlagBits::ColorBit,
        },
    };
    commandRecorder.textureMemoryBarrier(toTransferDstOptimal);

    // Ensure we have the data for the image planes (Y, Cb, Cr)
    DeviceSize dataSize = 0;
    size_t planeCount = 0;
    if (m_videoFormat == Format::G8_B8_R8_3PLANE_420_UNORM) {
        assert(frameData->data[0] != nullptr && frameData->data[1] != nullptr && frameData->data[2] != nullptr && frameData->data[3] == nullptr);
        dataSize = bufferSizes[0] + bufferSizes[1] + bufferSizes[2];
        planeCount = 3;
    } else if (m_videoFormat == Format::G8_B8R8_2PLANE_420_UNORM) {
        assert(frameData->data[0] != nullptr && frameData->data[1] != nullptr && frameData->data[2] == nullptr);
        dataSize = bufferSizes[0] + bufferSizes[1];
        planeCount = 2;
    }
    assert(dataSize > 0);
    assert(planeCount > 1);

    Buffer &frameStagingBuffer = m_videoStagingBuffers[m_inFlightIndex];
    size_t &bufferSize = m_videoStagingBufferSizes[m_inFlightIndex];
    if (!frameStagingBuffer.isValid() || bufferSize != dataSize) {
        bufferSize = dataSize;
        frameStagingBuffer = m_device.createBuffer(BufferOptions{
                .size = dataSize,
                .usage = BufferUsageFlagBits::TransferSrcBit,
                .memoryUsage = MemoryUsage::CpuOnly // Use a CPU heap for the staging buffer
        });
    }

    uint8_t *bufferData = static_cast<uint8_t *>(frameStagingBuffer.map());

    size_t offset = 0;
    for (size_t i = 0; i < planeCount; ++i) {
        std::memcpy(bufferData + offset, frameData->data[i], bufferSizes[i]);
        offset += bufferSizes[i];
    }

    frameStagingBuffer.unmap();

    // Now we perform the staging buffer to texture copy operation
    std::vector<BufferTextureCopyRegion> copyRegions = {
        BufferTextureCopyRegion{
                .bufferOffset = 0,
                .bufferRowLength = uint32_t(std::abs(frameData->linesize[0])),
                .textureSubResource = TextureSubresourceLayers{
                        .aspectMask = TextureAspectFlagBits::Plane0Bit, // Y Luma
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .textureOffset = { .x = 0, .y = 0, .z = 0 },
                .textureExtent = { .width = hRes, .height = vRes, .depth = 1 },
        }
    };

    if (planeCount == 2) { // G8_B8R8_2PLANE_420_UNORM case (Cb&Cr are interleaved on the second plane)
        copyRegions.emplace_back(BufferTextureCopyRegion{
                .bufferOffset = bufferSizes[0],
                .textureSubResource = TextureSubresourceLayers{
                        .aspectMask = TextureAspectFlagBits::Plane1Bit, // Cb & Cr interleaved
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .textureOffset = { .x = 0, .y = 0, .z = 0 },
                .textureExtent = { .width = hRes / 2, .height = vRes / 2, .depth = 1 },
        });
    } else if (planeCount > 2) { // G8_B8_R8_3PLANE_420_UNORM we have dedicated planes for Cb&Cr
        copyRegions.emplace_back(BufferTextureCopyRegion{
                .bufferOffset = bufferSizes[0],
                .bufferRowLength = uint32_t(std::abs(frameData->linesize[1])),
                .textureSubResource = TextureSubresourceLayers{
                        .aspectMask = TextureAspectFlagBits::Plane1Bit, // Cb
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .textureOffset = { .x = 0, .y = 0, .z = 0 },
                .textureExtent = { .width = hRes / 2, .height = vRes / 2, .depth = 1 },
        });
        copyRegions.emplace_back(BufferTextureCopyRegion{
                .bufferOffset = bufferSizes[0] + bufferSizes[1],
                .bufferRowLength = uint32_t(std::abs(frameData->linesize[2])),
                .textureSubResource = TextureSubresourceLayers{
                        .aspectMask = TextureAspectFlagBits::Plane2Bit, // Cr
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .textureOffset = { .x = 0, .y = 0, .z = 0 },
                .textureExtent = { .width = hRes / 2, .height = vRes / 2, .depth = 1 },
        });
    }

    commandRecorder.copyBufferToTexture(BufferToTextureCopy{
            .srcBuffer = frameStagingBuffer,
            .dstTexture = m_videoTexture,
            .dstTextureLayout = TextureLayout::TransferDstOptimal,
            .regions = std::move(copyRegions) });

    // Finally, we transition the texture to the specified final layout
    const TextureMemoryBarrierOptions toFinalLayout = {
        .srcStages = PipelineStageFlags(PipelineStageFlagBit::TransferBit),
        .srcMask = AccessFlags(AccessFlagBit::TransferWriteBit),
        .dstStages = PipelineStageFlagBit::AllGraphicsBit,
        .dstMask = AccessFlags(AccessFlagBit::MemoryReadBit),
        .oldLayout = TextureLayout::TransferDstOptimal,
        .newLayout = TextureLayout::ShaderReadOnlyOptimal,
        .texture = m_videoTexture,
        .range = {
                .aspectMask = TextureAspectFlagBits::ColorBit,
        },
    };
    commandRecorder.textureMemoryBarrier(toFinalLayout);

    m_commandBuffers[m_inFlightIndex].emplace_back(commandRecorder.finish());
    auto commandBufferHandle = m_commandBuffers[m_inFlightIndex].back().handle();

    m_queue.submit(SubmitOptions{
            .commandBuffers = { commandBufferHandle },
            .waitSemaphores = { m_presentCompleteSemaphores[m_inFlightIndex] },
            .signalSemaphores = { m_uploadCompleteSemaphores[m_inFlightIndex] },
    });

    m_textureUploadPerformed = true;
}

void VideoRenderer::recreateSwapChain()
{
    const AdapterSwapchainProperties swapchainProperties =
            m_device.adapter()->swapchainProperties(m_surface);

    // Create a swapchain of images that we will render to.
    SwapchainOptions swapchainOptions = {
        .surface = m_surface,
        .format = m_swapchainFormat,
        .minImageCount = getSuitableImageCount(swapchainProperties.capabilities),
        .imageExtent = { .width = m_window->width(), .height = m_window->height() },
        .imageLayers = 1,
        .presentMode = PresentMode::FifoRelaxed,
        .oldSwapchain = m_swapchain,
    };

    // Create swapchain and destroy previous one implicitly
    m_swapchain = m_device.createSwapchain(swapchainOptions);

    const auto &swapchainTextures = m_swapchain.textures();
    const auto swapchainTextureCount = swapchainTextures.size();

    m_swapchainViews.clear();
    m_swapchainViews.reserve(swapchainTextureCount);

    for (uint32_t i = 0; i < swapchainTextureCount; ++i) {
        auto view = swapchainTextures[i].createView({
                .viewType = ViewType::ViewType2D,
                .format = swapchainOptions.format,
                .range =
                        TextureSubresourceRange{
                                .aspectMask = TextureAspectFlagBits::ColorBit,
                                .baseArrayLayer = 0,
                                .layerCount = 1,
                        },
        });
        m_swapchainViews.push_back(std::move(view));
    }

    // Create a depth texture to use for depth-correct rendering
    TextureOptions depthTextureOptions = {
        .type = TextureType::TextureType2D,
        .format = m_depthFormat,
        .extent = { m_window->width(), m_window->height(), 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = SampleCountFlagBits::Samples1Bit,
        .usage = TextureUsageFlagBits::DepthStencilAttachmentBit |
                m_depthTextureUsageFlags,
        .memoryUsage = MemoryUsage::GpuOnly
    };
    m_depthTexture = m_device.createTexture(depthTextureOptions);
    m_depthTextureView = m_depthTexture.createView({
            .viewType = ViewType::ViewType2D,
            .range =
                    TextureSubresourceRange{
                            .aspectMask = TextureAspectFlagBits::DepthBit,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                    },
    });

    m_capabilitiesString = surfaceCapabilitiesToString(
            m_device.adapter()->swapchainProperties(m_surface).capabilities);
}

void VideoRenderer::updateScene()
{
    // Give a chance to update Texture Content at this point
    onUpdate.emit(this);
}

void VideoRenderer::resize()
{
    // Swapchain might have been resized and texture views recreated. Ensure we
    // update the PassOptions accordingly
    m_opaquePassOptions.depthStencilAttachment.view = m_depthTextureView;
}

void VideoRenderer::render()
{
    // Create a command encoder/recorder
    auto commandRecorder = m_device.createCommandRecorder();

    if (!m_textureWasInitialized && m_videoTexture.isValid()) {
        commandRecorder.textureMemoryBarrier(TextureMemoryBarrierOptions{
                .srcStages = PipelineStageFlags(PipelineStageFlagBit::TopOfPipeBit),
                .dstStages = PipelineStageFlags(PipelineStageFlagBit::FragmentShaderBit),
                .oldLayout = TextureLayout::Undefined,
                .newLayout = TextureLayout::ShaderReadOnlyOptimal,
                .texture = m_videoTexture,
                .range = {
                        .aspectMask = TextureAspectFlagBits::ColorBit,
                },
        });
        m_textureWasInitialized = true;
    }

    // OpaquePass
    m_opaquePassOptions.colorAttachments[0].view = m_swapchainViews.at(m_currentSwapchainImageIndex);

    auto opaquePass = commandRecorder.beginRenderPass(m_opaquePassOptions);
    if (m_videoTexture.isValid()) {
        opaquePass.setPipeline(m_pipeline);
        opaquePass.setVertexBuffer(0, m_vertexBuffer);
        opaquePass.setBindGroup(0, m_bindGroup);

        const Scaling aspectRatioScaling = computeScaling(m_window->width(), m_window->height(),
                                                          m_videoResolution.first, m_videoResolution.second);
        opaquePass.pushConstant(m_aspectRatioPushConstant, &aspectRatioScaling);
        opaquePass.draw(DrawCommand{ .vertexCount = 6 });
    }
    opaquePass.end();

    // End recording
    m_commandBuffers[m_inFlightIndex].emplace_back(commandRecorder.finish());
    auto commandBufferHandle = m_commandBuffers[m_inFlightIndex].back().handle();

    // If no texture was uploaded -> wait for prior presentation completion
    // If texture was uploaded -> wait for texture upload completion (since texture upload waits on presentation completion)
    const auto waitSemaphores = m_textureUploadPerformed ? std::vector<Handle<GpuSemaphore_t>>{ m_uploadCompleteSemaphores[m_inFlightIndex] }
                                                         : std::vector<Handle<GpuSemaphore_t>>{ m_presentCompleteSemaphores[m_inFlightIndex] };

    // Submit command buffer to queue
    const SubmitOptions submitOptions = {
        .commandBuffers = { commandBufferHandle },
        .waitSemaphores = std::move(waitSemaphores),
        .signalSemaphores = { m_renderCompleteSemaphores[m_inFlightIndex] },
        .signalFence = m_frameFences[m_inFlightIndex],
    };

    m_queue.submit(submitOptions);
}
