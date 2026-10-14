# Fork Changelog

## 2026-10-14 — Native backends everywhere, translation layers deleted

- SDL_GPU now drives the native API per OS: Metal on macOS, Direct3D 12
  on Windows 10/11 (Vulkan fallback if D3D12 creation fails), Vulkan on
  Linux. Note: SDL_GPU has no Direct3D 11 backend, so the requested D3D11
  fallback was replaced with a Vulkan fallback on Windows.
- Shaders ship per backend: SPIR-V (Vulkan), DXIL (D3D12), MSL (Metal),
  all generated from the committed SPIR-V by `tools/build_shaders.sh`
  (spirv-cross + Microsoft dxc). The loader picks the format from the
  live device caps; entries are `main` (SPIR-V/DXIL) and `main0` (MSL).
- Deleted the macOS Vulkan apparatus: ICD selection, bundled loader +
  MoltenVK/KosmicKrisp, SDK download steps, driver-override variable.
  The settings menu now names the active native backend (Metal /
  DirectX 12 / Vulkan) instead of implying translation layers.
- CI: mac smoke asserts `SDL_GPU driver: metal` on 15 + 26; the
  driver-selection test dimension is gone. D3D12 has no runtime test
  hardware yet — it compiles in CI but first real render is unverified.

## 2026-10-14 — KosmicKrisp preferred on modern Apple Silicon

- macOS settings now name the real driver: the video-system menu shows
  KosmicKrisp or MoltenVK (whichever ICD was selected at startup) with a
  note that it is a Vulkan-to-Metal translation layer, instead of implying
  native Vulkan. Display-only; the stored setting is unchanged on all OSes.

- macOS ICD policy changed: KosmicKrisp is now the automatic choice on
  Apple Silicon + macOS 26+, MoltenVK is the fallback everywhere else and
  is forced on Intel Macs. `SUPERTUX_MAC_VULKAN_DRIVER` still overrides
  (`moltenvk` / `kosmickrisp` / `auto`); explicit selections fail loudly
  instead of falling back silently.
- CI updated for the new default: macOS 26 `auto` runs the strict
  KosmicKrisp-unavailable guard, macOS 26 `moltenvk` runs the full HD/FSR
  suite, macOS 15 `auto` still covers MoltenVK end to end.
- Note: KosmicKrisp still fails GPU enumeration on CI runners, so the new
  default is unverified in automated tests pending real hardware results.

## 2026-10-13 — macOS HD packaging and Vulkan validation

- Published a checksum-pinned HD asset release containing 3,867 textures,
  original-size scale metadata, per-file hashes, creator credits and AI-use
  disclosure. macOS CI installs it beside the original data inside the app.
- Restored original alpha in 18 staged images whose HD alpha had become
  fully opaque, including `transparent_up.png`; local HD files are unchanged.
- Replaced shared temporary ICD manifests with bundle-relative library paths.
  MoltenVK remains the default; KosmicKrisp is an explicit, strict opt-in
  while its CI GPU-enumeration failure is unresolved.
- Added exact-artifact tests on macOS 15/26: HD menu, HD cloud level, FSR
  and stock fallback, including relocation to a path with spaces.
- Fixed screenshot readback: render into an owned presentation texture
  and blit to the swapchain, instead of reading a presented swapchain image.
  Local Vulkan validation no longer reports the screenshot layout/usage errors.
- Autotest level launches skip the interactive intro so screenshots capture
  gameplay. The ThinkPad T480 now runs CachyOS, not macOS.
- Fixed stock fallback dimensions: HD-off, missing HD images and failed HD
  decodes must use scale 1 instead of the upscale manifest's scale.

All times Europe/Budapest (UTC+2), 2026-10-11. Upstream base `b562c6c`
(2026-10-10). Full hashes: `git log` on branch `hd-hdr-experiment`.

## 2026-10-11 — HD textures, HDR groundwork, cloud batch, legal docs

- **12:02–12:05 — Research & system check.** 2026-only review of AI
  texture-upscaling practice (Real-ESRGAN + 4x-UltraSharp/AnimeSharp via
  chaiNNer, tiling/seam and alpha-handling rules). Host: CachyOS,
  i5-8500, GTX 1660 SUPER 6GB. Cloned upstream (+submodules) to
  `~/supertux-hd`, branch `hd-hdr-experiment`. Asset audit: 4,018 PNGs
  (3,867 non-deprecated), 393 `.sprite` files, `data/` 323MB.
- **12:05–12:10 — Pilot upscale (local Vulkan).** `realesrgan-ncnn-vulkan`
  verified: 48px sprite → 192px (~3s), 720p background → 2880p (~12s),
  alpha preserved, `-t 256` fits 6GB VRAM. Finding: blind 4x overshoots
  4K → adaptive policy (4x <800px, 2x ≥800px, 3840px cap).
- **12:10–12:15 — Baseline build.** Installed `ninja physfs sdl3_image
  sdl3_ttf` via `pkexec`; `cmake` + `ninja -j6` clean (`supertux2` 193MB,
  boots to gameplay).
- **12:15–12:20 — HD engine support.** Key insight: `Sprite::get_width`
  returns raw pixels, so naive 4x PNGs render 4x massive. Added
  `g_config->hd_textures` (default on), `data-hd/` PhysFS overlay mounted
  at `hd/`, manifest-gated redirect (`UPSCALE_LOG.txt`), per-texture
  `Surface::m_hd_scale` keeping world size constant, logical→HD rect
  scaling for tilesets, trilinear mipmaps for ≥256px textures
  (`gl_texture.cpp`), 10-bit sRGB framebuffer request behind
  `g_config->use_hdr` (`gl_video_system.cpp`).
- **12:20–12:30 — Config & first playtest.** Fixed user-broken video
  settings (800x600/0.4x) → forced 1920x1080@144 fullscreen. Found
  viewport letterboxing cause: magnification 100% caps view at 1368x800
  (`viewport.cpp`); `magnification 0` (auto) fills screen.
- **12:30–12:50 — Fast-track subset.** Full 4,018-file batch too slow
  locally (~30h); upscaled Tux + tiles + Antarctic set first: **847
  files**, verified exact 4x dims + manifest keys. Player confirmed
  sharper rendering — but left-facing Tux giant.
- **12:50–12:58 — Mirrored-sprite fix.** Root cause: `mirror-action`
  (`*-left` frames) uses `Surface::clone()`, which dropped `m_hd_scale`;
  `region()` same flaw. Fixed scale propagation (+ logical→HD rect
  conversion), also covered `from_reader`. Player-verified fixed.
- **12:26–13:10 (parallel) — Cloud batch (Modal, A10G).** Researched
  Oct-2026 GPU pricing (RunPod $0.34/hr 4090, Modal $30 free credit,
  Replicate per-image rejected at $40–200). Built `tools/cloud/`
  (setup script, PyTorch batch, Modal app, runbook). Fixed along the way:
  basicsr/cuda-toolkit pip conflict, wrong weight URLs (verified via
  GitHub API), Modal card requirement, libGL, torchvision shim,
  RRDB-arch pairing, weight normalization. Remaining ~3,000 files run
  in cloud; results merge via manifest.
- **13:10 — Legal & docs.** Verified Oct-2026 status: code GPL-3.0
  (`LICENSE.txt` byte-identical), data CC-BY-SA, © 2003–2026 The
  SuperTux Team (upstream 0.7.0, Mar 2026). Added `ATTRIBUTION.md`,
  `AI_DISCLOSURE.md` (Steam Jan-2026 tiers), this changelog, README
  fork banner. Pushed to private
  `github.com/Peter5235252/supertux2-hd`, branch `hd-hdr-experiment`.

## Planned next

1. Full texture set ingest from cloud run; in-engine QA pass.
2. **macOS Vulkan bundling**: KosmicKrisp (Apple Silicon + macOS 26+)
   with MoltenVK fallback, both shipped as ICDs in the .app bundle and
   selected at startup (`src/supertux/mac_vulkan_select.cpp`,
   `mk/macos-bundle-vulkan.sh`). No Metal/DX code — Vulkan everywhere.
   Status (Oct 2026): implemented, untested (no Apple hardware in-house;
   macOS marked beta pending a community tester).
3. Performance/VRAM profiling of 4K set on low-end GPUs.
