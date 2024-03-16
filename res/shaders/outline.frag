#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

//layout(constant_id = 0) const bool IS_SOMETHING = false;

#include "built_in/frameGlobalData.glsl"

USE_FRAME_GLOBAL_DATA;

layout(set = 1, binding = 0) uniform sampler2D inputTex;

void main() {
    // Define Sobel kernels
    const mat3 sobelX = mat3(-1, 0, 1, -2, 0, 2, -1, 0, 1);
    const mat3 sobelY = mat3(-1, -2, -1, 0, 0, 0, 1, 2, 1);
     // Compute gradients using Sobel operator
    float gx = 0.0;
    float gy = 0.0;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            vec2 offset = vec2(i, j) * 0.001;
            vec3 color = texture(inputTex, inUV + offset).rgb;
            gx += color.r * sobelX[i+1][j+1];
            gy += color.r * sobelY[i+1][j+1];
        }
    }
    // Compute gradient magnitude
    float gradientMagnitude = sqrt(gx * gx + gy * gy);
    outColor = vec4(vec3(gradientMagnitude),1.0);
}
