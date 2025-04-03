#pragma once

namespace pass::compositing::shader::composite_present {
// vertex shader
// - input
static constexpr size_t vertexInPositionLocation = 0;
static constexpr size_t vertexInTexCoordLocation = 1;

// fragment shader
// - binding
static constexpr size_t fragmentUniformColorTextureSet = 0;
static constexpr size_t fragmentUniformAlbedoPassTextureBinding = 0;
static constexpr size_t fragmentUniformOtherChannelPassTextureBinding = 1;

// - output
static constexpr size_t fragmentColorOutputLocation = 0;
}
