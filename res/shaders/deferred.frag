#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

#include "built_in/frameGlobalData.glsl"

USE_FRAME_GLOBAL_DATA

layout(set = 1, binding = 0) uniform sampler2D wPos;
//layout(set = 1, binding = 1) uniform sampler2D wNormal;

void main() {
    vec2 anchor = FGData.mousePos.xy;
    if(distance(gl_FragCoord.xy,anchor) < 10.0)
    {
        outColor = vec4(1.0,0.0,0.0,1.0);
    }else{
        discard;
    }
}
