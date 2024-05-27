#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

#include "built_in/frameGlobalData.glsl"
#include "built_in/camera.glsl"
#include "built_in/random.glsl"
#include "built_in/sample.glsl"

USE_FRAME_GLOBAL_DATA;

layout(set = 1, binding = 0) uniform sampler2D wDepth;
layout(set = 1, binding = 1) uniform sampler2D wPostion;
layout(set = 1, binding = 2) uniform sampler2D wNormal;

layout(set = 2, binding = 0) uniform BCAMERA_BLOCK_LAYOUT camera;

void main() {
    const float kernel_size = 1.0;
    const int max_sample_count = 32;
    vec3 normal = texture(wNormal, inUV).xyz;
    vec3 position = texture(wPostion, inUV).xyz;

    vec2 seed = inUV * FGData.time.x;
    int count = 0;
    for(int i = 0; i < max_sample_count; i++)
    {
        seed.x = blue_noise(seed);
        seed.y = blue_noise(seed);

        vec3 sample_point_wpos = position + (sample_sphere_uniform(seed) * kernel_size * blue_noise(seed));
        vec4 sample_point_ppos = camera.proj * camera.view * vec4(sample_point_wpos,1.0);
        float sample_point_depth = sample_point_ppos.z / sample_point_ppos.w;

        if(sample_point_depth < texture(wDepth,inUV).x)
        {
            count = count + 1;
        }
    }
    float ao = 1.0 - float(count) / float(max_sample_count);

    outColor = vec4(vec3(ao),1.0);
}
