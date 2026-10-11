// AMD FidelityFX Super Resolution 1.0 - RCAS sharpen pass (SDL_GPU).
// Copyright (c) 2021 Advanced Micro Devices, Inc. (MIT, see ATTRIBUTION.md).
// sharpnessStops: 0.0 = maximum, larger = less (FSR convention).
#version 450

#define A_GPU 1
#define A_GLSL 1
#define FSR_RCAS_F 1
#include "ffx_a.h"
#include "ffx_fsr1.h"

layout(set = 2, binding = 0) uniform sampler2D srcTex;

layout(set = 3, binding = 0) uniform RcasUBO {
  float sharpnessStops;
};

AF4 FsrRcasLoadF(ASU2 p) { return texelFetch(srcTex, p, 0); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}

layout(location = 0) out vec4 outColor;

void main()
{
  AU4 con;
  FsrRcasCon(con, sharpnessStops);
  AU2 ip = AU2(uint(gl_FragCoord.x), uint(gl_FragCoord.y));
  AF1 r, g, b;
  FsrRcasF(r, g, b, ip, con);
  outColor = vec4(r, g, b, 1.0);
}
