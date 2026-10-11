# Cloud upscale runbook (2026)

Goal: upscale remaining ~3,900 PNGs on a rented RTX 4090 instead of the
local GTX 1660 SUPER (~2 files/min → ~30h locally).

## Why RunPod Community (recommended)

| Option (Oct 2026 prices) | Rate | Est. job cost |
|---|---|---|
| RunPod Community RTX 4090 | ~$0.34/hr, per-second, **zero egress fees** | **~$1–2** (1–3h) |
| Vast.ai RTX 4090 | ~$0.2–0.5/hr + storage/egress extras | ~$1–3 + fees |
| Modal serverless | $30/mo free credit, T4 $0.59/hr equiv | free but 3–5x slower |
| Replicate per-image API | ~$0.01–0.05/image × 3,900 | $40–200 (avoid) |

## Steps (RunPod)

1. Create account at runpod.io, add ~$5 credit. Deploy a **Community
   RTX 4090** pod from the **PyTorch** template with ~50GB disk.
2. Upload (from home PC):
   ```
   runpodctl scp /tmp/opencode/supertux-images.tar <pod-id>:/workspace/
   runpodctl scp tools/cloud/setup-pod.sh tools/cloud/cloud_upscale.py <pod-id>:/workspace/
   ```
   (or use the pod's web terminal + `curl` if you host the tarball.)
3. On the pod:
   ```
   cd /workspace && tar -xf supertux-images.tar
   bash setup-pod.sh
   python3 -u cloud_upscale.py --input images --output data-hd/images --exclude-subset
   tar -cf data-hd.tar data-hd
   ```
4. Download `data-hd.tar` to home PC, extract over `~/supertux-hd/data-hd/`,
   restart the game. The engine picks up HD textures via
   `data-hd/images/UPSCALE_LOG.txt` (no code changes needed).

## Notes

- Same adaptive policy as local: 4x small / 2x large, 3840px cap,
  UltraSharp (bg) / anime_6B (sprites), alpha preserved, manifest merged.
- Pod-side output estimate: 5–15GB (bundle with `tar`, single download).
- Keep the local Tux/world-1 subset running; cloud `--exclude-subset`
  skips anything already logged once you copy the local manifest up.
