#version 450
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 inVertexPosition;

#if HAS_VERTEX_NORMAL
layout(location = 1) in vec3 inVertexNormal;
#endif

#if HAS_VERTEX_TANGENT_AND_BITANGENT
layout(location = 2) in vec3 inVertexTangent;
layout(location = 3) in vec3 inVertexBiTangent;
#endif

#if HAS_VERTEX_UV
layout(location = 4) in vec2 inVertexUV;
#endif

// what if fragment shader also want transformation ...
layout(location = 5) in uint inInstDataIdx;

layout(location = 0) out vec3 outVertexPosition;

#if HAS_VERTEX_NORMAL
layout(location = 1) out vec3 outVertexNormal;
#endif

#if HAS_VERTEX_TANGENT_AND_BITANGENT
layout(location = 2) out vec3 outVertexTangent;
layout(location = 3) out vec3 outVertexBiTangent;
#endif

#if HAS_VERTEX_UV
layout(location = 4) out vec2 outVertexUV;
#endif

layout(location = 5) flat out uint outInstanceID;
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
    outInstanceID = inInstDataIdx;
    mat4 model = instData.transform[inInstDataIdx];
    vec3 position = vec3(inVertexPosition.x,inVertexPosition.y,inVertexPosition.z);
    vec4 worldPosition = model * vec4(position, 1.0);
    gl_Position = camera.proj * camera.view * worldPosition;
    outVertexPosition = worldPosition.xyz;
    outVertexNormal = normalize(mat3(transpose(inverse(model))) * inVertexNormal);
    #if HAS_VERTEX_UV
    outVertexUV = inVertexUV;
    #endif
}
