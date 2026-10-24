#!/usr/bin/env -S uv run --script
"""The main window's title version reaches only the source that shows it (#2182).

    uv run python -m unittest ci/test_frontend_title_define_scope.py

frontend/CMakeLists.txt gave the whole obs-studio target
OBS_RUST_GIT_DESCRIBE (`git describe --long`, new on every commit), so every
commit changed the command line of all ~88 frontend compilations and the
compiler cache missed them all ("context_not_found" in the AppImage journal,
#2182). Only frontend/widgets/OBSBasic.cpp.hpp reads it (and its release twin
OBS_RUST_TITLE_RELEASE), through the build group frontend.widgets+.hpp.cpp, so
the definitions are set on that one source.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CMAKE = ROOT / "frontend" / "CMakeLists.txt"
GROUP = ROOT / "frontend" / "build" / "frontend.widgets+.hpp.cpp"
DEFINES = ("OBS_RUST_GIT_DESCRIBE", "OBS_RUST_TITLE_RELEASE")


class FrontendTitleDefineScope(unittest.TestCase):
    def test_title_defines_are_not_target_wide(self) -> None:
        text = CMAKE.read_text(encoding="utf-8")
        for define in DEFINES:
            with self.subTest(define=define):
                self.assertNotRegex(text, rf"target_compile_definitions\([^)]*{define}")
                self.assertRegex(
                    text,
                    rf"set_property\(\s*SOURCE build/frontend\.widgets\+\.hpp\.cpp\s+APPEND\s+PROPERTY COMPILE_DEFINITIONS\s+{define}=",
                )

    def test_the_group_still_contains_the_reader(self) -> None:
        # If OBSBasic.cpp.hpp moves to another group, the definitions must follow.
        self.assertIn('#include "frontend/widgets/OBSBasic.cpp.hpp"', GROUP.read_text(encoding="utf-8"))
        readers = [p for p in (ROOT / "frontend").rglob("*.*pp")
                   if any(d in p.read_text(encoding="utf-8", errors="replace") for d in DEFINES)]
        self.assertEqual(sorted(p.relative_to(ROOT).as_posix() for p in readers),
                         ["frontend/widgets/OBSBasic.cpp.hpp"])


if __name__ == "__main__":
    unittest.main()
