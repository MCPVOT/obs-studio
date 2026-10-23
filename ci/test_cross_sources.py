#!/usr/bin/env -S uv run --script
"""Every source the cross-compile check names exists.

cmake/cross/CMakeLists.txt lists each platform-only target's unity group
files by name. No CI job runs the cross check (#1294), so a group renamed in
its component (#1607 split coreaudio-encoder.core+ into .windows+/.macos+)
left the check failing at configure with nobody noticing. This keeps the
names in step with the tree.

    uv run python -m unittest ci/test_cross_sources.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CROSS = ROOT / "cmake/cross/CMakeLists.txt"

# One _obs_cross_plugin(...) call, up to its closing parenthesis.
CALL = re.compile(r"_obs_cross_plugin\((.*?)\n?\s*\)", re.S)
KEYWORDS = {"DIR", "SOURCES", "INCLUDES", "DEFINES", "OBJC_ARC", "OBJC_MODULES"}


def cross_sources() -> list[tuple[str, Path]]:
    """(target, path) for every SOURCES entry of every cross plugin."""
    found = []
    for call in CALL.finditer(CROSS.read_text(encoding="utf-8")):
        words = call.group(1).split()
        target, args = words[0], words[1:]
        directory, section = None, None
        for word in args:
            if word in KEYWORDS:
                section = word
            elif section == "DIR":
                directory = word
            elif section == "SOURCES" and "${" not in word:
                # A ${variable} comes from a source list CMake computes.
                found.append((target, ROOT / directory / word))
    return found


class CrossSourcesTest(unittest.TestCase):
    def test_parses_cross_plugins(self) -> None:
        self.assertGreater(len(cross_sources()), 10)

    def test_every_source_exists(self) -> None:
        missing = [f"{t}: {p.relative_to(ROOT)}" for t, p in cross_sources() if not p.is_file()]
        self.assertEqual(missing, [], "cmake/cross/CMakeLists.txt names missing sources")


if __name__ == "__main__":
    unittest.main()
