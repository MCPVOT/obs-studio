#!/usr/bin/env -S uv run --script
"""The AppStream metadata shows OBS Studio Rust's own main window (#2132).

    uv run python -m unittest ci/test_metainfo_screenshot.py

App catalogs (the AppImage catalog, AppImageHub, software centers) show the
metainfo's default screenshot. It pointed at upstream's demo picture, and the
AppImage catalog asks for the application's main window. The picture is
docs/images/appimage-main-window.png, taken by the AppImage's own saved-profile
test (build-aux/appimage/test-appimage.sh), and the metainfo links it from this
repository.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
METAINFO = ROOT / "frontend" / "cmake" / "linux" / "com.obsproject.Studio.metainfo.xml.in"
IMAGE = "docs/images/appimage-main-window.png"
URL = f"https://raw.githubusercontent.com/obs-rust/obs-studio/master/{IMAGE}"


class MetainfoScreenshot(unittest.TestCase):
    def test_default_screenshot_is_our_main_window(self) -> None:
        text = METAINFO.read_text(encoding="utf-8")
        default = re.search(r'<screenshot type="default">(.*?)</screenshot>', text, re.S)
        self.assertIsNotNone(default, "no default screenshot")
        self.assertIn(f"<image>{URL}</image>", default.group(1))
        self.assertNotIn("obsproject.com", default.group(1))

    def test_the_image_is_in_the_repository(self) -> None:
        png = (ROOT / IMAGE).read_bytes()
        self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")


if __name__ == "__main__":
    unittest.main()
