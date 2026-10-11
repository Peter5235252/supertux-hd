#!/usr/bin/env python3
"""Cloud batch upscaler (PyTorch Real-ESRGAN) - mirrors tools/hd_upscale.py.

Same contract, GPU-pod edition:
- Adaptive scale: 4x for small assets (<800px long edge), 2x for large,
  long edge capped at 3840px.
- Model pick: UltraSharp (fallback x4plus) for organic/backgrounds,
  official anime_6B for cartoon sprites/tiles.
- Alpha-safe: RGB upscaled by GAN, alpha channel Lanczos-resized, merged.
- Manifest: merges data-hd/images/UPSCALE_LOG.txt (same format), resume-safe.

Usage on pod:
  python3 cloud_upscale.py --input images --output data-hd/images [--include substr ...]
"""
import argparse
import os
from pathlib import Path

import cv2
import numpy as np
import torch

# Compatibility shim (Oct 2026): basicsr 1.4.2 does
# `from torchvision.transforms.functional_tensor import rgb_to_grayscale`,
# but that module was removed in new torchvision. Alias it to the same
# function in torchvision.transforms.functional before realesrgan/basicsr
# get imported.
import sys
import types

try:
    import torchvision.transforms.functional_tensor  # noqa: F401
except ImportError:
    from torchvision.transforms.functional import rgb_to_grayscale as _rtg

    _shim = types.ModuleType("torchvision.transforms.functional_tensor")
    _shim.rgb_to_grayscale = _rtg
    sys.modules["torchvision.transforms.functional_tensor"] = _shim

from PIL import Image
from realesrgan import RealESRGANer


def _load_archs():
    """Import arch classes, tolerating realesrgan>=0.3 which moved them."""
    try:
        from realesrgan.archs.srvgg_arch import SRVGGNetCompact
        from realesrgan.archs.rrdbnet_arch import RRDBNet
    except ImportError:
        from basicsr.archs.srvgg_arch import SRVGGNetCompact
        from basicsr.archs.rrdbnet_arch import RRDBNet
    return SRVGGNetCompact, RRDBNet


def build_upsampler(model_path: str, scale: int = 4, tile: int = 512):
    SRVGGNetCompact, RRDBNet = _load_archs()
    if "anime" in os.path.basename(model_path):
        model = SRVGGNetCompact(num_in_ch=3, num_out_ch=3, num_feat=64,
                                num_conv=16, upscale=4, act_type='prelu')
    else:
        model = RRDBNet(num_in_ch=3, num_out_ch=3, num_feat=64,
                        num_block=23, num_grow_ch=32, scale=4)
    return RealESRGANer(scale=scale, model_path=model_path, model=model,
                        tile=tile, tile_pad=16, pre_pad=0,
                        half=True, gpu_id=0)


def pick_model_scale(src: Path, wdir: Path):
    with Image.open(src) as im:
        w, h = im.size
    scale = 2 if max(w, h) >= 1600 else 4
    while max(w, h) * scale > 3840 and scale > 1:
        scale //= 2
    if "background" in src.parts:
        cand = wdir / "4x-UltraSharp.pth"
        model = str(cand if cand.exists() else wdir / "RealESRGAN_x4plus.pth")
    else:
        model = str(wdir / "RealESRGAN_x4plus_anime_6B.pth")
    return model, scale


def upscale_rgb_alpha(upsampler, outscale, src: Path, dst: Path):
    with Image.open(src) as im:
        has_alpha = im.mode == "RGBA"
        rgb = im.convert("RGB")
        alpha = im.split()[3] if has_alpha else None
    img = cv2.cvtColor(np.array(rgb), cv2.COLOR_RGB2BGR)
    with torch.no_grad():
        out, _ = upsampler.enhance(img, outscale=outscale)
    out_rgb = Image.fromarray(cv2.cvtColor(out, cv2.COLOR_BGR2RGB))
    if has_alpha:
        a = alpha.resize(out_rgb.size, Image.LANCZOS)
        out_rgb.putalpha(a)
    dst.parent.mkdir(parents=True, exist_ok=True)
    out_rgb.save(dst)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--weights", default="weights")
    ap.add_argument("--include", action="append", default=[])
    ap.add_argument("--exclude-subset", action="store_true",
                    help="skip files already listed in the output manifest")
    args = ap.parse_args()

    src_root, dst_root = Path(args.input), Path(args.output)
    wdir = Path(args.weights)
    pngs = sorted(src_root.rglob("*.png"))
    pngs = [p for p in pngs if "deprecated" not in p.name]
    if args.include:
        pngs = [p for p in pngs if any(s in str(p) for s in args.include)]

    manifest = dst_root / "UPSCALE_LOG.txt"
    merged = {}
    if manifest.exists():
        for line in manifest.read_text().splitlines():
            parts = line.split()
            if len(parts) >= 3:
                merged[parts[0]] = line
    if args.exclude_subset:
        pngs = [p for p in pngs
                if str(p.relative_to(src_root)) not in merged
                and not (dst_root / p.relative_to(src_root)).exists()]
    else:
        pngs = [p for p in pngs
                if not (dst_root / p.relative_to(src_root)).exists()
                or str(p.relative_to(src_root)) not in merged]

    print(f"queue: {len(pngs)} files", flush=True)
    cache = {}
    done = 0
    for src in pngs:
        rel = src.relative_to(src_root)
        dst = dst_root / rel
        model_path, scale = pick_model_scale(src, wdir)
        key = (model_path, scale)
        if key not in cache:
            print(f"loading {Path(model_path).name} (outscale x{scale})", flush=True)
            cache[key] = build_upsampler(model_path)
        try:
            upscale_rgb_alpha(cache[key], scale, src, dst)
        except RuntimeError as e:
            if "out of memory" in str(e).lower():
                torch.cuda.empty_cache()
                print(f"OOM on {rel}, retrying without fp16 tiling change", flush=True)
                upscale_rgb_alpha(cache[key], scale, src, dst)
            else:
                raise
        merged[str(rel)] = f"{rel} {Path(model_path).stem} x{scale}"
        done += 1
        print(f"OK {rel} {Path(model_path).name} x{scale} [{done}/{len(pngs)}]", flush=True)
        if done % 50 == 0:
            manifest.write_text("\n".join(sorted(merged.values())))
    manifest.write_text("\n".join(sorted(merged.values())))
    print(f"done: {done} new files, {len(merged)} total -> {dst_root}", flush=True)


if __name__ == "__main__":
    main()
