#!/usr/bin/env -S uv run --script
"""The README names as a user's setup only package formats a release ships
(#2304): a Flatpak is a CI build check, not a download.

    uv run --no-project ci/test_readme_package_formats.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# README name -> the asset file the release workflow uploads.
FORMATS = {"AppImage": r"\.AppImage\b", ".deb": r"\.deb\b", "Flatpak": r"\.flatpak(?:ref)?\b"}


class ReadmePackageFormatsTest(unittest.TestCase):
    def test_setups_the_readme_names_are_released(self):
        readme = (ROOT / "README.md").read_text()
        release = (ROOT / ".github/workflows/release-rust.yaml").read_text()
        setup = re.search(r"on your setup \(([^)]*)\)", " ".join(readme.split()))
        self.assertIsNotNone(setup, "README lost its 'on your setup (...)' list")
        for name, asset in FORMATS.items():
            if name in setup.group(1):
                with self.subTest(format=name):
                    self.assertRegex(release, asset, f"README asks about {name}, but no release ships one")

    def test_readme_says_there_is_no_flatpak(self):
        readme = " ".join((ROOT / "README.md").read_text().split())
        self.assertIn("There is no OBS-Studio-Rust Flatpak yet", readme)


if __name__ == "__main__":
    unittest.main()
