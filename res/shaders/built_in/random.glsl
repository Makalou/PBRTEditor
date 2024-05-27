#ifndef BUILT_IN_RANDOM_GLSL
#define BUILT_IN_RANDOM_GLSL

//https://www.shadertoy.com/view/tllcR2
#define hash(p)  fract(sin(dot(p, vec2(11.9898, 78.233))) * 43758.5453) // iq suggestion, for Windows

float blue_noise(vec2 U) {                           // 5-tap version 
    float v =  hash( U + vec2(-1, 0) )
             + hash( U + vec2( 1, 0) )
             + hash( U + vec2( 0, 1) )
             + hash( U + vec2( 0,-1) ); 
    return  hash(U) - v/4.  + .5;
}

#endif