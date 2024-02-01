#version 450

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

//layout(constant_id = 0) const bool IS_SOMETHING = false;

layout(set = 0, binding = 0) uniform FG
{
    vec4 time;
}FGData;

layout(set = 1, binding = 0) uniform sampler2D inputTex;

void main() {
    float offset = sin(FGData.time.x) * 0.5 + 0.5;
    float offset2 = cos(FGData.time.x) * 0.5 + 0.5;
    outColor = vec4(offset * inUV,offset2, 1.0);
}
