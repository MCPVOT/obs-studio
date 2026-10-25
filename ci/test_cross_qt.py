#!/usr/bin/env -S uv run --script
"""The cross check's host Qt is the one CI pins (#2408).

cmake/cross/CMakeLists.txt runs uic from the Linux Qt that
build-project.yaml's Qt job fetches (QT_TAG, QT_ASSET, QT_SHA256), so the
generated ui_*.h match the version of the target's prebuilt Qt headers.

    uv run python -m unittest ci/test_cross_qt.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CROSS = (ROOT / "cmake/cross/CMakeLists.txt").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/build-project.yaml").read_text(encoding="utf-8")


def cross_pin(name: str) -> str:
    m = re.search(rf"^set\(OBS_CROSS_HOST_QT_{name} (\S+)\)$", CROSS, re.M)
    assert m, f"cmake/cross/CMakeLists.txt sets no OBS_CROSS_HOST_QT_{name}"
    return m[1]


class CrossQtTest(unittest.TestCase):
    def test_host_qt_matches_the_workflow_pin(self):
        for name in ("TAG", "ASSET", "SHA256"):
            with self.subTest(name=name):
                self.assertIn(f"QT_{name}: {cross_pin(name)}\n", WORKFLOW)

    def test_host_qt_is_linux(self):
        self.assertRegex(cross_pin("ASSET"), r"-linux-x86_64-")

    def test_frontend_groups_are_compiled(self):
        for group in ("frontend.windows-obs-main+", "frontend.macos-arc-mm+"):
            self.assertIn(f"build/{group}", CROSS)


if __name__ == "__main__":
    unittest.main()
