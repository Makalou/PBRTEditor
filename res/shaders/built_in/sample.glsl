#ifndef BUILT_IN_SAMPLE_GLSL
#define BUILT_IN_SAMPLE_GLSL

#define PI 3.1415926535

vec3 sample_sphere_uniform(in vec2 seed)
{
    float theta = 2.0 * PI *  seed.x;
    float phi = acos(2.0 * seed.y - 1.0);

    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);
    return vec3(x,y,z);
}

vec3 sample_hemisphere_uniform(in vec3 N, in vec2 seed)
{
    float theta = 2.0 * PI *  seed.x;
    float phi = acos(seed.y);

    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);
    return vec3(x,y,z);
}
#endif