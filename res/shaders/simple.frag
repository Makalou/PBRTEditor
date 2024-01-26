#version 450

layout(location = 0) in vec3 inFragPosition;

#if HAS_VERTEX_NORMAL
layout(location = 1) in vec3 inFragNormal;
#endif

#if HAS_VERTEX_TANGENT_AND_BITANGENT
layout(location = 2) in vec3 inFragTangent;
layout(location = 3) in vec3 inFragBiTangent;
#endif

#if HAS_VERTEX_UV
layout(location = 4) in vec2 inFragUV;
#endif

layout(location = 0) out vec4 outFragColor;

#if HAS_NORMALMAP

#endif

void main() {
    outFragColor = vec4(1.0, 0.0, 0.0, 1.0);
}