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

layout(location = 5) flat in uint instanceID;

layout(location = 0) out vec4 outFlat;
layout(location = 1) out vec4 outMeshID;
layout(location = 2) out vec4 outFragPosition;
layout(location = 3) out vec4 outFragNormal;
layout(location = 4) out vec4 outFragUV;
layout(location = 5) out vec4 outFragAlbedo;
layout(location = 6) out vec4 outEncodedMeshID;
layout(location = 7) out vec4 outEncodedInstanceID;

layout(set = 2, binding = 1) uniform sampler2D reflectanceMap;

#if HAS_NORMALMAP

#endif

layout( push_constant ) uniform constants
{
    uvec4 ID;
} pushConstant;

//layout(set = 3, binding = 0, std430) coherent buffer AtomicBuffer {
//    uint meshID;
//    uint instanceID;
//} atomicBuffer;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 uintToColor(uint id,uint range)
{
    // Convert instance ID to a value between 0 and 1
    float normalizedID = float(id) / float(range); // MAX_INSTANCES is the maximum number of instances you have

    // Define hue, saturation, and value
    float hue = normalizedID;  // Vary hue based on instance ID
    float saturation = 1.0;     // Full saturation
    float value = 1.0;          // Full value

    // Convert HSV to RGB
    vec3 rgbColor = hsv2rgb(vec3(hue, saturation, value));

    return rgbColor;
}

vec3 encodeUint(uint val)
{
    uint x = (val & 0x00FF0000u) >> 16u;
    uint y = (val & 0x0000FF00u) >> 8u;
    uint z = (val & 0x000000FFu);
    return vec3(float(x) / 255.0, float(y) / 255.0, float(z) / 255.0);
}

void main() {
//    #if HAS_VERTEX_UV
//    outFragColor = vec4(inFragUV,1.0, 1.0);
//    #else
//    outFragColor = vec4(1.0,0.0,1.0, 1.0);
//    #endif
    //outFragColor = vec4(vec3(0.5) * max(0.0,dot(inFragNormal,normalize(vec3(1.0,1.0,1.0)))), 1.0);
    //outFragColor = vec4(inFragNormal,1.0);
//    atomicExchange(atomicBuffer.meshID,pushConstant.ID.x);
//    atomicExchange(atomicBuffer.instanceID,instanceID);
    outFlat = vec4(vec3(0.5),1.0);
    outMeshID = vec4(uintToColor(pushConstant.ID.x,pushConstant.ID.y),1.0);
    outEncodedMeshID = vec4(encodeUint(pushConstant.ID.x),1.0);
    outEncodedInstanceID = vec4(encodeUint(instanceID),1.0);
    outFragPosition = vec4(inFragPosition,1.0);
    #if HAS_VERTEX_NORMAL
    outFragNormal = vec4(inFragNormal,1.0);
    #endif
    #if HAS_VERTEX_UV
    outFragUV = vec4(inFragUV,0.0,1.0);
    vec4 reflectance = texture(reflectanceMap,inFragUV);
    #else
    outFragUV = vec4(0.0,0.0,1.0,1.0);
    vec4 reflectance = texture(reflectanceMap,vec2(0.0));
    #endif
    outFragAlbedo = vec4(reflectance.rgb,1.0);
}