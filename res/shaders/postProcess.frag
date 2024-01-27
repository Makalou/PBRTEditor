#version 450

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(constant_id = 0) const bool IS_SOMETHING = false;

layout(set = 0, binding = 0) uniform sampler2D inputTex;

void main() {
    outColor = vec4(inUV,1.0, 1.0);
}
