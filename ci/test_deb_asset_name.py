#!/usr/bin/env -S uv run --script
"""The .deb release asset names the distribution it is built on and needs
(#2297): dpkg-shlibdeps records that runner's library versions, so on any
other release apt fails with a wall of unmet dependencies.

    uv run --no-project ci/test_deb_asset_name.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

import release_decision

ROOT = Path(__file__).resolve().parent.parent

WORKFLOW = (ROOT / ".github/workflows/release-rust.yaml").read_text()


def linux_runner() -> str:
    found = re.search(r"- os: linux\n\s+runner: (ubuntu-[\d.]+)", WORKFLOW)
    assert found, "release-rust.yaml has no linux build leg"
    return found.group(1)


class DebAssetNameTest(unittest.TestCase):
    def test_required_asset_names_the_runner_release(self):
        distro = linux_runner()
        debs = [a for a in release_decision.required_assets("1.2.3") if a.endswith(".deb")]
        self.assertEqual(debs, [f"OBS-Studio-Rust-1.2.3-{distro}-x86_64.deb"])

    def test_collect_names_the_deb_from_os_release(self):
        collect = WORKFLOW.split("- name: Collect and verify", 1)[1].split("\n      - name:", 1)[0]
        self.assertIn("/etc/os-release", collect)
        self.assertIn('pkg/OBS-Studio-Rust-$VERSION-$distro-${{ matrix.target }}.deb', collect)

    def test_release_body_installs_the_named_deb(self):
        self.assertIn(f"./OBS-Studio-Rust-$VERSION-{linux_runner()}-x86_64.deb", WORKFLOW)
        self.assertNotIn("-linux-x86_64.deb", WORKFLOW)


if __name__ == "__main__":
    unittest.main()
