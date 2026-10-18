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
    def test_archive_restore_is_distinct_from_compiler_hits(self):
        log = "\n".join([
            "Tier2\tRestore cache\t2026-10-17T18:39:59Z Cache Size: ~266 MB (278493433 B)",
            "Tier2\tBuild\t2026-10-17T18:46:22Z Compilations: 772 total (0 cached, 772 cold, 0 non-cacheable)",
            "Gate\tPost setup\t2026-10-17T18:47:01Z final zccache session stats: hits=0 misses=73 compilations=75 non_cacheable=0 errors=2",
            "Qt\tReport cache\t2026-10-17T18:47:02Z Hits: 304 / 304 (100.0%)",
            "Tests ON\tTest\t2026-10-17T18:47:03Z 100% tests passed, 0 tests failed out of 283",
            "Tier2\tSave\t2026-10-17T18:47:04Z Cache saved with key: zccache-tier2-linux-both-abc",
        ])
        parsed = metrics.parse_cache_logs(log)
        self.assertEqual(parsed["Tier2"]["archive_bytes"], [278493433])
        self.assertEqual(parsed["Tier2"]["compiler_reports"][0]["cached"], 0)
        self.assertEqual(parsed["Tier2"]["compiler_reports"][0]["cold"], 772)
        self.assertEqual(parsed["Gate"]["compiler_reports"][0]["errors"], 2)
        self.assertEqual(parsed["Qt"]["compiler_reports"][0]["hits"], 304)
        self.assertEqual(parsed["Tests ON"]["ctest_reports"], [{"total": 283, "failed": 0}])
        self.assertEqual(parsed["Tier2"]["saved_keys"], ["zccache-tier2-linux-both-abc"])

    def test_missing_log_counters_are_not_invented(self):
        self.assertEqual(metrics.parse_cache_logs("Build\tstep\t2026-10-17T18:00:00Z Cache restored successfully"), {})

    def test_current_ctest_success_preserves_failures_and_unknown_counts(self):
        log = "\n".join([
            "Qt\tBuild frontend\t2026-10-18T01:29:45Z 100% tests passed out of 15",
            "OFF\tTest\t2026-10-18T01:19:08Z 99% tests passed, 1 tests failed out of 295",
            "Unknown\tTest\t2026-10-18T01:29:45Z Test project /build",
        ])
        parsed = metrics.parse_cache_logs(log)
        self.assertIn("Qt", parsed, "Capture the observed shorter success summary")
        self.assertEqual(parsed["Qt"]["ctest_reports"], [{"total": 15, "failed": 0}])
        self.assertEqual(parsed["OFF"]["ctest_reports"], [{"total": 295, "failed": 1}])
        self.assertNotIn("Unknown", parsed)

    def test_source_cohort_requires_files(self):
        self.assertEqual(metrics.cohort([]), "unknown")
        self.assertEqual(metrics.cohort(["docs/a.md", "AGENTS.md"]), "docs")
        self.assertEqual(metrics.cohort(["ci/test_a.py", ".github/workflows/ci.yml"]), "ci-config")
        self.assertEqual(metrics.cohort(["docs/a.md", "libobs/obs.c"]), "code")

    def test_structured_report_evidence_is_not_product_code(self):
        evidence = "docs/ci-evidence/2026-10-17-results.json"
        self.assertEqual(metrics.cohort(["docs/report.md", evidence]), "docs")
        self.assertEqual(metrics.cohort(["ci/cache_metrics.py", evidence]), "ci-config")
        self.assertEqual(metrics.cohort(["libobs/obs.c", evidence]), "code")
        self.assertEqual(metrics.cohort(["plugins/service/data/config.json"]), "code")

    def test_report_figures_and_generators_stay_in_documentation_cohort(self):
        evidence = ["docs/ci-evidence/libobs-2026-10-17/build-trials.svg",
                    "docs/ci-evidence/libobs-2026-10-17/plot_trials.py"]
        self.assertEqual(metrics.cohort(evidence), "docs")
        self.assertEqual(metrics.cohort(evidence + ["ci/cache_metrics.py"]), "ci-config")
        self.assertEqual(metrics.cohort(evidence + ["libobs/obs.c.h"]), "code")
        self.assertEqual(metrics.cohort(["scripts/plot_trials.py"]), "code")

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
