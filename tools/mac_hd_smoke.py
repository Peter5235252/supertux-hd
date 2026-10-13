#!/usr/bin/env python3
"""Exercise bundled HD/stock assets using a specific Vulkan ICD (GPL-3.0-or-later)."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from hd_assets import png_info, verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--driver", choices=("auto", "moltenvk", "kosmickrisp"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    app = args.app.resolve()
    data = app / "Contents/Resources/share/supertux2"
    verify(data.parent / "data-hd", data)
    info = json.loads((app / "Contents/Resources/HD_BUILD_INFO.json").read_text())
    print(json.dumps(info, indent=2), flush=True)
    env = dict(os.environ, SUPERTUX_MAC_VULKAN_DRIVER=args.driver,
               VK_LOADER_DEBUG="all", SUPERTUX_AUTOTEST_FRAMES="180")
    # Launch the same wrapper used by Finder, not a binary with CI-only dyld settings.
    launcher = app / "Contents/MacOS/SuperTux"
    results = []
    for name, level, hd, fsr in (
        ("menu-hd", None, True, 0),
        ("clouds-hd", "levels/world1/welcome_antarctica.stl", True, 0),
        ("clouds-hd-fsr", "levels/world1/welcome_antarctica.stl", True, 1),
        ("clouds-stock", "levels/world1/welcome_antarctica.stl", False, 0),
    ):
        output = (args.output / name).resolve()
        output.mkdir(parents=True, exist_ok=False)
        user = output / "user"
        user.mkdir()
        (user / "config").write_text(
            '(supertux-config (pause_on_focusloss #f) (do_release_check #f) '
            '(disable_network #t) (video (video "gpu") (fullscreen #f) (window_width 1280) '
            '(window_height 720) (hd_textures ' + ('#t' if hd else '#f') + ') '
            f'(use_hdr #f) (fsr_mode {fsr})) '
            '(audio (sound_enabled #f) (music_enabled #f)))\n')
        command = [str(launcher), "--userdir", str(user), "--debug"]
        if level:
            command.append(str(data / level))
        errors = []
        with (output / "run.log").open("wb") as log:
            try:
                result = subprocess.run(command, env=env, stdout=log,
                                        stderr=subprocess.STDOUT, timeout=180)
                if result.returncode != 0:
                    errors.append(f"exit={result.returncode}")
            except subprocess.TimeoutExpired:
                errors.append("timeout (game process killed)")
        text = (output / "run.log").read_text(errors="replace")
        expected_driver = "moltenvk" if args.driver == "auto" else args.driver
        for pattern in ("SDL_GPU driver: vulkan", f"/{expected_driver}.json", "[autotest] 180 frames drawn"):
            if pattern not in text:
                errors.append(f"missing: {pattern}")
        if hd and not re.search(r"HD manifest .*: " + str(info['hd_assets']['texture_count']) + r" scaled textures", text):
            errors.append("HD manifest not loaded")
        if hd and "HD texture loaded:" not in text:
            errors.append("No HD texture was decoded")
        if hd and level and not re.search(r"HD texture loaded: .*background/misc/clouds\.png x2", text):
            errors.append("Upscaled cloud texture not loaded")
        if not hd and "HD texture loaded:" in text:
            errors.append("HD-off setting ignored")
        if re.search(r"\[FATAL\]|Couldn't load config file|VUID-|Validation Error|HD texture .* failed|SDL_CreateGPUTexture failed|SDL_CreateGPUTransferBuffer failed", text):
            errors.append("Renderer/texture failure (see log)")
        shots = list((user / "screenshots").glob("*.png"))
        if not shots or any(png_info(p)[0][0] < 640 or p.stat().st_size < 51200 for p in shots):
            errors.append("Missing/invalid screenshot")
        results.append({"case": name, "driver": expected_driver, "errors": errors})
        print(json.dumps(results[-1]), flush=True)
    args.output.joinpath("results.json").write_text(json.dumps(results, indent=2) + "\n")
    return 1 if any(result["errors"] for result in results) else 0


if __name__ == "__main__":
    sys.exit(main())
