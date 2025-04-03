#pragma once

#include <global_resources.h>

#include <KDGpu/buffer_options.h>
#include <KDGpu/gpu_core.h>

namespace kdgpu_ext::graphics::vertex_buffer_object {
template <class TVertex, class TTexCoord>
class VertexBufferObject
{
public:
    std::vector<TVertex> vertices;
    std::vector<TTexCoord> textureCoordinates;
    PrimitiveTopology topology{ PrimitiveTopology::TriangleStrip };

    void initializeFromData()
    {
        auto &device = GlobalResources::instance().graphicsDevice();

        m_verticesBuffer = device.createBuffer(
                {
                        .size = vertices.size() * sizeof(TVertex),
                        .usage = BufferUsageFlagBits::VertexBufferBit,
                        .memoryUsage = MemoryUsage::CpuToGpu // allow mapping to CPU address space
                });

        m_texCoordsBuffer = device.createBuffer(
                {
                        .size = textureCoordinates.size() * sizeof(TTexCoord),
                        .usage = BufferUsageFlagBits::VertexBufferBit,
                        .memoryUsage = MemoryUsage::CpuToGpu // allow mapping to CPU address space
                });

        // upload vertices
        {
            auto buffer_data = m_verticesBuffer.map();
            std::memcpy(buffer_data, vertices.data(), vertices.size() * sizeof(TVertex));
            m_verticesBuffer.unmap();
        }

        // upload texture coordinates
        {
            auto buffer_data = m_texCoordsBuffer.map();
            std::memcpy(buffer_data, textureCoordinates.data(), textureCoordinates.size() * sizeof(TTexCoord));
            m_texCoordsBuffer.unmap();
        }
    }

    void addToPipelineOptions(GraphicsPipelineOptions& pipelineOptions, uint32_t vertexShaderLocation, uint32_t texCoordShaderLocation)
    {
        pipelineOptions.vertex = {
            .buffers = {
                {
                    .binding = 0,
                    .stride = sizeof(TVertex)
                },
                {
                    .binding = 1,
                    .stride = sizeof(TTexCoord)
                }
            },
            .attributes = {
                // Position
                {
                    .location = vertexShaderLocation,
                    .binding = 0,
                    .format = vectorFormat<TVertex>()
                },

                // Texture coordinates
                {
                    .location = texCoordShaderLocation,
                    .binding = 1,
                    .format = vectorFormat<TTexCoord>(),
                }
            }
        };

        assert(pipelineOptions.vertex.attributes[0].format != Format::UNDEFINED);
        assert(pipelineOptions.vertex.attributes[1].format != Format::UNDEFINED);

        pipelineOptions.primitive.topology = topology;
    }

    void deinitialize()
    {
        m_verticesBuffer = {};
        m_texCoordsBuffer = {};
    }

    void render(RenderPassCommandRecorder &renderPassRecorder)
    {
        renderPassRecorder.setVertexBuffer(0, m_verticesBuffer);
        renderPassRecorder.setVertexBuffer(1, m_texCoordsBuffer);
        renderPassRecorder.draw(DrawCommand{ .vertexCount = static_cast<uint32_t>(vertices.size()) });
    }

private:

    // for a glm vec type, return the correct KDGpu Format
    template<class TVec>
    constexpr Format vectorFormat()
    {
        if (TVec::length() == 2) {
            if (std::is_same<typename TVec::value_type, float>::value)
                return Format::R32G32_SFLOAT;
            return Format::UNDEFINED;
        }

        if (TVec::length() == 3) {
            if (std::is_same<typename TVec::value_type, float>::value)
                return Format::R32G32B32_SFLOAT;
            return Format::UNDEFINED;
        }

        if (TVec::length() == 4) {
            if (std::is_same<typename TVec::value_type, float>::value)
                return Format::R32G32B32A32_SFLOAT;
            return Format::UNDEFINED;
        }

        return Format::UNDEFINED;
    }

    Buffer m_verticesBuffer;
    Buffer m_texCoordsBuffer;
};
}; // namespace kdgpu_ext::graphics::vertex_buffer_object
