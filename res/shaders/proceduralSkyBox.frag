#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

#include "built_in/frameGlobalData.glsl"

USE_FRAME_GLOBAL_DATA

void main() {
    float offset = sin(FGData.time.x) * 0.5 + 0.5;
    float offset2 = cos(FGData.time.x) * 0.5 + 0.5;
    outColor = vec4(offset * inUV,offset2, 1.0);
}
