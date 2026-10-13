#!/usr/bin/env -S uv run --script
"""The daily beta builds a package for every platform we ship (#628).

    uv run python -m unittest ci/test_release_platforms.py

release-rust.yaml's build matrix decides which packages a beta has. Intel
Macs got nothing: the matrix had only macos/arm64, although the release gate
already builds macOS x86_64.
"""

from __future__ import annotations

import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "release-rust.yaml"
sys.path.insert(0, str(ROOT / "ci"))
import release_decision  # noqa: E402

# (os, target) pairs every beta must be built for.
REQUIRED = {
    ("linux", "x86_64"),
    ("macos", "arm64"),
    ("macos", "x86_64"),
    ("windows", "x64"),
}


def build_matrix(text: str) -> list[dict[str, str]]:
    """The `include:` entries of the build job's matrix."""
    m = re.search(r"^  build:\n(.*?)^  \S", text, re.S | re.M)
    assert m, "no build job in release-rust.yaml"
    block = re.search(r"^        include:\n((?:^          .*\n)+)", m.group(1), re.M)
    assert block, "no matrix include in the build job"
    entries: list[dict[str, str]] = []
    for line in block.group(1).splitlines():
        kv = re.match(r"^          (?:(- )|  )([a-z_]+): (\S+)", line)
        if not kv:
            continue
        if kv.group(1):
            entries.append({})
        entries[-1][kv.group(2)] = kv.group(3)
    return entries


class ReleasePlatformsTest(unittest.TestCase):
    def setUp(self):
        self.text = WORKFLOW.read_text()
        self.matrix = build_matrix(self.text)

    def test_matrix_builds_every_shipped_platform(self):
        built = {(e["os"], e["target"]) for e in self.matrix}
        self.assertEqual(REQUIRED - built, set(), f"release-rust.yaml builds {sorted(built)}")

    def test_artifact_names_are_unique_per_target(self):
        # Two macOS legs uploading obs-studio-rust-macos would collide.
        m = re.search(r"name: (obs-studio-rust-.+)\n\s+path: pkg/", self.text)
        self.assertIsNotNone(m, "no package upload in the build job")
        self.assertIn("matrix.target", m.group(1))

    def test_collect_finds_the_intel_dmg(self):
        # package-obs names the x86_64 DMG *-macos-intel.dmg, not -apple.
        self.assertIn("kind=intel", self.text, "Collect and verify looks only for -apple.dmg")

    def test_release_requires_every_built_package(self):
        # The release refuses to publish without these (#622); a leg the
        # matrix builds must be on the list, or it can silently go missing.
        required = release_decision.required_assets("1.0")
        for os_, target in sorted(REQUIRED - {("linux", "x86_64")}):
            with self.subTest(os=os_, target=target):
                self.assertTrue(
                    any(f"-{os_}-{target}." in a for a in required),
                    f"{os_}-{target} missing from {required}",
                )


if __name__ == "__main__":
    unittest.main()
