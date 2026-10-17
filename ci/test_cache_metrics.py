#!/usr/bin/env -S uv run --script
"""Evidence coverage, actual-diff cohorts and CI duration arithmetic."""
import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("cache_metrics", ROOT / "ci/cache_metrics.py")
metrics = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(metrics)


class CacheMetricsTest(unittest.TestCase):
    def test_source_cohort_requires_files(self):
        self.assertEqual(metrics.cohort([]), "unknown")
        self.assertEqual(metrics.cohort(["docs/a.md", "AGENTS.md"]), "docs")
        self.assertEqual(metrics.cohort(["ci/test_a.py", ".github/workflows/ci.yml"]), "ci-config")
        self.assertEqual(metrics.cohort(["docs/a.md", "libobs/obs.c"]), "code")

    def collect(self, missing_compare=False):
        run = {"databaseId": 1, "status": "completed", "conclusion": "success", "event": "pull_request",
               "headSha": "b" * 40, "headBranch": "docs/one", "createdAt": "2026-01-01T08:00:00Z",
               "updatedAt": "2026-01-01T08:02:00Z", "url": "https://example.invalid/run/1"}

        def fake_gh(*args):
            if args[:2] == ("run", "list"):
                return [run] if args[args.index("--workflow") + 1] == "ci.yml" else []
            if args[:2] == ("pr", "list"):
                return [{"headRefName": "docs/one", "headRefOid": "b" * 40, "baseRefOid": "a" * 40,
                         "createdAt": "2026-01-01T07:00:00Z", "closedAt": "2026-01-01T09:00:00Z"}]
            endpoint = args[1]
            if endpoint.endswith("/actions/runs/1"):
                return {"run_attempt": 1, "pull_requests": []}
            if "/jobs?" in endpoint:
                return [{"jobs": [{"name": "CI Gate ✅", "conclusion": "success", "steps": [],
                                   "started_at": "2026-01-01T08:01:00Z", "completed_at": "2026-01-01T08:02:00Z"}]}]
            if "/compare/" in endpoint:
                if missing_compare:
                    raise subprocess.CalledProcessError(1, ["gh", "api", endpoint])
                return {"files": [{"filename": "docs/a.md"}]}
            if "/actions/caches?" in endpoint:
                return [{"actions_caches": []}]
            raise AssertionError(args)

        scratch = ROOT / "target"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory, mock.patch.object(metrics, "gh", side_effect=fake_gh):
            return metrics.collect("test/repo", "2026-01-01T00:00:00+00:00", "2026-01-02T00:00:00Z", Path(directory), 12)

    def test_resolves_old_pr_diff_and_deduplicates_docs_candidates(self):
        report = self.collect()
        self.assertEqual(report["listed_runs"], 1)
        self.assertEqual(report["sampled_runs"], 1)
        self.assertEqual(report["cohorts"]["pull_request/docs"]["median_successful_gate_seconds"], 120)
        self.assertEqual(report["lanes"]["pull_request/CI Gate ✅"]["median_job_seconds"], 60)
        self.assertTrue(report["listing_coverage"]["ci.yml"]["window_covered"])

    def test_missing_compare_is_unknown_even_on_a_docs_branch(self):
        report = self.collect(missing_compare=True)
        self.assertIn("pull_request/unknown", report["cohorts"])
        self.assertNotIn("pull_request/docs", report["cohorts"])


if __name__ == "__main__":
    unittest.main()
