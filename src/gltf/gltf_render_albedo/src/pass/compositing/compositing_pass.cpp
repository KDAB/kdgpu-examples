#include "compositing_pass.h"

#include <KDGpu/buffer_options.h>
#include <KDGpu/graphics_pipeline_options.h>

#include "shader/composite_present.h"

#include <KDUtils/dir.h>

namespace pass::compositing {
void CompositingPass::initialize(Format swapchainFormat)
{
    using namespace pass::compositing;
    auto& device = kdgpu_ext::graphics::GlobalResources::instance().graphicsDevice();

    // Create a buffer to hold a full screen quad. This will be drawn as a triangle-strip (see pipeline creation below).
    {
        BufferOptions bufferOptions = {
            .size = 4 * (3 + 2) * sizeof(float),
            .usage = BufferUsageFlagBits::VertexBufferBit,
            .memoryUsage = MemoryUsage::CpuToGpu // allow mapping to CPU address space
        };
        m_fullScreenQuad = device.createBuffer(bufferOptions);

        std::array<float, 20> vertexData = {
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f, -1.0f, 0.0f, 1.0f, 0.0f
        };

        auto bufferData = m_fullScreenQuad.map();
        std::memcpy(bufferData, vertexData.data(), vertexData.size() * sizeof(float));
        m_fullScreenQuad.unmap();
    }

    // Create a sampler to sample from the color texture in the input pass
    m_colorOutputSampler = device.createSampler();

    // Load vertex shader and fragment shader (spir-v only for now)
    m_shader.setBaseDirectory("gltf/gltf_render_albedo/shader/");
    m_shader.loadVertexShader("composite_present.vert.glsl.spv");
    m_shader.loadFragmentShader("composite_present.frag.glsl.spv");

    // Create bind group layout consisting of a single binding holding the texture the previous pass rendered to
    {
        const BindGroupLayoutOptions bindGroupLayoutOptions = {
            .bindings = { { //
                .binding = shader::composite_present::fragmentUniformAlbedoPassTextureBinding,
                .resourceType = ResourceBindingType::CombinedImageSampler,
                .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit) },
              { //
                  .binding = shader::composite_present::fragmentUniformOtherChannelPassTextureBinding,
                  .resourceType = ResourceBindingType::CombinedImageSampler,
                  .shaderStages = ShaderStageFlags(ShaderStageFlagBits::FragmentBit) } }
        };
        m_bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutOptions);
    }

    // Create a pipeline layout (array of bind group layouts)
    // clang-format off
    const PipelineLayoutOptions pipelineLayoutOptions = {
        .bindGroupLayouts = { m_bindGroupLayout },
    };
    // clang-format on
    m_pipelineLayout = device.createPipelineLayout(pipelineLayoutOptions);

    // Create a pipeline
    // clang-format off
    const GraphicsPipelineOptions pipelineOptions = {
        .shaderStages = m_shader.shaderStages(),
        .layout = m_pipelineLayout,
        .vertex = {
            .buffers = {
                {
                    .binding = 0,
                    .stride = (3 + 2) * sizeof(float)
                }
            },
            .attributes = {
                // Position
                {
                    .location = shader::composite_present::vertexInPositionLocation,
                    .binding = 0,
                    .format = Format::R32G32B32_SFLOAT
                },

                // Texture coordinates
                {
                    .location = shader::composite_present::vertexInTexCoordLocation,
                    .binding = 0,
                    .format = Format::R32G32_SFLOAT,
                    .offset = 3 * sizeof(float)
                }
            }
        },
        .renderTargets = {
            { .format = swapchainFormat }
        },
        .primitive = {
            .topology = PrimitiveTopology::TriangleStrip
        }
    };
    // clang-format on
    m_graphicsPipeline = device.createGraphicsPipeline(pipelineOptions);
}

void CompositingPass::deinitialize()
{
    m_shader.deinitialize();
    m_fullScreenQuad = {};
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
        Device &device,
        TextureTarget &albedoColorTexture,
        TextureTarget &otherChannelColorTexture)
{
    using namespace pass::compositing;

    m_pbrPassTexture = &albedoColorTexture;
    m_sourceOtherChannelColorTexture = &otherChannelColorTexture;

    // Create a bindGroup for the source textures
    // clang-format off
    m_bindGroup = device.createBindGroup(
    {
            .layout = m_bindGroupLayout,
            .resources =
            {
                {
                       .binding = shader::composite_present::fragmentUniformAlbedoPassTextureBinding,
                       .resource = TextureViewSamplerBinding
                       {
                            .textureView = m_pbrPassTexture->textureView(),
                            .sampler = m_colorOutputSampler
                       }
                },
                {
                       .binding = shader::composite_present::fragmentUniformOtherChannelPassTextureBinding,
                       .resource = TextureViewSamplerBinding
                       {
                            .textureView = m_sourceOtherChannelColorTexture->textureView(),
                            .sampler = m_colorOutputSampler
                       }
                }
            }
    });
    // clang-format on
}

void CompositingPass::render(
        CommandRecorder &commandRecorder,
        TextureView &swapchainView)
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
    renderPass.setVertexBuffer(0, m_fullScreenQuad);
    renderPass.setBindGroup(0, m_bindGroup);
    renderPass.draw(DrawCommand{ .vertexCount = 4 });
    renderPass.end();
}
}
