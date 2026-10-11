#!/usr/bin/env python3
"""Normalize Real-ESRGAN weight files to the {'params': state_dict} layout.

RealESRGANer (realesrgan 0.3.0) only understands top-level 'params_ema' /
'params' keys and strict-loads them. Community weights (e.g. UltraSharp)
ship bare state dicts or other wrappers, so convert once at image build
time instead of patching the library at runtime.

Usage: python normalize_weights.py /weights/*.pth
"""
import sys

import torch


def main(paths):
    for raw in paths:
        data = torch.load(raw, map_location="cpu", weights_only=True)
        if isinstance(data, dict):
            for key in ("params_ema", "params-ema", "params",
                        "state_dict", "model", "net_g"):
                if key in data and isinstance(data[key], dict):
                    data = data[key]
                    break
        torch.save({"params": data}, raw)
        print(f"normalized {raw}: {len(data)} tensors", flush=True)


if __name__ == "__main__":
    main(sys.argv[1:])
