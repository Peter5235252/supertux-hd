# AI Use Disclosure

Prepared October 2026 against Valve's Steam AI-content rules as rewritten
**January 16–17, 2026** (pre-generated vs live-generated tiers; internal
dev-tools explicitly exempt) and the U.S. Copyright Office's January 2025
guidance on AI-assisted authorship. This file exists so players,
distributors, and reviewers know exactly where AI was involved.

## 1. Player-facing AI content (Steam "Pre-Generated" — DISCLOSED)

- **What:** ~3,900 game textures AI-upscaled to up to 4K (`data-hd/`,
  logged per-file in `data-hd/images/UPSCALE_LOG.txt` with model + scale).
- **Tools (all offline, pre-ship, no runtime generation):**
  - `Real-ESRGAN_x4plus` (Tencent ARC Lab, BSD-3) — organic/backgrounds.
  - `RealESRGAN_x4plus_anime_6B` (Tencent ARC Lab, BSD-3) — cartoon sprites.
  - `4x-UltraSharp` (Kim2091, community fine-tune — check its license
    before commercial use) — sharpness pass for backgrounds.
  - Local path: `realesrgan-ncnn-vulkan`; cloud path: PyTorch
    Real-ESRGAN (`tools/cloud/`).
- **Human direction:** curated per-category model selection, adaptive
  scale policy (4x small / 2x large, 3840px cap), alpha-channel
  preservation, tiled-seam review, in-engine QA. No new art is invented
  beyond detail reconstruction; layout, palette, and design are the
  original artists'. Per the Jan 2025 Copyright Office guidance, these
  are human-directed derivative transforms, and they inherit the
  upstream data license (CC-BY-SA, ShareAlike).
- **What is NOT used:** no live/in-match generation, no AI music,
  dialogue, voice, or marketing assets. There is nothing to add guardrails
  to — the "Live-Generated" Steam category does not apply.

## 2. Development workflow (Steam-exempt, disclosed here voluntarily)

- Since the January 2026 rewrite, AI coding assistants and other
  behind-the-scenes efficiency tools are **explicitly exempt** from
  Steam disclosure. This project goes further and states it anyway:
- **100% of the fork's new code was written with Muse Spark 1.3,
  running inside OpenCode, under the direct supervision, review, and
  testing of the repository owner** (Peter5235252). Every change was
  compiled, run, screenshot-verified, and playtested locally before
  being committed — including the mirrored-sprite scale fix, which was
  diagnosed from a player-visible bug report with in-engine debug logs.

## 3. Notes for re-distributors

- If you ship this fork on Steam, declare section 1 under
  **Pre-Generated AI content** in the Content Survey; check nothing
  under Live-Generated.
- Keep `data-hd/images/UPSCALE_LOG.txt` with any release — it is the
  per-asset audit trail (source file → model → scale).
- Community model licenses (UltraSharp et al.) must be re-checked
  before any commercial use; prefer BSD-3 `x4plus`/`anime_6B` if unsure.
