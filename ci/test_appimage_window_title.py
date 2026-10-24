#!/usr/bin/env -S uv run --script
"""test-appimage.sh recognizes the main window by its "OBS Studio Rust" title.

    uv run python -m unittest ci.test_appimage_window_title

#1985 titled the main window "OBS Studio Rust <version>" (#1983). The first-start
check matched only the old "OBS-Studio-Rust <version>" title, so it never found
the window and every distro's first-start scenario failed (#2162).
"""

from __future__ import annotations

import re
import subprocess
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent.parent / "build-aux/appimage/test-appimage.sh"
CLASS = '("obs-studio-rust-gui" "obs-studio-rust-gui")  1280x770+0+0  +0+0'
# Titles from test/cmocka/test_product_title.c.
TITLES = [
    "OBS Studio Rust 0.1.0-beta.20261024",
    "OBS Studio Rust 0.1.0-beta.20261023-dev+78-g4527fbf78",
    "OBS Studio Rust dev",
]


def window_pattern() -> str:
    match = re.search(r"if grep -qE '([^']+)' <<<\"\$WINDOWS\"; then", SCRIPT.read_text())
    if match is None:
        raise AssertionError("test-appimage.sh has no grep -qE window title check")
    return match.group(1)


def matches(pattern: str, line: str) -> bool:
    result = subprocess.run(["grep", "-qE", pattern], input=line, text=True)
    return result.returncode == 0


class AppImageWindowTitleTest(unittest.TestCase):
    def test_main_window_title_matches(self):
        pattern = window_pattern()
        for title in TITLES:
            line = f'     0x400040 "{title}": {CLASS}'
            self.assertTrue(matches(pattern, line), f"{pattern!r} does not match {line!r}")


if __name__ == "__main__":
    unittest.main()
