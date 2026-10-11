"""Modal serverless batch upscale for the SuperTux HD fork.

Runs the same Real-ESRGAN job as tools/cloud/cloud_upscale.py on Modal GPUs
(T4 $0.59/hr, A10G ~$1.10/hr, covered by the $30/mo free credit).

One-time setup (needs your Modal account, interactive):
    modal setup

Upload source + seed:
    modal volume put supertux-data /tmp/opencode/supertux-images.tar images.tar

Run the full batch (~3.9k files):
    modal run tools/cloud/modal_upscale.py

Pack + download results:
    modal run tools/cloud/modal_upscale.py::pack_results
    modal volume get supertux-data data-hd.tar /tmp/opencode/data-hd.tar
    tar -xf /tmp/opencode/data-hd.tar -C ~/supertux-hd
"""
import importlib.util
from pathlib import Path

import modal

app = modal.App("supertux-hd-upscale")
vol = modal.Volume.from_name("supertux-data", create_if_missing=True)

image = (
    modal.Image.debian_slim(python_version="3.11")
    .apt_install("libgomp1", "curl")
    # Layer 1: wheels only. torch + facexlib install cleanly together.
    .pip_install("torch", "torchvision", "numpy", "pillow",
                 "opencv-python-headless", "tqdm", "yapf", "facexlib")
    # Layer 2: basicsr/gfpgan/realesrgan with --no-build-isolation.
    # basicsr's setup.py egg-fetches cuda-toolkit (nvidia-cublas==13.1.*),
    # which conflicts with torch's bundled nvidia-cublas 13.8. Skipping
    # build isolation avoids the egg fetch; runtime imports only need
    # torch, which layer 1 already provides.
    .run_commands(
        "python -m pip install --no-build-isolation basicsr gfpgan realesrgan"
    )
    .run_commands(
        "mkdir -p /weights && cd /weights && "
        "curl -fL -o RealESRGAN_x4plus.pth "
        "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth && "
        "curl -fL -o RealESRGAN_x4plus_anime_6B.pth "
        "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.2.4/RealESRGAN_x4plus_anime_6B.pth && "
        "(curl -fL -o 4x-UltraSharp.pth "
        "https://huggingface.co/uwg/upscaler/resolve/main/ESRGAN/4x-UltraSharp.pth "
        "|| echo NO_ULTRASHARP_MIRROR)"
    )
    .add_local_file("tools/cloud/cloud_upscale.py", "/opt/cloud_upscale.py")
)

BATCH = 80


def _loader():
    spec = importlib.util.spec_from_file_location("cu", "/opt/cloud_upscale.py")
    cu = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cu)
    return cu


@app.function(image=image, volumes={"/data": vol}, timeout=600)
def seed():
    """Extract images.tar into the volume once (idempotent)."""
    import tarfile

    v = modal.Volume.from_name("supertux-data")
    if (Path("/data/images")).exists():
        return "images already seeded"
    with tarfile.open("/data/images.tar") as t:
        t.extractall("/data")
    v.commit()
    n = sum(1 for _ in Path("/data/images").rglob("*.png"))
    return f"seeded {n} pngs"


@app.function(image=image, volumes={"/data": vol}, timeout=600)
def read_manifest() -> list:
    p = Path("/data/data-hd/images/UPSCALE_LOG.txt")
    if not p.exists():
        return []
    return p.read_text().splitlines()


@app.function(image=image, gpu="A10G", volumes={"/data": vol}, timeout=2700)
def upscale_batch(files: list) -> list:
    cu = _loader()
    v = modal.Volume.from_name("supertux-data")
    src_root, dst_root = Path("/data/images"), Path("/data/data-hd/images")
    cache = {}
    lines = []
    for rel in files:
        src, dst = src_root / rel, dst_root / rel
        model, scale = cu.pick_model_scale(src, Path("/weights"))
        key = (model, scale)
        if key not in cache:
            print(f"loading {Path(model).name} x{scale}", flush=True)
            cache[key] = cu.build_upsampler(model)
        try:
            cu.upscale_rgb_alpha(cache[key], scale, src, dst)
        except RuntimeError as e:
            if "out of memory" in str(e).lower():
                import torch

                torch.cuda.empty_cache()
                print(f"OOM on {rel}, retrying at tile 256", flush=True)
                cache[key] = cu.build_upsampler(model, scale, tile=256)
                cu.upscale_rgb_alpha(cache[key], scale, src, dst)
            else:
                raise
        lines.append(f"{rel} {Path(model).stem} x{scale}")
    v.commit()
    return lines


@app.function(image=image, volumes={"/data": vol}, timeout=600)
def write_manifest(lines: list) -> str:
    v = modal.Volume.from_name("supertux-data")
    p = Path("/data/data-hd/images/UPSCALE_LOG.txt")
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text("\n".join(sorted(lines)))
    v.commit()
    return f"manifest: {len(lines)} entries"


@app.function(image=image, volumes={"/data": vol}, timeout=1200)
def pack_results() -> str:
    import tarfile

    v = modal.Volume.from_name("supertux-data")
    with tarfile.open("/data/data-hd.tar", "w") as t:
        t.add("/data/data-hd", arcname="data-hd")
    v.commit()
    return "packed /data/data-hd.tar"


@app.local_entrypoint()
def main():
    print(seed.remote())
    done = set()
    for line in read_manifest.remote():
        parts = line.split()
        if len(parts) >= 3:
            done.add(parts[0])
    root = Path("data/images")
    files = sorted(
        str(p.relative_to(root))
        for p in root.rglob("*.png")
        if "deprecated" not in p.name and str(p.relative_to(root)) not in done
    )
    print(f"queue: {len(files)} files ({len(done)} already done)")
    batches = [files[i:i + BATCH] for i in range(0, len(files), BATCH)]
    all_lines = list(read_manifest.remote())
    for out in upscale_batch.map(batches):
        all_lines.extend(out)
        print(f"progress: {len(all_lines)} total", flush=True)
    print(write_manifest.remote(sorted(set(all_lines))))
