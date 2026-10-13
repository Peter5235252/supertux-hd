# Attribution

This project (**supertux2-hd**) is a fan-made graphical-enhancement fork of
**SuperTux** (modern `supertux2` engine generation, colloquially "SuperTux 2"). All game design, artwork, music, levels, and code remain the
work of their original creators. Nothing here claims their authorship.

## The original game

- **SuperTux** — open-source 2D jump'n'run sidescroller (Mario-style),
  © 2003–2026 The SuperTux Team.
- Original authors: **Bill Kendrick** (code) and **Ingo Ruhnke** (art).
- Upstream source: <https://github.com/SuperTux/supertux> (3.2k stars,
  600+ forks as of October 2026). Stable release **0.7.0** (March 2026).
- The modern engine generation (0.5.0+) ships as the `supertux2` binary,
  colloquially "SuperTux 2" — same project, not a separate game.
- Website: <https://www.supertux.org>
- This fork is based on upstream commit `b562c6c` (2026-10-10).

## Full contributor credits

The complete contributor list ships with the game and is shown in-game
under *Contribute → Credits*. Source of truth, preserved unmodified:

- [`data/credits.stxt`](data/credits.stxt)

## License split (unchanged from upstream)

- **Code** (`src/`, tools where noted): **GNU GPL-3.0-or-later** —
  see [`LICENSE.txt`](LICENSE.txt), shipped byte-identical to upstream.
- **Game data** (most of `data/`): **CC-BY-SA**, with per-file
  exceptions documented upstream (e.g. `data/AUTHORS`,
  `data/credits.stxt`). AI-upscaled derivatives in `data-hd/` inherit
  the same ShareAlike terms as the art they derive from.
- New fork code in this repo is likewise **GPL-3.0-or-later**; new
  fork docs are CC-BY-SA-4.0 unless noted.

## Third-party code in this fork

- `data/shader/ffx_a.h`, `data/shader/ffx_fsr1.h`: AMD FidelityFX
  Super Resolution 1.0 framework headers, © 2021 Advanced Micro Devices,
  Inc., **MIT license** (see `GPUOpen-Effects/FidelityFX-FSR/license.txt`
  upstream). Only the EASU/RCAS shader functions are used.
- Real-ESRGAN weights remain property of their authors (see
  [`AI_DISCLOSURE.md`](AI_DISCLOSURE.md) for per-model licenses).

AI-upscaled 4K texture set (same world size, higher texel density),
HD-aware engine scaling, HDR groundwork, and cloud batch tooling.
Details: [`CHANGELOG-FORK.md`](CHANGELOG-FORK.md).
Planned: complete rendering-pipeline rewrite from OpenGL to Vulkan.
AI involvement is disclosed in [`AI_DISCLOSURE.md`](AI_DISCLOSURE.md).
