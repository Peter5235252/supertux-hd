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
  // HDR output transform. hdrEnabled = 0 keeps the exact legacy SDR
  // behavior; 1 PQ-encodes into an HDR10 (BT.2020 container) swapchain.
  // hdrHeadroom scales SDR white (1.0) to nits: nits = linear*100*headroom.
  float hdrEnabled;
  float hdrHeadroom;
  float _pad;
};

AF4 FsrRcasLoadF(ASU2 p) { return texelFetch(srcTex, p, 0); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}

// ITU-R BT.2084 (PQ) opto-electrical transfer, input normalized to 10k nits.
float PqOetf(float n)
{
  const float m1 = 0.1593017578125;
  const float m2 = 78.84375;
  const float c1 = 0.8359375;
  const float c2 = 18.8515625;
  const float c3 = 18.6875;
  float y = pow(n, m1);
  return pow((c1 + c2 * y) / (1.0 + c3 * y), m2);
}

vec3 SrgbToLinear(vec3 c)
{
  // Exact sRGB EOTF^-1 (piecewise); FSR works in display-referred space.
  bvec3 lo = lessThanEqual(c, vec3(0.04045));
  vec3 lin_lo = c / 12.92;
  vec3 lin_hi = pow((c + 0.055) / 1.055, vec3(2.4));
  return mix(lin_hi, lin_lo, lo);
}

// sRGB primaries mapped into the BT.2020 container (D65, BT.2087).
// NOTE: mat3() takes columns; the row-major matrix is
// [0.6274 0.3293 0.0433 / 0.0691 0.9195 0.0114 / 0.0164 0.0880 0.8956]
// (rows sum to 1 so white is preserved). The conversion MUST run in
// linear space, before the PQ encode: PQ is highly nonlinear and a
// matrix applied after it shifts hues (notably green).
const mat3 SRGB_TO_2020 = mat3(
  0.6274, 0.0691, 0.0164,
  0.3293, 0.9195, 0.0880,
  0.0433, 0.0114, 0.8956);

layout(location = 0) out vec4 outColor;

void main()
{
  AU4 con;
  FsrRcasCon(con, sharpnessStops);
  AU2 ip = AU2(uint(gl_FragCoord.x), uint(gl_FragCoord.y));
  AF1 r, g, b;
  FsrRcasF(r, g, b, ip, con);
  outColor = vec4(r, g, b, 1.0);

  if (hdrEnabled > 0.5)
  {
    // RCAS overshoot can exceed [0,1]; clamp before the PQ encode.
    // Gamut mapping first (linear), PQ per channel after.
    vec3 lin = SrgbToLinear(clamp(vec3(r, g, b), vec3(0.0), vec3(1.0)));
    vec3 lin2020 = SRGB_TO_2020 * lin;
    vec3 norm = clamp(lin2020 * (100.0 * hdrHeadroom) / 10000.0, vec3(0.0), vec3(1.0));
    vec3 pq = vec3(PqOetf(norm.r), PqOetf(norm.g), PqOetf(norm.b));
    outColor = vec4(pq, 1.0);
  }
}
