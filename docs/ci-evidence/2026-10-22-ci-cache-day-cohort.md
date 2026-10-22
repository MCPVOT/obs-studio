# CI cache and gate observations over a 24-hour creation cohort (#1269)

Window: **2026-10-21 13:35:00 UTC through 2026-10-22 13:35:00 UTC**,
using `ci.yml` run creation times. All PR and merge-group attempts are included,
with every observed job attempt. Manual dispatches are excluded from the cohort.
The original run inventory and eight later outcome rechecks remain separate.
Job lifecycle observations can extend beyond the creation window.

This population mixes code, documentation and CI changes, cache conditions and
rollout versions. Code-only post-rollout performance and causal optimization
benefits remain unmeasured by this cohort. No price or billing estimate is made.

## Population and outcomes

| Event | Runs | Successful workflows | Failed workflows | Cancelled workflows |
| --- | ---: | ---: | ---: | ---: |
| PR | 240 | 160 | 28 | 52 |
| Merge group | 139 | 116 | 7 | 16 |

These are observed final workflow conclusions. One originally running PR was
rechecked as successful. Failure counts include intentional RED work as well as
unexpected failures; cancellation causes were not comprehensively classified.

370 runs have job records from attempt 1 only, five from attempts 1–2, and
three from attempts 1–3. One run has no returned job records. The source records
retain that absence rather than inventing durations or execution.

## Required gate latency

Latency is initial run creation to the latest successful `CI Gate` completion.
It includes dependency/scheduling waits and any rerun interval. P95 uses the
nearest-rank observation, not interpolation. All times are seconds.

| Event / admission | n | Median | P95 |
| --- | ---: | ---: | ---: |
| PR, latest gate successful | 163 | 499.0 | 1692 |
| Merge group, latest gate successful | 120 | 527.5 | 1557 |
| PR, workflow and latest gate both successful | 160 | 498.5 | 1692 |
| Merge group, workflow and latest gate both successful | 116 | 538.0 | 1613 |

Required-gate success and workflow success differ. Four merge-group workflows
were cancelled in their off-gate janitor after a successful gate. Two cancelled
PRs have no failed/cancelled job entries in the collected job metadata; their
workflow cancellation is preserved without guessing its cause.

A separate [historical PR run 39677084695](https://github.com/obs-rust/obs-studio/actions/runs/39677084695)
had two selected macOS validation jobs cancelled because no hosted runner
acquired them. Their annotations establish missing validation. Its failure-check
step was skipped and CI Gate still succeeded; the workflow concluded failure.
[#1793](https://github.com/obs-rust/obs-studio/issues/1793), a verified native child
of #930, tracks current-state diagnosis and a cheap fixture regression.
This run cannot count as completed native coverage. No actual merge through the
gap is asserted.

The eight initially discrepant gate/workflow rows were individually rechecked;
only the initially running PR changed its outcome. The other conclusions and
attempt numbers were unchanged. Preserve both inventories when reproducing.

## Timing proxies and missing measurements

| Event | Known all-attempt job wall seconds | Observed execution-step envelope seconds | Inverted job timestamps | Jobs without execution-step envelope |
| --- | ---: | ---: | ---: | ---: |
| PR | 329397 | 316972 | 89 | 298 |
| Merge group | 208859 | 203856 | 25 | 79 |

Job wall time sums valid `completed_at - started_at` values for non-skipped API
job entries. The 114 inverted timestamps all belong to cancelled entries;
they are excluded and retained as anomalies, not clamped to zero. Cancelled
workflow cohorts account for 58697/26455 known job-wall seconds for PR/queue.

These are incomplete timing proxies. The historical unacquired macOS jobs show
that API job wall time can include runner-acquisition waiting; it is neither
metered billing nor a lower bound on allocated runner time. A second proxy sums
each job's interval from first to last valid, non-skipped execution-step
timestamp. Jobs without such timestamps remain unmeasured. This proxy also
omits or combines overhead and does not establish billed runner-minutes.

## Coverage level

| Event | Successful API job entries | Failed | Cancelled | Skipped |
| --- | ---: | ---: | ---: | ---: |
| PR | 2049 | 143 | 388 | 3539 |
| Merge group | 1458 | 29 | 102 | 1888 |

These counts include all returned attempts and unevaluated matrix/routing
entries. They describe API outcomes, not allocated hosts, test executables or
per-case execution. Per-case compatibility and protocol coverage must be
reported from completed test logs separately. This dataset alone cannot prove
preservation of all selected tests or satisfy the code-PR latency target.

## Reproduction and source integrity

The compressed companion contains **398 exact saved JSON records**, including
379 complete paginated job collections, three source records and one complete-log
hash with numbered excerpts. JSON byte hashes cover saved collector output;
`gh --paginate --slurp` collections are not HTTP wire-byte claims. Historical
workflow text is decoded repository-blob data. The original API run inventory,
rechecks, anomalies, per-run metrics and historical annotations are retained.

Run the accompanying standard-library program against the companion:

```sh
uv run --no-project docs/ci-evidence/2026-10-22-ci-cache-day-cohort.py docs/ci-evidence/2026-10-22-ci-cache-day-cohort.json.gz
```

It verifies every saved JSON record's bytes/hash, recomputes cohort outcomes,
all-attempt job-wall sums, excluded-duration counts and both gate-latency
admissions, then compares them with the archived analysis. It performs no network
request, package installation, build or application test. Its source is also
embedded in the companion for auditability.

Companion SHA256:
`033091353d1ff5503ef865f66871189a79dc1ed95cb6f0d86e9f43cac02dcf71`.

Remaining acceptance work is tracked in #1269, #930 and #1667: resolve the gate
admission gap, classify a matched code-only cohort, preserve actual test-case
coverage, and reconcile current cache retention/allocation against observed
consumer value. These observations do not authorize a cache budget increase or
production backend adoption.
