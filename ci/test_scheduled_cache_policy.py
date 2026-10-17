#!/usr/bin/env -S uv run --script
"""Nightly builds preserve warm caches and can call the CI proof reader."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class ScheduledCachePolicyTest(unittest.TestCase):
    def setUp(self):
        self.text = (ROOT / ".github/workflows/scheduled.yaml").read_text()
        self.jobs = dict(re.findall(r"^  ([\w-]+):\n((?:    .*\n|\n)*)", self.text, re.M))

    def test_nightly_has_no_independent_cache_purge(self):
        self.assertNotIn("cache-cleanup", self.jobs)
        self.assertNotRegex(self.text, r"gh api .*DELETE.*actions/caches")
        for block in self.jobs.values():
            self.assertNotRegex(block, r"needs:.*cache-cleanup")

    def test_build_can_read_lane_proofs(self):
        self.assertIn("actions: read", self.jobs["build-project"])

    def test_new_root_jobs_skip_fork_schedules(self):
        for name in ("build-project", "analyze-project"):
            with self.subTest(job=name):
                self.assertIn("github.event_name != 'schedule' || github.repository == 'obs-rust/obs-studio'", self.jobs[name])


if __name__ == "__main__":
    unittest.main()
