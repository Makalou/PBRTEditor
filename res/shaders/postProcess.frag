#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

//layout(constant_id = 0) const bool IS_SOMETHING = false;

#include "built_in/frameGlobalData.glsl"

USE_FRAME_GLOBAL_DATA;

layout(set = 1, binding = 0) uniform sampler2D inputTex;

void main() {
    outColor = vec4(texture(inputTex,inUV).rgb,1.0);
}
