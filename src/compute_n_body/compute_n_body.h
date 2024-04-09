/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <tinygltf_helper/camera.h>

#include <KDGpuExample/simple_example_engine_layer.h>

#include <KDGpu/bind_group.h>
#include <KDGpu/buffer.h>
#include <KDGpu/graphics_pipeline.h>
#include <KDGpu/render_pass_command_recorder_options.h>
#include <KDGpu/sampler.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>

#include <glm/glm.hpp>

struct Particle {
    glm::vec4 pos; // xyz = position, w = mass
    glm::vec4 vel; // xyz = velocity, w = gradient texture position
};

// Compute pipeline uniform block object
struct ComputeUbo {
    float deltaT{ 0.0f };
    int32_t particleCount{ 0 };
};

// Graphics pipeline uniform block object
struct GraphicsUbo {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec2 screenDim;
};

class ComputeNBody : public KDGpuExample::SimpleExampleEngineLayer
{
public:
    ComputeNBody();

    TinyGltfHelper::Camera *camera() { return &m_camera; }

protected:
    void initializeScene() override;
    void cleanupScene() override;
    void updateScene() override;
    void render() override;
    void resize() override;

private:
    void initializeParticles();
    void initializeGraphicsPipeline();
    void initializeComputePipelines();
    void createRenderTarget();
    Texture createTextureFromKtxFile(const std::string &filename);

    TinyGltfHelper::Camera m_camera;

    const uint32_t m_particlesPerAttractor{ 4 * 1024 };
    ComputeUbo m_computeUboData;
    ComputeUbo *m_mappedComputeUbo{ nullptr };
    Buffer m_computeUbo;

    GraphicsUbo m_graphicsUboData;
    GraphicsUbo *m_mappedGraphicsUbo{ nullptr };
    Buffer m_graphicsUbo;

    Buffer m_particlesBuffer;

    Texture m_particleTexture;
    TextureView m_particleTextureView;
    Texture m_gradientTexture;
    TextureView m_gradientTextureView;
    Sampler m_sampler;

    Texture m_msaaTexture;
    TextureView m_msaaTextureView;
    PipelineLayout m_graphicsPipelineLayout;
    GraphicsPipeline m_graphicsPipeline;
    RenderPassCommandRecorderOptions m_passOptions;
    CommandBuffer m_commandBuffer;
    BindGroup m_graphicsBindGroup;

    PipelineLayout m_computePipelineLayout;
    ComputePipeline m_calculatePipeline;
    ComputePipeline m_integratePipeline;
    BindGroup m_computeBindGroup;

    BufferMemoryBarrierOptions m_computeBufferBarrierOptions;
    BufferMemoryBarrierOptions m_computeGraphicsBufferBarrierOptions;
};
