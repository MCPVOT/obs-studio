#!/usr/bin/env -S uv run --script
"""Tier 2 macOS and Windows run each ctest suite in parallel (#2138).

    uv run python -m unittest ci/test_tier2_native_parallel.py

Both jobs ran their four unchanged suites (C reference, hybrid, and the two
Cargo cores) serially: about 8 min of macOS's 13 and 5.7 min of Windows's
12.5, the critical path of the merge queue. Linux already runs ctest with
-j "$OBS_CTEST_JOBS"; tests that must not overlap carry RUN_SERIAL or
RESOURCE_LOCK, which ctest honors on every platform.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "build-project.yaml"
VALIDATE_SH = ROOT / "rust" / "tools" / "tier2-validate" / "validate.sh"
VALIDATE_PS1 = ROOT / "rust" / "tools" / "tier2-validate" / "validate.ps1"


def step(name: str) -> str:
    text = WORKFLOW.read_text(encoding="utf-8")
    m = re.search(rf"- name: {re.escape(name)}.*?(?=\n      - name: |\Z)", text, re.S)
    assert m, f"no step {name}"
    return m.group(0)


class NativeTier2Parallel(unittest.TestCase):
    def test_workflow_sets_jobs_for_both_platforms(self) -> None:
        for name in ("Validate Tier 2 on macOS", "Validate Tier 2 on Windows"):
            with self.subTest(step=name):
                self.assertRegex(step(name), r"OBS_CTEST_JOBS: '?[2-9]")

    def test_every_suite_run_uses_the_jobs(self) -> None:
        sh = VALIDATE_SH.read_text(encoding="utf-8")
        suites = [l for l in sh.splitlines() if "ctest --test-dir" in l and "if !" in l]
        self.assertEqual(len(suites), 2, suites)
        for line in suites:
            self.assertIn('"${ctest_jobs[@]}"', line)
        ps1 = VALIDATE_PS1.read_text(encoding="utf-8")
        suites = [l for l in ps1.splitlines() if l.strip().startswith("ctest --test-dir") and "@repeatArgs" not in l and "--show-only" not in l]
        self.assertEqual(len(suites), 2, suites)
        for line in suites:
            self.assertIn("@ctestJobs", line)


if __name__ == "__main__":
    unittest.main()
