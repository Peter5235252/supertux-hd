#!/usr/bin/env python3
"""HD upscale pipeline for SuperTux fork.
- Adaptive scale: 4x for small sprites (<800px long edge), 2x for large (>=800px), cap long edge at 3840.
- Model pick: *-anime for cartoon sprites/tiles, x4plus for organic backgrounds.
- Alpha-safe: Real-ESRGAN ncnn preserves RGBA (verified in pilot); script verifies after each file.
- Tiling: -t 256 fits 6GB GTX 1660 SUPER (verified).
Usage: python3 tools/hd_upscale.py --upscaler /tmp/opencode/upscaler/realesrgan-ncnn-vulkan --input data/images --output data-hd/images
"""
import argparse, subprocess, shutil
from pathlib import Path
from PIL import Image

def pick_model_scale(src: Path):
    with Image.open(src) as im:
        w, h = im.size
    long_edge = max(w, h)
    if long_edge >= 1600:
        scale = 2  # 1280x720 -> 2560x1440, avoid 5120x2880 waste
    else:
        scale = 4
    # cartoon vs organic heuristic: background/ -> x4plus, else anime
    if "background" in src.parts:
        model = "realesrgan-x4plus"
    else:
        model = "realesrgan-x4plus-anime"
    # cap: if w*scale > 3840, drop to 2x or 1x
    while max(w, h) * scale > 3840 and scale > 1:
        scale //= 2
    return model, scale

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--upscaler", required=True)
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--tile", type=int, default=256)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--include", action="append", default=[],
                    help="only process files whose path contains this substring (repeatable)")
    args = ap.parse_args()
    src_root, dst_root = Path(args.input), Path(args.output)
    pngs = sorted(src_root.rglob("*.png"))
    # skip deprecated
    pngs = [p for p in pngs if "deprecated" not in p.name]
    if args.include:
        pngs = [p for p in pngs if any(s in str(p) for s in args.include)]
    if args.limit:
        pngs = pngs[:args.limit]
    print(f"queue: {len(pngs)} files", flush=True)
    # Merge with any existing manifest so interrupted/resumed runs keep entries.
    manifest_path = dst_root / "UPSCALE_LOG.txt"
    merged = {}
    if manifest_path.exists():
        for line in manifest_path.read_text().splitlines():
            parts = line.split()
            if len(parts) >= 3:
                merged[parts[0]] = line
    log = []
    for src in pngs:
        rel = src.relative_to(src_root)
        dst = dst_root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.exists():
            continue
        model, scale = pick_model_scale(src)
        cmd = [args.upscaler, "-i", str(src), "-o", str(dst),
               "-n", model, "-s", str(scale), "-t", str(args.tile), "-f", "png"]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"FAIL {rel}: {r.stderr[-500:]}")
            continue
        # verify alpha preserved + size
        with Image.open(src) as a, Image.open(dst) as b:
            assert b.size == (a.size[0]*scale, a.size[1]*scale), f"size mismatch {rel}"
            if a.mode == "RGBA" and b.mode != "RGBA":
                print(f"WARN alpha lost {rel}")
        log.append(f"{rel} {model} x{scale}")
        merged[str(rel)] = f"{rel} {model} x{scale}"
        print(f"OK {rel} {model} x{scale}", flush=True)
    manifest_path.write_text("\n".join(sorted(merged.values())))
    print(f"done: {len(log)} new files, {len(merged)} total -> {dst_root}", flush=True)

if __name__ == "__main__":
    main()
