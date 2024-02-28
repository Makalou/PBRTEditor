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

layout(set = 2, binding = 1) uniform sampler2D reflectanceMap;

#if HAS_NORMALMAP

#endif

void main() {
//    #if HAS_VERTEX_UV
//    outFragColor = vec4(inFragUV,1.0, 1.0);
//    #else
//    outFragColor = vec4(1.0,0.0,1.0, 1.0);
//    #endif
    //outFragColor = vec4(vec3(0.5) * max(0.0,dot(inFragNormal,normalize(vec3(1.0,1.0,1.0)))), 1.0);
    //outFragColor = vec4(inFragNormal,1.0);
    #if HAS_VERTEX_UV
    vec4 reflectance = texture(reflectanceMap,inFragUV);
    if(reflectance.a == 0)
    {
        discard;
    }
    outFragColor = vec4(reflectance.rgb,1.0);
    #else
    outFragColor = vec4(1.0,0.0,1.0, 1.0);
    #endif
}