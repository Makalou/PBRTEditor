#version 450

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
layout(location = 5) in mat4 inInstWTransform;

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

void main() {
    gl_Position = vec4(vec3(0.0), 1.0);
}
