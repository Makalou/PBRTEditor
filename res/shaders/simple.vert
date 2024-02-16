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
layout(location = 5) in uint inInstWTransform;

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

#include "built_in/frameGlobalData.glsl"
#include "built_in/camera.glsl"

USE_FRAME_GLOBAL_DATA

layout(set = 1, binding = 0) uniform BCAMERA_BLOCK_LAYOUT camera;

void main() {
    float t = FGData.time.x;
    float x = cos(t)*inVertexPosition.x - sin(t)*inVertexPosition.z;
    float z = cos(t)*inVertexPosition.x + sin(t)*inVertexPosition.z;
    vec3 position = 0.1 * vec3(x,-inVertexPosition.y,z);
    gl_Position = camera.view * vec4(position, 1.0);
    outVertexPosition = position;
}
