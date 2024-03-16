#version 450
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 inVertexPosition;

// what if fragment shader also want transformation ...
layout(location = 1) in uint inInstDataIdx;

#include "built_in/frameGlobalData.glsl"
#include "built_in/camera.glsl"

USE_FRAME_GLOBAL_DATA;

layout(set = 1, binding = 0) uniform BCAMERA_BLOCK_LAYOUT camera;
//layout(set = 2, binding = 0) uniform Material;
//https://app.diagrams.net/#G1ei8XsclhGNg_qMR_J7LBKmGRjSSXyShI#%7B%22pageId%22%3A%22sedBS7P0nTr2XQddadu8%22%7D
layout(std140,set = 2, binding = 0) readonly buffer PerInstanceData
{
    mat4 transform[];
} instData;

void main() {
    mat4 model = instData.transform[inInstDataIdx];
    vec3 position = vec3(inVertexPosition.x,inVertexPosition.y,inVertexPosition.z);
    vec4 worldPosition = model * vec4(position, 1.0);
    gl_Position = camera.proj * camera.view * worldPosition;
}
