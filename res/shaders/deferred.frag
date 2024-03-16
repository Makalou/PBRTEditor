#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

#include "built_in/frameGlobalData.glsl"

USE_FRAME_GLOBAL_DATA;

layout(set = 1, binding = 0) uniform sampler2D wPos;
layout(set = 1, binding = 1) uniform sampler2D wNormal;
layout(set = 1, binding = 2) uniform sampler2D albedoMap;

void main() {
    // vec2 anchor = FGData.mousePos.xy;
    // if(distance(gl_FragCoord.xy,anchor) < 10.0)
    // {
    //     outColor = vec4(1.0,0.0,0.0,1.0);
    // }else{
    //     discard;
    // }
    vec3 worldPosition = texture(wPos,inUV).xyz;
    vec3 worldNormal = texture(wNormal,inUV).xyz;
    vec3 albedo = texture(albedoMap,inUV).rgb;

    if(texture(wPos,inUV).w == 0.0)
    {
        discard;
    }else{
        vec3 lightDir = vec3(1.0);
        vec3 color = albedo * max(0.0,dot(worldNormal,lightDir));
        outColor = vec4(color,1.0);
    }
}
