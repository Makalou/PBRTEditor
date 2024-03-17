#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

#include "built_in/frameGlobalData.glsl"

USE_FRAME_GLOBAL_DATA;

layout(set = 1, binding = 0) uniform sampler2D inputTex0;
layout(set = 1, binding = 1) uniform sampler2D inputTex1;
layout(set = 1, binding = 2) uniform sampler2D inputTex2;
layout(set = 1, binding = 3) uniform sampler2D inputTex3;
layout(set = 1, binding = 4) uniform sampler2D inputTex4;

layout( push_constant ) uniform constants
{
	uvec4 inputTexIdx;
} PushConstants;


void main() {
    switch(PushConstants.inputTexIdx.x)
    {
        case 0:
        outColor = vec4(texture(inputTex0,inUV).rgb,1.0);
        break;
        case 1:
        outColor = vec4(texture(inputTex1,inUV).rgb,1.0);
        break;
        case 2:
        outColor = vec4(texture(inputTex2,inUV).rgb,1.0);
        break;
        case 3:
        outColor = vec4(texture(inputTex3,inUV).rgb,1.0);
        break;
        case 4:
        outColor = vec4(texture(inputTex4,inUV).rgb,1.0);
        break;
        default:
        outColor = vec4(vec3(0.0),1.0);
        break;
    }
}
