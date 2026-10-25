#!/usr/bin/env -S uv run --script
"""The Rust .deb recommends what it starts at run time but cannot link
against (#2301): xdg-utils for xdg-open, and xdg-desktop-portal with a
backend for portals and PipeWire capture. apt installs Recommends by default.

    uv run --no-project ci/test_deb_recommends.py
"""

from __future__ import annotations

import importlib.util
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("app_identity_check", ROOT / "ci/app_identity_check.py")
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)


def cpack_field(name: str) -> str:
    cpack = (ROOT / "cmake/linux/cpackconfig.cmake").read_text()
    found = re.search(rf'set\(\s*CPACK_DEBIAN_PACKAGE_{name}\s+"([^"]*)"\s*\)', cpack)
    return found.group(1) if found else ""


class DebRecommendsTest(unittest.TestCase):
    def test_cpack_recommends_the_runtime_helpers(self):
        check.check_deb_recommends(cpack_field("RECOMMENDS"))

    def test_packaged_check_rejects_a_missing_helper(self):
        for field in ("", "xdg-utils", "xdg-desktop-portal, xdg-desktop-portal-gtk"):
            with self.subTest(field=field), self.assertRaises(RuntimeError):
                check.check_deb_recommends(field)

    def test_alternatives_count(self):
        check.check_deb_recommends("xdg-utils, xdg-desktop-portal, xdg-desktop-portal-gtk | xdg-desktop-portal-backend")

    def test_virtual_camera_module_is_suggested(self):
        self.assertIn("v4l2loopback-dkms", cpack_field("SUGGESTS"))


if __name__ == "__main__":
    unittest.main()
