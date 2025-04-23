/*
  This file is part of KDGpu Examples.
  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "compositing_pass.h"

#include <KDGpu/graphics_pipeline_options.h>

#include "shader/composit_present.h"

#include <KDUtils/dir.h>

#include <global_resources.h>

namespace pass::compositing {

void CompositingPass::initialize(Format swapchainFormat)
{
    using namespace pass::compositing;

    auto &device = kdgpu_ext::graphics::GlobalResources::instance().graphicsDevice();

    // initialize full screen quad data
    {
        m_quad.topology = PrimitiveTopology::TriangleStrip;
        m_quad.vertices = {
            { -1.0f, 1.0f, 0.0f },
            { 1.0f, 1.0f, 0.0f },
            { -1.0f, -1.0f, 0.0f },
            { 1.0f, -1.0f, 0.0f }
        };
        m_quad.textureCoordinates = {
            { 0.0f, 1.0f },
            { 1.0f, 1.0f },
            { 0.0f, 0.0f },
            { 1.0f, 0.0f }
        };
        m_quad.initializeFromData();
    }

    // Create a sampler to sample from the color texture in the input pass
    m_colorOutputSampler = device.createSampler();

    // load shader
    m_shader.setBaseDirectory("gltf/gltf_render_pbr/shader");
    m_shader.loadVertexShader("composite_present.vert.glsl.spv");
    m_shader.loadFragmentShader("composite_present.frag.glsl.spv");

    // Create bind group layout consisting of a two bindings holding the textures the previous passes rendered to
    {
        const BindGroupLayoutOptions bindGroupLayoutOptions = {
            .bindings = {
                    { //
                      .binding = shader::composit_present::fragmentUniformAlbedoPassTextureBinding,
                      .resourceType = ResourceBindingType::CombinedImageSampler,
                      .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit) },
                    { //
                      .binding = shader::composit_present::fragmentUniformOtherChannelPassTextureBinding,
                      .resourceType = ResourceBindingType::CombinedImageSampler,
                      .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit) },
                    { //
                      .binding = shader::composit_present::fragmentUniformBasicGeometryPassTextureBinding,
                      .resourceType = ResourceBindingType::CombinedImageSampler,
                      .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit) },
                    { //
                      .binding = shader::composit_present::fragmentUniformAreaLightPassTextureBinding,
                      .resourceType = ResourceBindingType::CombinedImageSampler,
                      .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit) },
                //
            }
        };
        m_bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutOptions);
    }

    // Create a pipeline layout (array of bind group layouts)
    const PipelineLayoutOptions pipelineLayoutOptions = {
        .bindGroupLayouts = { m_bindGroupLayout },
    };
    m_pipelineLayout = device.createPipelineLayout(pipelineLayoutOptions);

    // Create a pipeline
    GraphicsPipelineOptions pipelineOptions = {
        .shaderStages = m_shader.shaderStages(),
        .layout = m_pipelineLayout,
        .renderTargets = {
                { .format = swapchainFormat } }
    };

    m_quad.addToPipelineOptions(pipelineOptions, shader::composit_present::vertexInPositionLocation, shader::composit_present::vertexInTexCoordLocation);
    m_graphicsPipeline = device.createGraphicsPipeline(pipelineOptions);
}

void CompositingPass::deinitialize()
{
    m_shader.deinitialize();
    m_quad.deinitialize();
    m_bindGroup = {};
    m_bindGroupLayout = {};
    m_colorOutputSampler = {};
    m_graphicsPipeline = {};
    m_pipelineLayout = {};
}

void CompositingPass::update(float time)
{
}

void CompositingPass::setInputTextures(
        TextureTarget &pbrPassTexture,
        TextureTarget &otherChannelColorTexture,
        TextureTarget &basicGeometryTexture,
        TextureTarget &areaLightTexture
        )
{
    using namespace pass::compositing;

    m_pbrPassTexture = &pbrPassTexture;
    m_sourceOtherChannelColorTexture = &otherChannelColorTexture;
    m_basicGeometryTexture = &basicGeometryTexture;
    m_areaLightTexture = &areaLightTexture;

    // Create a bindGroup for the source textures
    m_bindGroup = kdgpu_ext::graphics::GlobalResources::instance().graphicsDevice().createBindGroup(
            {
                    //
                    .layout = m_bindGroupLayout,
                    .resources = {
                            {
                                    //
                                    .binding = shader::composit_present::fragmentUniformAlbedoPassTextureBinding, //
                                    .resource = TextureViewSamplerBinding{
                                            .textureView = m_pbrPassTexture->textureView(),
                                            .sampler = m_colorOutputSampler } // resource

                            },
                            {
                                    //
                                    .binding = shader::composit_present::fragmentUniformOtherChannelPassTextureBinding, //
                                    .resource = TextureViewSamplerBinding{
                                            //
                                            .textureView = m_sourceOtherChannelColorTexture->textureView(), //
                                            .sampler = m_colorOutputSampler //
                                    } // resource
                            },
                            {
                                    //
                                    .binding = shader::composit_present::fragmentUniformBasicGeometryPassTextureBinding, //
                                    .resource = TextureViewSamplerBinding{
                                            //
                                            .textureView = m_basicGeometryTexture->textureView(), //
                                            .sampler = m_colorOutputSampler //
                                    } // resource
                            },
                            {
                                    //
                                    .binding = shader::composit_present::fragmentUniformAreaLightPassTextureBinding, //
                                    .resource = TextureViewSamplerBinding{
                                            //
                                            .textureView = m_areaLightTexture->textureView(), //
                                            .sampler = m_colorOutputSampler //
                                    } // resource
                            },

                            //
                    } // resources
            } // bind group options
    );
}

void CompositingPass::render(CommandRecorder &commandRecorder, TextureView &swapchainView)
{
    auto renderPass = commandRecorder.beginRenderPass({
            .colorAttachments = {
                    {
                            .view = swapchainView,
                            .loadOperation = AttachmentLoadOperation::DontCare, // Don't clear color
                            .finalLayout = TextureLayout::PresentSrc,
                    } },
    });
    renderPass.setPipeline(m_graphicsPipeline);
    renderPass.setBindGroup(0, m_bindGroup);
    m_quad.render(renderPass);
    renderPass.end();
}
} // namespace pass::compositing
