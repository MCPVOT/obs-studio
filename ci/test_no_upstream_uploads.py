#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///
"""The frontend sends no logs or crash reports to upstream OBS Studio (#1731).

Usage:
    uv run python -m unittest ci/test_no_upstream_uploads.py

obs-rust runs no log upload service, and upstream's log service and analyzer
are not ours to send OBS-Studio-Rust data to. Crash reports and logs stay on
the user's computer; the UI points to the obs-rust issue page instead.
"""

import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

FORBIDDEN = (
    "obsproject.com/logs/upload",
    "obsproject.com/tools/analyzer",
)

SOURCE_SUFFIXES = {".c", ".cpp", ".h", ".hpp", ".mm", ".ui"}


def frontend_sources() -> list[Path]:
    out = subprocess.run(
        ["git", "ls-files", "frontend"], cwd=ROOT, check=True, capture_output=True, text=True
    ).stdout
    return [ROOT / p for p in out.splitlines() if Path(p).suffix in SOURCE_SUFFIXES]


class NoUpstreamUploads(unittest.TestCase):
    def test_frontend_has_no_upstream_upload_urls(self) -> None:
        hits = []
        for path in frontend_sources():
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except FileNotFoundError:
                continue
            for lineno, line in enumerate(text.splitlines(), 1):
                for url in FORBIDDEN:
                    if url in line:
                        hits.append(f"{path.relative_to(ROOT)}:{lineno}: {url}")
        self.assertEqual(hits, [], "frontend sends data to upstream OBS Studio (#1731):\n" + "\n".join(hits))


if __name__ == "__main__":
    unittest.main()
