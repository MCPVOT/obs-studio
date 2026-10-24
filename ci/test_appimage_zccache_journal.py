#!/usr/bin/env -S uv run --script
"""The AppImage build journals each compilation's cache outcome (#2182).

    uv run python -m unittest ci/test_appimage_zccache_journal.py

With the cache saved (#2179), a second build of the same commit still had
129 of 322 compilations cold, and zccache's totals do not say which or why.
The build step runs in a zccache session with a JSONL journal, and the
workflow summarizes it like the native Ubuntu lane does
(ci/native_cache_journal.py): outcome, miss reason and the names of the
inputs that differed, per source, without arguments or values.
"""

from __future__ import annotations

import importlib.util
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "appimage.yaml"
JOURNAL = "${RUNNER_TEMP:-/tmp}/appimage-zccache.jsonl"

spec = importlib.util.spec_from_file_location("appimage_build", ROOT / "build-aux" / "appimage" / "build.py")
build = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = build
spec.loader.exec_module(build)


def steps() -> list[str]:
    return re.split(r"\n(?=      - (?:name|uses): )", WORKFLOW.read_text(encoding="utf-8"))


class AppImageZccacheJournal(unittest.TestCase):
    def test_build_runs_in_a_journaled_session(self) -> None:
        script = build.BUILD
        start = script.find("zccache session-start")
        compile_ = script.find("cmake --build build_ubuntu")
        end = script.find('zccache session-end "$ZCCACHE_SESSION_ID"')
        self.assertGreaterEqual(start, 0, "no zccache session-start")
        self.assertIn(f'--journal "{JOURNAL}"', script)
        self.assertIn("--stats", script)
        self.assertIn("export ZCCACHE_SESSION_ID", script[:compile_])
        self.assertTrue(start < compile_ < end, "session must wrap cmake --build")
        # A failed session-start must not fail the build.
        self.assertRegex(script, r"if ZCCACHE_SESSION_ID=.*zccache session-start")

    def test_workflow_summarizes_the_journal(self) -> None:
        all_steps = steps()
        build_i = next(i for i, s in enumerate(all_steps) if "name: Build 🧱" in s)
        summary = [i for i, s in enumerate(all_steps) if "ci/native_cache_journal.py summarize" in s]
        self.assertEqual(len(summary), 1, "no journal summary step")
        step = all_steps[summary[0]]
        self.assertGreater(summary[0], build_i)
        self.assertIn("if: always()", step)
        self.assertIn("continue-on-error: true", step)
        self.assertIn('--journal "${RUNNER_TEMP}/appimage-zccache.jsonl"', step)


if __name__ == "__main__":
    unittest.main()
