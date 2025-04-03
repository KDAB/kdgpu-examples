/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "compute_n_body.h"

#include <KDGpuExample/engine.h>
#include <KDGpuExample/kdgpuexample.h>

#include <KDGpu/bind_group_layout_options.h>
#include <KDGpu/bind_group_options.h>
#include <KDGpu/buffer_options.h>
#include <KDGpu/compute_pipeline_options.h>
#include <KDGpu/graphics_pipeline_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/vulkan/vulkan_enums.h>

#include <KDUtils/file.h>

#include <glm/gtx/transform.hpp>

#include <ktx.h>
#include <ktxvulkan.h>

#include <spdlog/spdlog.h>

#include <cmath>
#include <fstream>
#include <random>
#include <string>

#include "example_utility.h"

namespace {

ktxResult loadKtxFile(const std::string &filename, ktxTexture **texture)
{
    ktxResult result = KTX_SUCCESS;
    KDUtils::File file(filename);
    if (!file.exists()) {
        SPDLOG_CRITICAL("Failed to load file {}. File does not exist", filename);
        exit(1);
    }
    result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, texture);
    return result;
}

} // namespace

ComputeNBody::ComputeNBody()
    : KDGpuExample::SimpleExampleEngineLayer()
{
    m_samples = SampleCountFlagBits::Samples8Bit;
}

void ComputeNBody::initializeScene()
{
    // Load the particle and gradient textures
    m_particleTexture = createTextureFromKtxFile(ExampleUtility::assetPath() + "/textures/particle01_rgba.ktx");
    m_particleTextureView = m_particleTexture.createView();
    m_gradientTexture = createTextureFromKtxFile(ExampleUtility::assetPath() + "/textures/particle_gradient_rgba.ktx");
    m_gradientTextureView = m_gradientTexture.createView();
    m_sampler = m_device.createSampler(SamplerOptions{ .magFilter = FilterMode::Linear, .minFilter = FilterMode::Linear });

    // Create the buffer of initial particle positions and velocities
    initializeParticles();

    // Create a UBO for the compute pipeline
    const BufferOptions computeUboOptions = {
        .size = sizeof(ComputeUbo),
        .usage = BufferUsageFlagBits::UniformBufferBit,
        .memoryUsage = MemoryUsage::CpuToGpu
    };
    m_computeUbo = m_device.createBuffer(computeUboOptions);
    m_mappedComputeUbo = static_cast<ComputeUbo *>(m_computeUbo.map());

    // Create a UBO for the graphics pipeline
    const BufferOptions graphicsUbOptions = {
        .size = sizeof(GraphicsUbo),
        .usage = BufferUsageFlagBits::UniformBufferBit,
        .memoryUsage = MemoryUsage::CpuToGpu
    };
    m_graphicsUbo = m_device.createBuffer(graphicsUbOptions);
    m_mappedGraphicsUbo = static_cast<GraphicsUbo *>(m_graphicsUbo.map());
    m_graphicsUboData.screenDim = glm::vec2(m_window->width(), m_window->height());

    // Create a multisample texture into which we will render. The pipeline will then resolve the
    // multi-sampled texture into the current swapchain image.
    createRenderTarget();

    // Create the 2 x compute pipelines and 1 x graphics pipeline
    initializeGraphicsPipeline();
    initializeComputePipelines();

    // Set up options for barriers to correctly order the 2 compute phases
    // and the compute-graphics usages.
    m_computeGraphicsBufferBarrierOptions = {
        .srcStages = PipelineStageFlagBit::ComputeShaderBit,
        .srcMask = AccessFlagBit::ShaderWriteBit,
        .dstStages = PipelineStageFlagBit::VertexAttributeInputBit,
        .dstMask = AccessFlagBit::VertexAttributeReadBit,
        .buffer = m_particlesBuffer
    };
    m_computeBufferBarrierOptions = {
        .srcStages = PipelineStageFlagBit::ComputeShaderBit,
        .srcMask = AccessFlagBit::ShaderWriteBit,
        .dstStages = PipelineStageFlagBit::ComputeShaderBit,
        .dstMask = AccessFlagBit::ShaderReadBit,
        .buffer = m_particlesBuffer
    };

    // Setup the constant render pass options
    // clang-format off
    m_passOptions = {
        .colorAttachments = {
            {
                .view = m_msaaTextureView,
                .resolveView = {}, // Not setting the swapchain texture view just yet
                .clearValue = { 0.3f, 0.3f, 0.3f, 1.0f },
                .finalLayout = TextureLayout::PresentSrc
            }
        },
        .depthStencilAttachment = {
            .view = m_depthTextureView,
        },
        .samples = m_samples.get()
    };
    // clang-format on
}

void ComputeNBody::initializeParticles()
{
    constexpr std::array<glm::vec3, 6> attractors = {
        glm::vec3(5.0f, 0.0f, 0.0f),
        glm::vec3(-5.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, -5.0f),
        glm::vec3(0.0f, 4.0f, 0.0f),
        glm::vec3(0.0f, -8.0f, 0.0f),
    };

    const auto particleCount = static_cast<uint32_t>(attractors.size()) * m_particlesPerAttractor;
    std::vector<Particle> particles(particleCount);
    m_computeUboData.particleCount = static_cast<int32_t>(particleCount);

    std::default_random_engine rndEngine(0);
    std::normal_distribution<float> rndDist(0.0f, 1.0f);

    for (uint32_t i = 0; i < static_cast<uint32_t>(attractors.size()); i++) {
        for (uint32_t j = 0; j < m_particlesPerAttractor; j++) {
            Particle &particle = particles[i * m_particlesPerAttractor + j];

            // First particle in group as heavy center of gravity
            if (j == 0) {
                particle.pos = glm::vec4(attractors[i] * 1.5f, 90000.0f);
                particle.vel = glm::vec4(glm::vec4(0.0f));
            } else {
                // Position
                glm::vec3 position(attractors[i] + glm::vec3(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine)) * 0.75f);
                float len = glm::length(glm::normalize(position - attractors[i]));
                position.y *= 2.0f - (len * len);

                // Velocity
                glm::vec3 angular = glm::vec3(0.5f, 1.5f, 0.5f) * (((i % 2) == 0) ? 1.0f : -1.0f);
                glm::vec3 velocity = glm::cross((position - attractors[i]), angular) + glm::vec3(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine) * 0.025f);

                float mass = (rndDist(rndEngine) * 0.5f + 0.5f) * 75.0f;
                particle.pos = glm::vec4(position, mass);
                particle.vel = glm::vec4(velocity, 0.0f);
            }

            // Color gradient offset
            particle.vel.w = (float)i * 1.0f / static_cast<uint32_t>(attractors.size());
        }
    }

    // Create a buffer to hold the particle data. This will be used as an SSBO in the compute phase
    // and then as a Vertex buffer in the render phase.
    const DeviceSize particlesByteSize{ particles.size() * sizeof(Particle) };
    const BufferOptions options = {
        .size = particlesByteSize,
        .usage = BufferUsageFlagBits::VertexBufferBit | BufferUsageFlagBits::StorageBufferBit | BufferUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly
    };
    m_particlesBuffer = m_device.createBuffer(options);
    const WaitForBufferUploadOptions uploadOptions = {
        .destinationBuffer = m_particlesBuffer,
        .data = particles.data(),
        .byteSize = particlesByteSize
    };
    m_queue.waitForUploadBufferData(uploadOptions);
}

void ComputeNBody::initializeGraphicsPipeline()
{
    // Create a vertex shader and fragment shader (spir-v only for now)
    const auto vertexShaderPath = ExampleUtility::assetPath() + "/shaders/compute_n_body/particle.vert.spv";
    auto vertexShader = m_device.createShaderModule(KDGpuExample::readShaderFile(vertexShaderPath));

    const auto fragmentShaderPath = ExampleUtility::assetPath() + "/shaders/compute_n_body/particle.frag.spv";
    auto fragmentShader = m_device.createShaderModule(KDGpuExample::readShaderFile(fragmentShaderPath));

    // Create bind group layout consisting of a single binding holding a UBO
    // clang-format off
    const BindGroupLayoutOptions bindGroupLayoutOptions = {
        .bindings = {{
            .binding = 0,
            .resourceType = ResourceBindingType::CombinedImageSampler,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
        }, {
            .binding = 1,
            .resourceType = ResourceBindingType::CombinedImageSampler,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit)
        }, {
            .binding = 2,
            .resourceType = ResourceBindingType::UniformBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::VertexBit)
        }}
    };
    // clang-format on
    const BindGroupLayout bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutOptions);

    // Create a pipeline layout (array of bind group layouts)
    const PipelineLayoutOptions pipelineLayoutOptions = {
        .bindGroupLayouts = { bindGroupLayout }
    };
    m_graphicsPipelineLayout = m_device.createPipelineLayout(pipelineLayoutOptions);

    // Create a pipeline
    // clang-format off
    GraphicsPipelineOptions pipelineOptions = {
        .shaderStages = {
            { .shaderModule = vertexShader, .stage = ShaderStageFlagBits::VertexBit },
            { .shaderModule = fragmentShader, .stage = ShaderStageFlagBits::FragmentBit }
        },
        .layout = m_graphicsPipelineLayout,
        .vertex = {
            .buffers = {
                { .binding = 0, .stride = sizeof(Particle) }
            },
            .attributes = {
                { .location = 0, .binding = 0, .format = Format::R32G32B32A32_SFLOAT }, // Position
                { .location = 1, .binding = 0, .format = Format::R32G32B32A32_SFLOAT, .offset = sizeof(glm::vec4) } // Velocity
            }
        },
        .renderTargets = {{
            .format = m_swapchainFormat,
            .blending = {
                .blendingEnabled = true,
                .color = {
                    .srcFactor = BlendFactor::One,
                    .dstFactor = BlendFactor::One
                },
                .alpha = {
                    .srcFactor = BlendFactor::SrcAlpha,
                    .dstFactor = BlendFactor::DstAlpha
                }
            }
        }},
        .depthStencil = {
            .format = m_depthFormat,
            .depthTestEnabled = false,
            .depthWritesEnabled = false,
            .depthCompareOperation = CompareOperation::Always
        },
        .primitive = {
            .topology = PrimitiveTopology::PointList
        },
        .multisample = {
            .samples = m_samples.get()
        }
    };
    // clang-format on
    m_graphicsPipeline = m_device.createGraphicsPipeline(pipelineOptions);

    // Create a bindGroup to hold the graphics UBO and textures
    // clang-format off
    BindGroupOptions bindGroupOptions = {
        .layout = bindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = TextureViewSamplerBinding{ .textureView = m_particleTextureView, .sampler = m_sampler }
        }, {
            .binding = 1,
            .resource = TextureViewSamplerBinding{ .textureView = m_gradientTextureView, .sampler = m_sampler }
        }, {
            .binding = 2,
            .resource = UniformBufferBinding{ .buffer = m_graphicsUbo }
        }}
    };
    // clang-format on
    m_graphicsBindGroup = m_device.createBindGroup(bindGroupOptions);
}

void ComputeNBody::initializeComputePipelines()
{
    // clang-format off
    const BindGroupLayoutOptions bindGroupLayoutOptions = {
        .bindings = {{
            .binding = 0,
            .resourceType = ResourceBindingType::StorageBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::ComputeBit)
        }, {
            .binding = 1,
            .resourceType = ResourceBindingType::UniformBuffer,
            .shaderStages = ShaderStageFlags(ShaderStageFlagBits::ComputeBit)
        }}
    };
    // clang-format on
    const BindGroupLayout bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutOptions);

    // Create a pipeline layout (array of bind group layouts). The same layout is used by both compute pipelines
    const PipelineLayoutOptions pipelineLayoutOptions = { .bindGroupLayouts = { bindGroupLayout } };
    m_computePipelineLayout = m_device.createPipelineLayout(pipelineLayoutOptions);

    // Create a bindGroup to hold the UBO with the transform
    // clang-format off
    const BindGroupOptions bindGroupOptions {
        .layout = bindGroupLayout,
        .resources = {{
            .binding = 0,
            .resource = StorageBufferBinding{ .buffer = m_particlesBuffer }
        }, {
            .binding = 1,
            .resource = UniformBufferBinding{ .buffer = m_computeUbo }
        }}
    };
    // clang-format on
    m_computeBindGroup = m_device.createBindGroup(bindGroupOptions);

    {
        const auto calculateShaderPath = ExampleUtility::assetPath() + "/shaders/compute_n_body/particle_calculate.comp.spv";
        auto calculateShader = m_device.createShaderModule(KDGpuExample::readShaderFile(calculateShaderPath));
        const ComputePipelineOptions pipelineOptions{
            .layout = m_computePipelineLayout,
            .shaderStage = { .shaderModule = calculateShader }
        };
        m_calculatePipeline = m_device.createComputePipeline(pipelineOptions);
    }

    {
        const auto integrateShaderPath = ExampleUtility::assetPath() + "/shaders/compute_n_body/particle_integrate.comp.spv";
        auto integrateShader = m_device.createShaderModule(KDGpuExample::readShaderFile(integrateShaderPath));
        const ComputePipelineOptions pipelineOptions{
            .layout = m_computePipelineLayout,
            .shaderStage = { .shaderModule = integrateShader }
        };
        m_integratePipeline = m_device.createComputePipeline(pipelineOptions);
    }
}

void ComputeNBody::createRenderTarget()
{
    const TextureOptions options = {
        .type = TextureType::TextureType2D,
        .format = m_swapchainFormat,
        .extent = { .width = m_window->width(), .height = m_window->height(), .depth = 1 },
        .mipLevels = 1,
        .samples = m_samples.get(),
        .usage = TextureUsageFlagBits::ColorAttachmentBit,
        .memoryUsage = MemoryUsage::GpuOnly,
        .initialLayout = TextureLayout::Undefined
    };
    m_msaaTexture = m_device.createTexture(options);
    m_msaaTextureView = m_msaaTexture.createView();
}

Texture ComputeNBody::createTextureFromKtxFile(const std::string &filename)
{
    ktxTexture *ktxTexture{ nullptr };
    ktxResult result = loadKtxFile(filename, &ktxTexture);

    ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
    ktx_size_t ktxTextureSize = ktxTexture->dataSize;
    const auto format = ktxTexture_GetVkFormat(ktxTexture);
    const TextureOptions options = {
        .type = TextureType::TextureType2D,
        .format = vkFormatToFormat(format),
        .extent = { .width = ktxTexture->baseWidth, .height = ktxTexture->baseHeight, .depth = 1 },
        .mipLevels = ktxTexture->numLevels,
        .usage = TextureUsageFlagBits::SampledBit | TextureUsageFlagBits::TransferDstBit,
        .memoryUsage = MemoryUsage::GpuOnly,
        .initialLayout = TextureLayout::Undefined
    };
    Texture texture = m_device.createTexture(options);

    // Upload the texture data and transition to ShaderReadOnlyOptimal
    std::vector<BufferTextureCopyRegion> regions;
    regions.reserve(ktxTexture->numLevels);
    for (uint32_t i = 0; i < ktxTexture->numLevels; i++) {
        ktx_size_t offset;
        KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
        assert(result == KTX_SUCCESS);
        // clang-format off
        const BufferTextureCopyRegion region = {
            .bufferOffset = offset,
            .textureSubResource = {
                .aspectMask = TextureAspectFlagBits::ColorBit,
                .mipLevel = i
            },
            .textureExtent = {
                .width = std::max(1u, ktxTexture->baseWidth >> i),
                .height = std::max(1u, ktxTexture->baseHeight >> i),
                .depth = 1
            }
        };
        // clang-format on
        regions.push_back(region);
    }

    // clang-format off
    const TextureUploadOptions uploadOptions = {
        .destinationTexture = texture,
        .dstStages = PipelineStageFlagBit::AllGraphicsBit,
        .dstMask = AccessFlagBit::MemoryReadBit,
        .data = ktxTextureData,
        .byteSize = ktxTextureSize,
        .oldLayout = TextureLayout::Undefined,
        .newLayout = TextureLayout::ShaderReadOnlyOptimal,
        .regions = regions
    };
    // clang-format on
    uploadTextureData(uploadOptions);

    return std::move(texture);
}

void ComputeNBody::cleanupScene()
{
    m_graphicsUbo.unmap();
    m_graphicsUbo = {};
    m_mappedGraphicsUbo = nullptr;

    m_computeUbo.unmap();
    m_computeUbo = {};
    m_mappedComputeUbo = nullptr;

    m_particlesBuffer = {};
    m_sampler = {};
    m_particleTextureView = {};
    m_particleTexture = {};
    m_gradientTextureView = {};
    m_gradientTexture = {};

    m_msaaTextureView = {};
    m_msaaTexture = {};
    m_graphicsPipeline = {};
    m_graphicsPipelineLayout = {};
    m_commandBuffer = {};
    m_graphicsBindGroup = {};

    m_computePipelineLayout = {};
    m_calculatePipeline = {};
    m_integratePipeline = {};
    m_computeBindGroup = {};
}

void ComputeNBody::updateScene()
{
    // Update the camera matrices
    m_graphicsUboData.projection = m_camera.lens().projectionMatrix;
    m_graphicsUboData.view = m_camera.viewMatrix;
    std::memcpy(m_mappedGraphicsUbo, &m_graphicsUboData, sizeof(GraphicsUbo));

    // Update the compute UBO
    m_computeUboData.deltaT = engine()->deltaTimeSeconds();
    std::memcpy(m_mappedComputeUbo, &m_computeUboData, sizeof(ComputeUbo));
}

void ComputeNBody::resize()
{
    // Recreate the msaa render target texture
    createRenderTarget();

    // Swapchain might have been resized and texture views recreated. Ensure we update the PassOptions accordingly
    m_passOptions.colorAttachments[0].view = m_msaaTextureView;
    m_passOptions.depthStencilAttachment.view = m_depthTextureView;

    m_graphicsUboData.screenDim = glm::vec2(m_window->width(), m_window->height());
}

void ComputeNBody::render()
{
    auto commandRecorder = m_device.createCommandRecorder();

    // Record the compute commands. There are 2 phases: calculate and integrate. These two phases
    // must be separated by a buffer memory barrier to ensure the first compute phase has finished
    // writing before the second one begins reading from it.
    const uint32_t workGroupX = static_cast<uint32_t>(m_computeUboData.particleCount / 256);
    auto computePass = commandRecorder.beginComputePass();
    computePass.setBindGroup(0, m_computeBindGroup, m_computePipelineLayout);
    computePass.setPipeline(m_calculatePipeline);
    computePass.dispatchCompute(ComputeCommand{ .workGroupX = workGroupX });

    // Ensure the compute calculate shader is finished before the compute integration shader reads from it.
    commandRecorder.bufferMemoryBarrier(m_computeBufferBarrierOptions);
    computePass.setPipeline(m_integratePipeline);
    computePass.dispatchCompute(ComputeCommand{ .workGroupX = workGroupX });
    computePass.end();

    // Ensure the compute integration shader is finished before the vertex shader reads from it.
    commandRecorder.bufferMemoryBarrier(m_computeGraphicsBufferBarrierOptions);

    // Update the resolveView to use the current swapchain image
    m_passOptions.colorAttachments[0].resolveView = m_swapchainViews.at(m_currentSwapchainImageIndex);

    // Record the graphics commands
    auto renderPass = commandRecorder.beginRenderPass(m_passOptions);
    renderPass.setPipeline(m_graphicsPipeline);
    renderPass.setVertexBuffer(0, m_particlesBuffer);
    renderPass.setBindGroup(0, m_graphicsBindGroup);
    renderPass.draw(DrawCommand{ .vertexCount = static_cast<uint32_t>(m_computeUboData.particleCount) });
    renderPass.end();

    m_commandBuffer = commandRecorder.finish();

    SubmitOptions submitOptions = {
        .commandBuffers = { m_commandBuffer },
        .waitSemaphores = { m_presentCompleteSemaphores[m_inFlightIndex] },
        .signalSemaphores = { m_renderCompleteSemaphores[m_inFlightIndex] }
    };
    m_queue.submit(submitOptions);
}
