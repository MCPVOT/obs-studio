#!/usr/bin/env -S uv run --script
"""Canonical release assets and real updater metadata contracts for #624."""

import hashlib
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location(
    "asset_metadata", ROOT / "build-aux/appimage/asset_metadata.py"
)
metadata = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(metadata)
VERSION = "OBS-Studio-Rust 0.1.0-beta.20261014 (based on OBS 33.0.0)"
NAME = "OBS-Studio-Rust-0.1.0-beta.20261014-linux-x86_64.AppImage"
TARGET = "gh-releases-zsync|obs-rust|obs-studio|continuous|OBS-Studio-Rust-*-linux-x86_64.AppImage.zsync"


class AppImageAssetsTest(unittest.TestCase):
    maxDiff = 250

    def test_names_come_from_actual_executable_identity(self):
        self.assertEqual(metadata.asset_name(VERSION), NAME)
        self.assertEqual(
            metadata.asset_name("OBS Studio - 33.0.0"),
            "OBS-Studio-33.0.0-linux-x86_64.AppImage",
        )
        for value in (
            "",
            "OBS-Studio-Rust ../bad (based on OBS 33.0.0)",
            "unrelated output",
        ):
            with self.subTest(value=value), self.assertRaises(ValueError):
                metadata.asset_name(value)

    def test_updater_accepts_matching_assets_and_rejects_each_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            app = Path(directory) / NAME
            app.write_bytes(b"actual payload for metadata checksum test")
            fields = {
                "Filename": NAME,
                "URL": NAME,
                "Length": str(app.stat().st_size),
                "SHA-1": hashlib.sha1(app.read_bytes()).hexdigest(),
            }
            sidecar = Path(str(app) + ".zsync")

            def write(values):
                sidecar.write_bytes(
                    (
                        "zsync: 0.6.2\n"
                        + "\n".join(f"{key}: {value}" for key, value in values.items())
                        + "\n\n"
                    ).encode()
                    + b"\xff\x00\x80"
                )

            write(fields)
            metadata.check_metadata(
                app, VERSION, TARGET, "obs-rust/obs-studio", require_update=True
            )
            for key, value in (
                ("Filename", "old.AppImage"),
                ("URL", "old.AppImage"),
                ("URL", "https://wrong.example/" + NAME),
                ("Length", "0"),
                ("SHA-1", "0" * 40),
            ):
                with self.subTest(key=key):
                    write({**fields, key: value})
                    with self.assertRaises(ValueError):
                        metadata.check_metadata(
                            app,
                            VERSION,
                            TARGET,
                            "obs-rust/obs-studio",
                            require_update=True,
                        )
            write(fields)
            for target in (
                "",
                TARGET.replace("OBS-Studio-Rust-*", "OBS_Studio-*"),
                TARGET.replace("obs-rust", "other"),
            ):
                with self.subTest(target=target), self.assertRaises(ValueError):
                    metadata.check_metadata(
                        app, VERSION, target, "obs-rust/obs-studio", require_update=True
                    )
            metadata.check_metadata(app, VERSION, "", "obs-rust/obs-studio")
            with self.assertRaises(ValueError):
                metadata.check_metadata(
                    app,
                    VERSION.replace("20261014", "20261015"),
                    "",
                    "obs-rust/obs-studio",
                )

    def test_workflows_preserve_the_canonical_name(self):
        appimage = (ROOT / ".github/workflows/appimage.yaml").read_text()
        release = (ROOT / ".github/workflows/release-rust.yaml").read_text()
        self.assertIn("asset_metadata.py name", appimage)
        self.assertIn(
            "continuous|OBS-Studio-Rust-*-linux-x86_64.AppImage.zsync", appimage
        )
        self.assertNotIn("OBS_Studio-", appimage)
        self.assertIn(
            "cp appimage/OBS-Studio-Rust-*-linux-x86_64.AppImage* dist/", release
        )
        self.assertNotIn("OBS_Studio-", release)
        smoke = (ROOT / "build-aux/appimage/test-appimage.sh").read_text()
        self.assertIn("asset_metadata.py verify", appimage)
        self.assertIn("EXPECT_APPIMAGE_UPDATE", smoke)


if __name__ == "__main__":
    unittest.main()
