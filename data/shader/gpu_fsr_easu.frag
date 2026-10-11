// AMD FidelityFX Super Resolution 1.0 - EASU upscale pass (SDL_GPU).
// Copyright (c) 2021 Advanced Micro Devices, Inc. (MIT, see ATTRIBUTION.md).
// Input: scene rendered at reduced resolution. Output: display resolution.
// EASU constants are derived in-shader from viewport sizes (no CPU math).
#version 450

#define A_GPU 1
#define A_GLSL 1
#define FSR_EASU_F 1
#include "ffx_a.h"
#include "ffx_fsr1.h"

layout(set = 2, binding = 0) uniform sampler2D srcTex;

layout(set = 3, binding = 0) uniform EasuUBO {
  vec2 inputViewport; // rendered region in pixels (== input texture size here)
  vec2 inputSize;     // input texture size in pixels
  vec2 outputSize;    // display size in pixels
};

AF4 FsrEasuRF(AF2 p) { return AF4(textureGather(srcTex, p, 0)); }
AF4 FsrEasuGF(AF2 p) { return AF4(textureGather(srcTex, p, 1)); }
AF4 FsrEasuBF(AF2 p) { return AF4(textureGather(srcTex, p, 2)); }

layout(location = 0) out vec4 outColor;

void main()
{
  AU4 con0, con1, con2, con3;
  FsrEasuCon(con0, con1, con2, con3,
             inputViewport.x, inputViewport.y,
             inputSize.x, inputSize.y,
             outputSize.x, outputSize.y);
  AU2 ip = AU2(uint(gl_FragCoord.x), uint(gl_FragCoord.y));
  AF3 c;
  FsrEasuF(c, ip, con0, con1, con2, con3);
  outColor = vec4(c, 1.0);
}
