#!/usr/bin/env bash
# Generate Metal (MSL) and DirectX (DXIL) shaders from the committed SPIR-V.
#
# The .spv files stay the byte-identical Vulkan path on Linux (and the
# Windows Vulkan fallback). MSL feeds SDL's native Metal backend, DXIL
# feeds native Direct3D 12. Conventions follow SDL_CreateGPUShader docs:
# fragment resources in HLSL spaces 2 (textures/samplers) + 3 (uniforms),
# MSL argument tables [[texture(n)]]/[[sampler(n)]]/[[buffer(n)]], entry
# "main0" for Metal and "main" for Direct3D.
#
# Requirements (CachyOS/Arch): spirv-cross, and Microsoft's official dxc
# (the repo directx-shader-compiler build crashes on some CPUs; use the
# Linux tarball from microsoft/DirectXShaderCompiler releases):
#   DXC=/path/to/dxc-ms/bin/dxc LD_LIBRARY_PATH=/path/to/dxc-ms/lib \
#     bash tools/build_shaders.sh
set -euo pipefail

DXC="${DXC:-dxc}"

cd "$(dirname "$0")/../data/shader"

for spv in gpu_textured.vert.spv gpu_fullscreen.vert.spv \
           gpu_textured.frag.spv gpu_fsr_easu.frag.spv gpu_fsr_rcas.frag.spv; do
  base="${spv%.spv}"
  if [[ "$base" == *.vert ]]; then stage=vert; profile=vs; else stage=frag; profile=ps; fi

  spirv-cross --msl --msl-version 20000 \
    --rename-entry-point main main0 "$stage" \
    "$spv" --output "$base.msl"

  spirv-cross --hlsl --shader-model 60 \
    "$spv" --output "$base.hlsl"

  "$DXC" -T "${profile}_6_0" -E main "$base.hlsl" -Fo "$base.dxil"
  rm -f "$base.hlsl"

  echo "== $base: $(wc -c < "$base.msl") B MSL, $(wc -c < "$base.dxil") B DXIL"
done

echo done.
