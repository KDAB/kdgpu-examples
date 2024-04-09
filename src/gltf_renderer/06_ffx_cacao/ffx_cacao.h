/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include <KDGpu/bind_group_layout.h>
#include <KDGpu/bind_group.h>
#include <KDGpu/buffer.h>
#include <KDGpu/compute_pipeline.h>
#include <KDGpu/device.h>
#include <KDGpu/pipeline_layout.h>
#include <KDGpu/sampler.h>
#include <KDGpu/shader_module.h>
#include <KDGpu/texture.h>
#include <KDGpu/texture_view.h>
#include <KDGpu/command_buffer.h>

#include <glm/glm.hpp>

#include <array>
#include <vector>

class FFX_Cacao
{
public:
    explicit FFX_Cacao(KDGpu::Device *device);
    ~FFX_Cacao();

    enum class Quality : uint32_t {
        Lowest = 0,
        Low = 1,
        Medium = 2,
        High = 3,
        Highest = 4,
    };

    struct Settings {
        Settings(){};

        float radius = 1.2f; ///< [0.0,  ~ ] World (view) space size of the occlusion sphere.
        float shadowMultiplier = 1.0f; ///< [0.0, 5.0] Effect strength linear multiplier.
        float shadowPower = 1.5f; ///< [0.5, 5.0] Effect strength pow modifier.
        float shadowClamp = 0.98f; ///< [0.0, 1.0] Effect max limit (applied after multiplier but before blur).
        float horizonAngleThreshold = 0.06f; ///< [0.0, 0.2] Limits self-shadowing (makes the sampling area less of a hemisphere, more of a spherical cone, to avoid self-shadowing and various artifacts due to low tessellation and depth buffer imprecision, etc.).
        float fadeOutFrom = 50.0f; ///< [0.0,  ~ ] Distance to start fading out the effect.
        float fadeOutTo = 300.0f; ///< [0.0,  ~ ] Distance at which the effect is faded out.
        Quality qualityLevel = Quality::Medium; ///<            Effect quality, affects number of taps etc.
        float adaptiveQualityLimit = 0.45f; ///< [0.0, 1.0] (only for quality level FFX_CACAO_QUALITY_HIGHEST).
        uint32_t blurPassCount = 2; ///< [  0,   8] Number of edge-sensitive smart blur passes to apply.
        float sharpness = 0.98f; ///< [0.0, 1.0] (How much to bleed over edges; 1: not at all, 0.5: half-half; 0.0: completely ignore edges).
        float temporalSupersamplingAngleOffset = 0.0f; ///< [0.0,  PI] Used to rotate sampling kernel; If using temporal AA / supersampling, suggested to rotate by ( (frame%3)/3.0*PI ) or similar. Kernel is already symmetrical, which is why we use PI and not 2*PI.
        float temporalSupersamplingRadiusOffset = 0.0f; ///< [0.0, 2.0] Used to scale sampling kernel; If using temporal AA / supersampling, suggested to scale by ( 1.0f + (((frame%3)-1.0)/3.0)*0.1 ) or similar.
        float detailShadowStrength = 0.5f; ///< [0.0, 5.0] Used for high-res detail AO using neighboring depth pixels: adds a lot of detail but also reduces temporal stability (adds aliasing).
        bool generateNormals = true; ///< This option should be set to FFX_CACAO_TRUE if FidelityFX-CACAO should reconstruct a normal buffer from the depth buffer. It is required to be FFX_CACAO_TRUE if no normal buffer is provided.
        float bilateralSigmaSquared = 5.0f; ///< [0.0,  ~ ] Sigma squared value for use in bilateral upsampler giving Gaussian blur term. Should be greater than 0.0.
        float bilateralSimilarityDistanceSigma = 0.01f; ///< [0.0,  ~ ] Sigma squared value for use in bilateral upsampler giving similarity weighting for neighbouring pixels. Should be greater than 0.0.
    };

    struct BufferSizeInfo {
        uint32_t inputOutputBufferWidth;
        uint32_t inputOutputBufferHeight;

        uint32_t ssaoBufferWidth;
        uint32_t ssaoBufferHeight;

        uint32_t depthBufferXOffset;
        uint32_t depthBufferYOffset;

        uint32_t depthBufferWidth;
        uint32_t depthBufferHeight;

        uint32_t deinterleavedDepthBufferXOffset;
        uint32_t deinterleavedDepthBufferYOffset;

        uint32_t deinterleavedDepthBufferWidth;
        uint32_t deinterleavedDepthBufferHeight;

        uint32_t importanceMapWidth;
        uint32_t importanceMapHeight;

        uint32_t downsampledSsaoBufferWidth;
        uint32_t downsampledSsaoBufferHeight;

        BufferSizeInfo() = default;
        explicit BufferSizeInfo(uint32_t width, uint32_t height, bool useDownsampledSsao = false);
    };

    void initializeScene(KDGpu::Format colorFormat, KDGpu::Format depthFormat, Settings settings = Settings());
    void cleanupScene();
    void resize(uint32_t width, uint32_t height, const KDGpu::Texture &depthTexture,
                const KDGpu::TextureView &depthView, const KDGpu::TextureView &normalsView = {});
    void updateScene(const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix);
    KDGpu::CommandBuffer render(const KDGpu::TextureView &swapchainImageView);

private:
    void initializeCacaoComputePass();
    void initializeCacaoFullScreenPass(KDGpu::Format colorFormat, KDGpu::Format depthFormat);

    void cleanupCacaoComputePass();
    void cleanupCacaoFullScreenPass();

    void resizeCacaoComputePass(uint32_t width, uint32_t height, const KDGpu::TextureView &depthView, const KDGpu::TextureView &normalsView);
    void resizeCacaoFullScreenPass(uint32_t width, uint32_t height, const KDGpu::TextureView &depthView);

    void cacaoComputePass(KDGpu::CommandRecorder &recorder);
    void cacaoApplyFullScreen(KDGpu::CommandRecorder &recorder, const KDGpu::TextureView &swapchainImageView);

    // Compute CACAO Pass
    std::vector<KDGpu::Sampler> m_samplers;
    std::vector<KDGpu::BindGroupLayout> m_bindGroupLayouts;
    std::vector<KDGpu::PipelineLayout> m_pipelineLayouts;
    std::vector<KDGpu::ShaderModule> m_shaderModules;
    std::vector<KDGpu::ComputePipeline> m_pipelines;
    std::vector<KDGpu::BindGroup> m_bindGroups;
    std::vector<KDGpu::Texture> m_textures;
    std::vector<KDGpu::TextureView> m_textureViews;
    std::vector<KDGpu::Buffer> m_ubos;

    KDGpu::Texture m_loadCounter;
    KDGpu::TextureView m_loadCounterView;

    // FullScreen Quad Pass
    KDGpu::BindGroupLayout m_fsqTextureBindGroupLayout;
    KDGpu::PipelineLayout m_fsqPipelineLayout;
    KDGpu::GraphicsPipeline m_fsqPipeline;
    KDGpu::RenderPassCommandRecorderOptions m_fsqPassOptions;
    KDGpu::BindGroup m_fsqTextureBindGroup;
    KDGpu::Sampler m_fsqPassSampler;

    // Common
    KDGpu::Texture m_output;
    KDGpu::TextureView m_outputView;

    KDGpu::Handle<KDGpu::Texture_t> m_depthHandle;
    KDGpu::TextureLayout m_oldDepthTextureLayout = KDGpu::TextureLayout::DepthStencilAttachmentOptimal;

    KDGpu::Device *m_device{ nullptr };

    Settings m_settings;
    BufferSizeInfo m_bufferSizeInfo;
};
