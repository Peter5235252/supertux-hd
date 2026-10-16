# AI Use Disclosure

Prepared October 2026 against Valve's Steam AI-content rules as rewritten
**January 16–17, 2026** (pre-generated vs live-generated tiers; internal
dev-tools explicitly exempt) and the U.S. Copyright Office's January 2025
guidance on AI-assisted authorship. This file exists so players,
distributors, and reviewers know exactly where AI was involved.

## 1. Player-facing AI content (NONE — nothing to disclose)

- **This fork ships no AI-generated or AI-modified game content.** All
  artwork, music, sound, levels, and text are the original SuperTux
  assets, unmodified.
- **Removed:** an earlier version of this fork shipped ~3,900 Real-ESRGAN
  upscaled textures (`data-hd/`). That set and its pipeline have been
  **deleted** — it produced visual glitches (opaque rectangles over
  transparent backgrounds, mis-scaled sprites) that could not be reliably
  fixed. No AI-upscaled data ships in any current build or release.
- Consequently there is nothing to declare under Steam's
  "Pre-Generated" category, and nothing in "Live-Generated" either.

## 2. Development workflow (AI-written, disclosed)

- **A majority of this remaster was developed by AI agents running inside
  [OpenCode](https://opencode.ai), under the direct supervision, review,
  and testing of the repository owner (Peter5235252).** This includes the
  SDL_GPU renderer, the native Metal / Direct3D 12 / Vulkan backends, the
  per-backend shader pipeline, the CI workflows, and the (now-removed) HD
  content pipeline.
- AI coding assistants are **explicitly exempt** from Steam disclosure, but
  this project states it plainly anyway. Every change was compiled and run
  before being committed; the owner directed the work, made the design
  decisions, and verified behavior on real hardware (CachyOS desktop and a
  macOS Hackintosh).

## 3. Notes for re-distributors

- Nothing needs to be declared under AI content categories: the shipped
  game data is the original, human-made SuperTux content.
- Code is GPL-3.0-or-later; game data is CC-BY-SA with the per-file
  exceptions in `data/AUTHORS`. See [ATTRIBUTION.md](ATTRIBUTION.md).
