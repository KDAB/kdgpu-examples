#pragma once

namespace pass::compositing::shader::composit_present {
// vertex shader
// - input
static constexpr size_t vertexInPositionLocation = 0;
static constexpr size_t vertexInTexCoordLocation = 1;

// fragment shader
// - binding
static constexpr size_t fragmentUniformColorTextureSet = 0;
static constexpr size_t fragmentUniformAlbedoPassTextureBinding = 0;
static constexpr size_t fragmentUniformOtherChannelPassTextureBinding = 1;
static constexpr size_t fragmentUniformBasicGeometryPassTextureBinding = 2;
static constexpr size_t fragmentUniformAreaLightPassTextureBinding = 3;

// - output
static constexpr size_t fragmentColorOutputLocation = 0;
}
