#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

#include "built_in/frameGlobalData.glsl"
#include "built_in/camera.glsl"

USE_FRAME_GLOBAL_DATA;

layout(set = 1, binding = 0) uniform BCAMERA_BLOCK_LAYOUT camera;

void main() {

    vec4 clipCoords = vec4(inUV * 2.0 - 1.0, 0.0, 1.0);
    vec4 viewCoords = inverse(camera.proj) * clipCoords;
    viewCoords.z = -1.0;
    viewCoords.w = 0.0;
    vec4 worldCoords = inverse(camera.view) * viewCoords;
    float a = 0.5*(normalize(worldCoords.xyz).y + 1.0);
    vec3 skyColor =  a * vec3(1.0, 1.0, 1.0) + (1.0 - a) * vec3(0.3, 0.5, 1.0);

    outColor = vec4(skyColor, 1.0);
}
