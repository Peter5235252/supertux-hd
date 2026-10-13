#!/usr/bin/env python3
"""Package and verify the offline HD overlay (GPL-3.0-or-later)."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import struct
import tarfile
import tempfile


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def png_info(path):
    with path.open("rb") as stream:
        header = stream.read(33)
    if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Invalid PNG: {path}")
    return struct.unpack(">II", header[16:24]), header[25]


def scales(root):
    result = {}
    for line in (root / "images/UPSCALE_LOG.txt").read_text().splitlines():
        rel, model, scale = line.rsplit(" ", 2)
        path = PurePosixPath(rel)
        if path.is_absolute() or ".." in path.parts or str(path) != rel:
            raise ValueError(f"Unsafe manifest path: {rel}")
        if rel in result or scale not in ("x1", "x2", "x4"):
            raise ValueError(f"Invalid/duplicate scale entry: {line}")
        result[rel] = int(scale[1:])
    if not result:
        raise ValueError("Empty HD scale manifest")
    return result


def verify(root, source=None):
    entries = scales(root)
    actual = {p.relative_to(root / "images").as_posix()
              for p in (root / "images").rglob("*.png")}
    if actual != set(entries):
        raise ValueError("HD PNG files do not match the scale manifest")
    for rel, scale in entries.items():
        size, color = png_info(root / "images" / rel)
        if source:
            original = source / "images" / rel
            if not original.exists():
                original = original.with_suffix(".deprecated" + original.suffix)
            old_size, old_color = png_info(original)
            if size != tuple(n * scale for n in old_size):
                raise ValueError(f"Logical size mismatch: {rel}: {old_size} x{scale} != {size}")
            if old_color in (4, 6) and color not in (4, 6):
                raise ValueError(f"Alpha channel missing: {rel}")
    index_path = root / "ASSET_INDEX.json"
    if not index_path.exists():
        raise ValueError("HD checksum index missing")
    index = json.loads(index_path.read_text())
    actual_files = {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()}
    if actual_files != set(index["files"]) | {"ASSET_INDEX.json"}:
        raise ValueError("Files do not match the HD checksum index")
    for rel, expected in index["files"].items():
        if digest(root / rel) != expected:
            raise ValueError(f"HD checksum mismatch: {rel}")
    if index["texture_count"] != len(entries):
        raise ValueError("HD texture count mismatch")
    print(f"Verified {len(entries)} HD textures, scale metadata and file checksums")


def pack(args):
    repo = args.repo.resolve()
    with tempfile.TemporaryDirectory(prefix="supertux-hd-assets-") as tmp:
        root = Path(tmp) / "data-hd"
        shutil.copytree(repo / "data-hd/images", root / "images")
        # The original batch dropped LA (grayscale+alpha) in a few images.
        # Repair only the staged copy; preserve RGB detail and source alpha.
        from PIL import Image
        repaired = []
        for path in sorted((root / "images").rglob("*.png")):
            rel = path.relative_to(root / "images")
            source = repo / "data/images" / rel
            if not source.exists():
                source = source.with_suffix(".deprecated" + source.suffix)
            with Image.open(source) as original, Image.open(path) as upscaled:
                if "A" in original.getbands() or "transparency" in original.info:
                    alpha = original.convert("RGBA").getchannel("A")
                    hd_alpha = upscaled.convert("RGBA").getchannel("A")
                    if alpha.getextrema()[0] < 255 and hd_alpha.getextrema()[0] == 255:
                        image = upscaled.convert("RGBA")
                        image.putalpha(alpha.resize(image.size, Image.Resampling.LANCZOS))
                        image.save(path)
                        repaired.append(rel.as_posix())
        (root / "ALPHA_REPAIRS.json").write_text(json.dumps({
            "method": "Original artwork alpha resized with Lanczos; upscaled RGB preserved",
            "files": repaired}, indent=2) + "\n")
        print(f"Restored source transparency in {len(repaired)} staged textures: {repaired}")
        for name in ("LICENSE.txt", "ATTRIBUTION.md", "AI_DISCLOSURE.md"):
            shutil.copy2(repo / name, root / name)
        # Preserve the upstream per-asset license exceptions and creator credits.
        for name in ("AUTHORS", "credits.stxt", "ACKNOWLEDGEMENTS.txt"):
            shutil.copy2(repo / "data" / name, root / name)
        (root / "NOTICE.txt").write_text(
            "AI-upscaled derivatives of SuperTux artwork; original creators retain credit.\n"
            "Artwork retains its source license, including per-file exceptions in AUTHORS.\n"
            "See ATTRIBUTION.md and AI_DISCLOSURE.md. Models are not included.\n")
        files = sorted(p for p in root.rglob("*") if p.is_file())
        index = {"texture_count": len(scales(root)),
                 "files": {p.relative_to(root).as_posix(): digest(p) for p in files}}
        (root / "ASSET_INDEX.json").write_text(json.dumps(index, indent=2, sort_keys=True) + "\n")
        verify(root, repo / "data")
        args.archive.parent.mkdir(parents=True, exist_ok=True)
        with tarfile.open(args.archive, "w:gz", compresslevel=6) as archive:
            for path in sorted(root.rglob("*")):
                if not path.is_file():
                    continue
                info = archive.gettarinfo(str(path), "data-hd/" + path.relative_to(root).as_posix())
                info.uid = info.gid = info.mtime = 0
                info.uname = info.gname = ""
                info.mode = 0o644
                with path.open("rb") as stream:
                    archive.addfile(info, stream)
    checksum = digest(args.archive)
    args.archive.with_suffix(args.archive.suffix + ".sha256").write_text(
        f"{checksum}  {args.archive.name}\n")
    print(f"Archive: {args.archive} (sha256 {checksum})")


def install(args):
    pin = json.loads(args.pin.read_text())
    if digest(args.archive) != pin["sha256"]:
        raise ValueError("HD archive checksum mismatch")
    # Destination is the sibling of the original data directory, not a replacement.
    dest = args.data_dir.parent / "data-hd"
    if dest.exists():
        raise ValueError(f"Refusing to overwrite existing overlay: {dest}")
    with tempfile.TemporaryDirectory(prefix="supertux-hd-extract-") as tmp:
        with tarfile.open(args.archive, "r:gz") as archive:
            seen = set()
            for member in archive:
                path = PurePosixPath(member.name)
                if (not member.isfile() or path.is_absolute() or ".." in path.parts
                        or not path.parts or path.parts[0] != "data-hd"
                        or len(path.parts) < 2 or member.name in seen):
                    raise ValueError(f"Unsafe/duplicate archive member: {member.name}")
                seen.add(member.name)
                target = Path(tmp).joinpath(*path.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as stream, target.open("wb") as output:
                    shutil.copyfileobj(stream, output)
        root = Path(tmp) / "data-hd"
        verify(root, args.data_dir)
        if len(scales(root)) != pin["texture_count"]:
            raise ValueError("Pinned HD texture count mismatch")
        shutil.copytree(root, dest)
    print(f"Installed HD overlay: {dest}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    create = commands.add_parser("pack")
    create.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    create.add_argument("--archive", type=Path, required=True)
    stage = commands.add_parser("install")
    stage.add_argument("--archive", type=Path, required=True)
    stage.add_argument("--pin", type=Path, required=True)
    stage.add_argument("--data-dir", type=Path, required=True)
    check = commands.add_parser("verify")
    check.add_argument("root", type=Path)
    check.add_argument("--source", type=Path)
    args = parser.parse_args()
    if args.command == "pack":
        pack(args)
    elif args.command == "install":
        install(args)
    else:
        verify(args.root, args.source)


if __name__ == "__main__":
    main()
