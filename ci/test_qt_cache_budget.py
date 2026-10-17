#!/usr/bin/env -S uv run --script
"""Qt's useful compiler working set fits its declared and enforced budget."""
import re
import tomllib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class QtCacheBudgetTest(unittest.TestCase):
    def setUp(self):
        text = (ROOT / ".github/workflows/build-project.yaml").read_text()
        self.job = text.split("  linux-qt-frontend:\n", 1)[1].split("\n  rust-tier2:", 1)[0]
        self.family = tomllib.loads((ROOT / "ci.toml").read_text())["cache"]["family"]["qt-ccache"]

    def test_budget_fits_observed_entry_and_enforced_limit(self):
        # Master cache snapshot in #1286: 1,112,530,605 bytes, not 300 MB.
        amount, unit = re.fullmatch(r"([0-9.]+)(MB|GB)", self.family["max"]).groups()
        declared = float(amount) * (1_000_000 if unit == "MB" else 1_000_000_000)
        self.assertGreaterEqual(declared, 1_112_530_605)
        limit = re.search(r"CCACHE_MAXSIZE: (\d+)M", self.job)
        self.assertIsNotNone(limit, "bound cache contents before upload")
        self.assertLess(int(limit.group(1)) * 1_000_000, declared)
        self.assertIn('ccache --max-size="$CCACHE_MAXSIZE"', self.job)
        self.assertIn("ccache --cleanup", self.job)

    def test_hit_statistics_describe_this_job_only(self):
        self.assertLess(self.job.index("ccache --zero-stats"), self.job.index("- name: Build Frontend"))
        self.assertGreater(self.job.index("ccache --show-stats --verbose"), self.job.index("- name: Build Frontend"))


if __name__ == "__main__":
    unittest.main()
