#version 450
#extension GL_GOOGLE_include_directive : enable

#include "shadingCommon.glsl"

struct CoatedConductorData
{
    ShadingGeoData geo;
    vec4 albedo;
    float g;
    int maxdepth;
    int nsamples;
    float thickness;
};

vec4 evaluateCoatedConductor(in CoatedConductorData data)
{
    return data.albedo;
}