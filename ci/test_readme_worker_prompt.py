#!/usr/bin/env -S uv run --script
"""The README's copy-paste worker prompt (#2326): one fenced block, near
the top, identical to docs/worker-prompt.md, that starts any agent as a
worker and ends at an open PR.

    uv run --no-project ci/test_readme_worker_prompt.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FENCE = re.compile(r"^```text\n(.*?)^```$", re.S | re.M)
REQUIRED = (
    "obs-rust/obs-studio",
    "AGENTS.md",
    ".claude/skills/obs-worker/SKILL.md",
    "docs/rust-port/coordination.md",
    "docs/rust-port/testing-policy.md",
    "label:ready",
    "Claiming:",
    "RED",
    "GREEN",
    "master",
    "Do not merge",
    "Discord",
)


def block(text: str) -> str:
    found = FENCE.search(text)
    return found.group(1) if found else ""


class ReadmeWorkerPromptTest(unittest.TestCase):
    def setUp(self):
        self.readme = (ROOT / "README.md").read_text()
        self.prompt = block(self.readme)

    def test_readme_offers_the_prompt_near_the_top(self):
        self.assertIn("Want to help port OBS to Rust?", self.readme)
        self.assertLess(self.readme.index("Want to help port OBS to Rust?"), self.readme.index("# Conversion progress"))
        self.assertLess(self.readme.index("```text"), self.readme.index("# Conversion progress"))

    def test_prompt_matches_its_canonical_file(self):
        canonical = ROOT / "docs/worker-prompt.md"
        self.assertTrue(canonical.is_file())
        self.assertTrue(self.prompt)
        self.assertEqual(self.prompt, block(canonical.read_text()))

    def test_prompt_carries_what_a_cold_agent_needs(self):
        for token in REQUIRED:
            with self.subTest(token=token):
                self.assertIn(token, self.prompt)

    def test_prompt_is_short_and_backend_neutral(self):
        lines = self.prompt.strip().splitlines()
        self.assertLessEqual(len(lines), 18)
        for word in ("claude -p", "codex exec", "opus", "sonnet", "gpt-"):
            self.assertNotIn(word, self.prompt.lower())

    def test_named_files_exist(self):
        for path in re.findall(r"[\w./-]+\.md", self.prompt):
            with self.subTest(path=path):
                self.assertTrue((ROOT / path).is_file())


if __name__ == "__main__":
    unittest.main()
