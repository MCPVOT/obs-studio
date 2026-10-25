#!/usr/bin/env -S uv run --script
"""macOS and Windows build against a Qt with the QTBUG-147602 backport
(obs-rust/obs-studio#2290).

    uv run --no-project ci/test_qt6_pin.py

On macOS 27.2, OBS crashed on every launch inside QWidget::setCursor ->
QImage::toCGImage (upstream obsproject/obs-studio#14061). The backport landed
in obs-rust/obs-deps#6. The first obs-deps release that carries it is
2026-10-25, so the Qt6 pin must not fall below it.
"""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOOR = "2026-10-25"
MIRROR = "https://github.com/obs-rust/obs-deps/releases/download"


def qt6_spec() -> dict:
    presets = json.loads((ROOT / "CMakePresets.json").read_text())
    for preset in presets["configurePresets"]:
        if preset["name"] == "dependencies":
            return preset["vendor"]["obsproject.com/obs-studio"]["dependencies"]["qt6"]
    raise AssertionError("CMakePresets.json has no dependencies preset")


class Qt6PinTest(unittest.TestCase):
    def test_qt6_carries_the_qtbug_147602_backport(self):
        spec = qt6_spec()
        self.assertGreaterEqual(spec["version"], FLOOR)
        self.assertEqual(spec["baseUrl"], MIRROR)

    def test_every_package_has_a_sha256(self):
        spec = qt6_spec()
        for group in ("hashes", "debugSymbols"):
            for target, digest in spec[group].items():
                with self.subTest(group=group, target=target):
                    self.assertRegex(digest, r"^[0-9a-f]{64}$")
        self.assertEqual(set(spec["hashes"]), {"macos-universal", "windows-x64", "windows-arm64"})


if __name__ == "__main__":
    unittest.main()
