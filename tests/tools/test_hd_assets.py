"""HD archive safety and logical-size tests (GPL-3.0-or-later)."""
import hashlib
import io
import json
from pathlib import Path
import struct
import tarfile
import tempfile
from types import SimpleNamespace
import unittest

from tools.hd_assets import digest, install, verify


class HDAssetsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.source = self.base / "share/supertux2"
        self.root = self.base / "overlay"
        for path, size in ((self.source, 2), (self.root, 4)):
            (path / "images").mkdir(parents=True)
            # Only IHDR is needed by the stdlib dimensions/alpha validator.
            (path / "images/test.png").write_bytes(
                b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR"
                + struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0) + b"\0" * 4)
        (self.root / "images/UPSCALE_LOG.txt").write_text("test.png model x2\n")
        self.reindex()

    def reindex(self):
        (self.root / "ASSET_INDEX.json").write_text(json.dumps({
            "texture_count": 1, "files": {
                p.relative_to(self.root).as_posix(): digest(p)
                for p in self.root.rglob("*") if p.is_file() and p.name != "ASSET_INDEX.json"}}))

    def test_valid(self):
        verify(self.root, self.source)

    def test_corrupt(self):
        with (self.root / "images/test.png").open("ab") as stream:
            stream.write(b"corrupt")
        with self.assertRaisesRegex(ValueError, "checksum"):
            verify(self.root, self.source)

    def test_bad_scale(self):
        (self.root / "images/UPSCALE_LOG.txt").write_text("test.png model x4\n")
        self.reindex()
        with self.assertRaisesRegex(ValueError, "Logical size"):
            verify(self.root, self.source)

    def test_missing_png(self):
        (self.root / "images/test.png").unlink()
        with self.assertRaisesRegex(ValueError, "scale manifest"):
            verify(self.root, self.source)

    def test_safe_install(self):
        archive = self.base / "assets.tar.gz"
        with tarfile.open(archive, "w:gz") as stream:
            for path in self.root.rglob("*"):
                if path.is_file():
                    stream.add(path, "data-hd/" + path.relative_to(self.root).as_posix())
        self.do_install(archive)
        verify(self.source.parent / "data-hd", self.source)
        with self.assertRaisesRegex(ValueError, "overwrite"):
            self.do_install(archive)

    def do_install(self, archive, checksum=None):
        pin = self.base / "pin.json"
        pin.write_text(json.dumps({"sha256": checksum or digest(archive), "texture_count": 1}))
        install(SimpleNamespace(archive=archive, pin=pin, data_dir=self.source))

    def test_archive_attacks(self):
        for name, link in (("../escape", False), ("/absolute", False),
                           ("data-hd/link", True)):
            with self.subTest(name=name):
                archive = self.base / "attack.tar.gz"
                with tarfile.open(archive, "w:gz") as stream:
                    info = tarfile.TarInfo(name)
                    if link:
                        info.type = tarfile.SYMTYPE
                        info.linkname = "../../escape"
                        stream.addfile(info)
                    else:
                        info.size = 1
                        stream.addfile(info, io.BytesIO(b"x"))
                with self.assertRaisesRegex(ValueError, "Unsafe"):
                    self.do_install(archive)
                with self.assertRaisesRegex(ValueError, "archive checksum"):
                    self.do_install(archive, hashlib.sha256(b"wrong").hexdigest())


if __name__ == "__main__":
    unittest.main()
