#!/usr/bin/env -S uv run --script
"""The AppImage release-gate test matrix covers the distributions the README
promises it runs on (#2353): the glibc 2.28 floor, Ubuntu 24.04 (the LTS
upstream OBS 33 dropped) and the newest rolling release. The README names
exactly the images the matrix runs.

    uv run python -m unittest ci/test_appimage_test_distros.py
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "appimage.yaml"
README = ROOT / "README.md"

# Matrix image -> how the README names it.
NAMES = {
    "almalinux:8": "AlmaLinux 8",
    "ubuntu:22.04": "Ubuntu 22.04",
    "ubuntu:24.04": "Ubuntu 24.04",
    "archlinux:latest": "Arch Linux",
}


def matrix() -> dict[str, str]:
    text = WORKFLOW.read_text()
    return dict(re.findall(r"^\s*- image: (\S+)\n\s*scenarios: (.+)$", text, re.M))


class AppImageTestDistros(unittest.TestCase):
    def test_ubuntu_2404_runs_the_saved_profile(self):
        scenarios = matrix().get("ubuntu:24.04", "").split()
        for scenario in ("bundle", "first-start", "probes", "saved-profile"):
            self.assertIn(scenario, scenarios)

    def test_glibc_floor_still_starts(self):
        self.assertIn("first-start", matrix().get("almalinux:8", "").split())

    def test_readme_names_the_tested_distributions(self):
        text = README.read_text()
        line = re.search(r"The AppImage is tested on (.+?)\.\s", text, re.S)
        self.assertIsNotNone(line, "README has no 'The AppImage is tested on ...' line")
        tested = line.group(1)
        for image in matrix():
            self.assertIn(NAMES[image], tested)


if __name__ == "__main__":
    unittest.main()
