#ifndef BUILT_IN_FRAME_GLOBAL_DATA_GLSL
#define BUILT_IN_FRAME_GLOBAL_DATA_GLSL

#define FG_BLOCK_LAYOUT FG { vec4 time; vec4 mousePos;}

#define USE_FRAME_GLOBAL_DATA layout(set = 0, binding = 0) uniform FG_BLOCK_LAYOUT FGData

#endif