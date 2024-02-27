#version 450
#extension GL_GOOGLE_include_directive : enable

#include "shadingCommon.glsl"

struct CoatedDiffuseData
{
    vec3 wNormal;
    float displacement;
    float roughness;
    float uroughenss;
    float vroughenss;
    bool remaproughness;
    vec4 albedo;
    float g;
    int maxdepth;
    int nsamples;
    float thickness;
};

vec4 evaluateCoatedDiffuse(in CoatedDiffuseData data)
{
    return data.albedo;
}

layout(set = 1, binding = 0) uniform CoatedDiffuseDataUniformBuffer{
    float roughness;
    float uroughness;
    float vroughness;
    bool remaproughness;
    vec4 albedo;
    bool useAledoMap;
    float g;
    bool useGMap;
    int maxdepth;
    int nsamples;
    float thickenss;
}coatedDiffuseData;

layout(set = 1, binding = 0) uniform coatedDiffuseNormalMap;
layout(set = 1, binding = 1) uniform coatedDiffuseAlbedoMap;
layout(set = 1, binding = 2) uniform coatedDiffuseGMap;
layout(set = 1, binding = 3) uniform sampler2D roughness;
layout(set = 1, binding = 4) uniform sampler2D uroughness;
layout(set = 1, binding = 5) uniform sampler2D vroughness;

CoatedDiffuseData getCoatedDiffuseData(vec2 uv)
{
    CoatedDiffuseData data;
    return data;
}








