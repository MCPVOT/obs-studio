#!/usr/bin/env -S uv run --script
"""Suspected vulnerabilities go to a private advisory, not a public issue (#1260).

Usage:
    uv run python -m unittest ci/test_security_reporting.py

SECURITY.md forbids public issues for a suspected vulnerability. The worker
docs and issue templates must point there, or a worker following them files
the defect, and a public reproducer, in the open.
"""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ADVISORY_URL = "https://github.com/obs-rust/obs-studio/security/advisories/new"


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class SecurityReportingTests(unittest.TestCase):
    def test_security_md_has_advisory_url(self) -> None:
        self.assertIn(ADVISORY_URL, read("SECURITY.md"))

    def test_worker_docs_link_security_md(self) -> None:
        for path in (
            ".claude/skills/obs-worker/SKILL.md",
            "docs/rust-port/coordination.md",
            "AGENTS.md",
        ):
            with self.subTest(path=path):
                self.assertRegex(read(path), r"\]\([./]*SECURITY\.md\)", f"{path} must link SECURITY.md")

    def test_bug_template_mentions_security_md(self) -> None:
        self.assertIn("SECURITY.md", read(".github/ISSUE_TEMPLATE/bug.yml"))

    def test_issue_config_has_advisory_link(self) -> None:
        config = read(".github/ISSUE_TEMPLATE/config.yml")
        self.assertIn("name: Report a vulnerability", config)
        self.assertIn(f"url: {ADVISORY_URL}", config)


if __name__ == "__main__":
    unittest.main()
