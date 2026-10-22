# CI rollout-day observation: 2026-10-21 UTC (#1269)

This is an observational baseline spanning several rollouts, before the Qt
used-object pruning publication. It is not a stable post-rollout day or a
cold/warm experiment. It does not establish a causal improvement.

The existing `ci/cache_metrics.py` collector enumerated 289 runs created from
2026-10-21T00:00:00Z inclusive to 2026-10-22T00:00:00Z exclusive. Both workflow
listings cover the window. It inspected 108 completed runs, selected evenly
within event/outcome strata (up to 12 each), plus documentation candidates.
Actual source comparisons determine code/docs/CI cohorts. Collection finished
2026-10-22T01:35:00Z.

| Code cohort | Sample runs | Successful gates | Median gate seconds | Sample p95 seconds | Sample runner-minutes | Canceled sample runner-minutes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Pull request | 27 | 10 | 637.5 | 1085 | 740.700 | 140.100 |
| Merge group | 12 | 9 | 746 | 1145 | 510.332 | 7.883 |

The eight-minute code queue target is not met in these nine successful queue
observations. Nearest-rank p95 on a small sample often equals the maximum.
Sample outcomes are PR 10 success / 9 failure / 8 canceled, queue 9 / 2 / 1.
These stratified proportions are not population failure rates. All in-window
PR outcomes are 78 success / 25 failure / 30 canceled; queue 74 / 2 / 1.
Sample runner-minutes cover only inspected latest attempts, not all 289 runs
or overwritten attempts, and are elapsed hosted job time rather than a bill.

No logs were collected in this pass. Compiler hits, cold/warm conditions and
literal executed/skipped test counts are unverified. Workflow job statuses
must not be substituted for test coverage. The capture labels counts as
non-skipped/skipped job records: non-skipped records may include canceled
or never-started jobs and do not establish job execution. Creation-to-start intervals can
include dependency waiting and do not isolate runner capacity delays.

The adjacent `2026-10-21-rollout-day.json.gz` preserves the full window listing,
collector summary, sampled source comparisons, and API job/step timing and
outcome records. Job metadata unrelated to those observations is projected
out. It uses deterministic gzip (`mtime=0`) to avoid several megabytes of
repetitive API text in the repository. It contains no downloaded job logs.

Reproduce from the repository root with authenticated `gh`:

```sh
capture_root=$(safe-mktemp /tmp/obs-ci-rollout-day)
uv run --no-project ci/cache_metrics.py --start 2026-10-21T00:00:00Z --end 2026-10-22T00:00:00Z --per-group 12 --output "$capture_root/capture"
safe-rm -r "$capture_root"
```

Later API collection may see changed outcomes or attempts. The committed
capture records its observation time; it is not a live dashboard.
