#!/usr/bin/env python3
"""Normalize Real-ESRGAN weight files to the {'params': state_dict} layout.

RealESRGANer (realesrgan 0.3.0) only understands top-level 'params_ema' /
'params' keys and strict-loads them. Community weights ship other layouts,
so convert once at image build time instead of patching the library:

- bare state dicts and {'state_dict'/'model'/'net_g'} wrappers are
  re-wrapped as {'params': ...};
- original-ESRGAN key layouts (e.g. 4x-UltraSharp: 'model.1.sub.0.RDB1…')
  are translated to RealESRGAN/RRDBNet keys ('body.0.rdb1…').

Usage: python normalize_weights.py /weights/*.pth
"""
import re
import sys

import torch

_ESRGAN_MAP = (
    (re.compile(r"model\.0\.(.*)"), r"conv_first.\1"),
    (re.compile(r"model\.1\.sub\.(\d+)\.RDB(\d)\.conv(\d)\.0\.(.*)"),
     r"body.\1.rdb\2.conv\3.\4"),
    (re.compile(r"model\.2\.(.*)"), r"conv_body.\1"),
    (re.compile(r"model\.3\.(.*)"), r"conv_up1.\1"),
    (re.compile(r"model\.6\.(.*)"), r"conv_up2.\1"),
    (re.compile(r"model\.8\.(.*)"), r"conv_hr.\1"),
    (re.compile(r"model\.10\.(.*)"), r"conv_last.\1"),
)


def _translate_esrgan_keys(state):
    out = {}
    for key, val in state.items():
        new_key = key
        for pattern, repl in _ESRGAN_MAP:
            m = pattern.fullmatch(key)
            if m:
                new_key = m.expand(repl)
                break
        out[new_key] = val
    return out


def main(paths):
    for raw in paths:
        data = torch.load(raw, map_location="cpu", weights_only=True)
        if isinstance(data, dict):
            for key in ("params_ema", "params-ema", "params",
                        "state_dict", "model", "net_g"):
                if key in data and isinstance(data[key], dict):
                    data = data[key]
                    break
        if any(k.startswith("model.") for k in data):
            data = _translate_esrgan_keys(data)
            print(f"{raw}: translated ESRGAN keys", flush=True)
        torch.save({"params": data}, raw)
        print(f"normalized {raw}: {len(data)} tensors", flush=True)


if __name__ == "__main__":
    main(sys.argv[1:])
