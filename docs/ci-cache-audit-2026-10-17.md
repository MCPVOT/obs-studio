# CI cache audit: 2026-10-17

Implementation and measurement tracker:
[obs-studio#1286](https://github.com/obs-rust/obs-studio/issues/1286).

## Isolated C libobs build

Measured the `libobs` target alone, with Rust libobs, frontend, scripting,
browser, websocket and unit-test builds disabled. Source revisions were
`be611227b18bc9df89ad089992e6f05eda6e53ba` (immediately before #1128) and
`d145635326a4753e729a4767eb233911258ef82b` (explicit unity/PCH groups).
Both used the same Nix dependency environment, GCC 15.2, RelWithDebInfo,
eight parallel jobs, and an AMD Ryzen 7 3700X. This is a local Linux build
experiment, not a GitHub runner measurement or an application benchmark.

| Condition | Before median wall (range), s | Current median wall (range), s | Before/current median CPU, s |
| --- | --- | --- | --- |
| Compiler cache disabled, 3 trials | 9.749 (9.654–9.899) | 9.407 (9.390–9.932) | 63.482 / 46.620 |
| Warm ccache rebuild, 3 trials | 1.543 (1.541–1.556) | 1.435 (1.428–1.447) | 2.652 / 2.368 |

The uncached wall medians differ by 3.5%, with overlapping ranges; this
experiment does not establish a robust wall-time gain. CPU work fell 26.6%.
The warm wall difference is 0.108 s (7.0%). Neither result supports calling
the broader 315 → 198 s benchmark a libobs-only improvement. The revisions
also contain intervening source changes, so this comparison measures the
two actual revisions rather than isolating unity as the sole cause. Load
averages are recorded per trial; no attempt was made to monopolize the host.

With ccache 4.13.6, every measured warm trial had 125/125 cacheable hits
before and 39/39 now. The initial cache-population build is retained in the
raw data but excluded from warm medians. Warm trials remove compiler
outputs before building, so they are real rebuilds rather than Ninja
no-ops. Current PCH outputs cover `libobs`, `libobs-objects`,
`libobs-core-modules`, and `libobs-version`; the remaining group families
include their own headers textually under the build policy.

[Raw trials, compiler settings, environment and cache statistics](ci-evidence/libobs-2026-10-17/)
are committed with the report. Configure and Nix environment startup time
are excluded from build timings. Existing application build directories and
global caches were preserved; benchmark outputs and per-revision ccache
stores live under the same checkout's ignored `build_ci_audit/` directory.

### Reproduce

Use the same checkout, switching source revisions between experiments.
Save the measurement helper under `build_ci_audit/` before checking out an
older revision that predates the helper. Run configure and measurement
inside the same dependency environment, for example
`nix-shell rust/tools/sanitize/shell.nix` on this Linux host.

```sh
mkdir -p build_ci_audit
cp ci/benchmark_libobs.py build_ci_audit/benchmark_libobs.py
# Switch to the revision being measured; use a new directory per revision.
cmake -S . -B build_ci_audit/libobs-example -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOBS_BUILD_NUMBER=1 \
  -DENABLE_FRONTEND=OFF -DENABLE_SCRIPTING=OFF -DENABLE_BROWSER=OFF \
  -DENABLE_WEBSOCKET=OFF -DENABLE_RUST_LIBOBS=OFF -DENABLE_UNIT_TESTS=OFF \
  -DENABLE_AJA=OFF -DENABLE_WEBRTC=OFF -DENABLE_QSV11=OFF \
  -DENABLE_VST=OFF -DENABLE_DECKLINK=OFF \
  -DENABLE_ZCCACHE=OFF -DENABLE_CCACHE=OFF
uv run build_ci_audit/benchmark_libobs.py \
  --build build_ci_audit/libobs-example --output build_ci_audit/example-cold \
  --repetitions 3 --jobs 8 --rebuild
```

For warm-cache trials, add ccache to the environment, reconfigure the same
dedicated build with `-DCMAKE_C_COMPILER_LAUNCHER=ccache` and
`-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`, set a fresh per-revision `CCACHE_DIR`
under `build_ci_audit/` and `CCACHE_MAXSIZE=250M`, then run the helper with
`--repetitions 4 --rebuild --cache-stats`. The first trial populates the
empty cache; compare the next three. The helper requires `safe-rm` to move
only the dedicated build's compiler outputs to trash before each trial.

## Rust compiler cache: verified blocker

The pinned soldr 0.9.35 still reproduces
[zccache#1943](https://github.com/zackees/zccache/issues/1943). A nested Rust
module changed from a comment to `compile_error!`, preserving its size and
nanosecond mtime. Touching the unchanged crate root forced Cargo to invoke
rustc. The cached command returned success with a HIT; the same command
with `SOLDR_RUSTC_WRAPPER=none` failed with the injected diagnostic.

| Step | Exit | Probe error reported | Seconds |
| --- | ---: | --- | ---: |
| Warm valid source | 0 | No | 2.888 |
| Cached changed source | 0 | No: false success | 2.052 |
| Uncached changed source | 101 | Yes | 1.621 |

These durations describe a tiny correctness fixture, not a performance
benchmark. [Captured result](ci-evidence/2026-10-17-rust-cache-source-probe.json).
Keep the local/CI gate's wrapper bypass until this reproduction passes on
the version we adopt. The standalone Linux C/C++ PCH cache upgrade in
[#1265](https://github.com/obs-rust/obs-studio/pull/1265) is separate from
soldr's embedded Rust cache and does not prove this issue fixed.

Reproduce from the repository root with the same executable CI pins:

```sh
uv run ci/cache_source_probe.py \
  --directory build_ci_audit/source-probe-new \
  --soldr /absolute/path/to/soldr --expected-version 0.9.35
```

Use a new scratch directory for every run. The probe preserves logs and
`result.json`, and exits 1 when the cache fails the source-safety check.
The explicit binary and expected-version check matter: in this checkout,
`uv run` found soldr 0.9.34 on its environment PATH while the shell's soldr
was 0.9.35. Both reproduced the stale success; the table records the explicit
0.9.35 run. No cache or daemon was purged to obtain these results.

The required gate's existing source probe now exercises a nested module
edit preserving size and mtime, so it guards the workaround against this
specific failure mode rather than checking only an ordinary append.

## Timing persistence and scheduled retention

[#1288](https://github.com/obs-rust/obs-studio/pull/1288) enables hidden-file
uploads for `.tier2-cost` and rejects missing or unmeasured OFF/ON timing
files before writing a cache. Master artifact production and subsequent
consumption must be observed before declaring the timing pipeline repaired.

The [failed scheduled run](https://github.com/obs-rust/obs-studio/actions/runs/39002695404)
was rejected before job execution: its annotation says the nested `ci-mode`
job requested `actions: read` but the caller allowed none. This was a workflow
permission error, not a cache script failure.
[#1289](https://github.com/obs-rust/obs-studio/pull/1289) grants that permission
and removes the independent nightly compiler-cache purge. Cache retention
belongs to the existing merge-group/master janitor and `ci.toml`; scheduled
builds reuse its warm entries. A live full dispatch remains the startup check.

## A day of baseline CI traffic

The window 2026-10-16 17:00 → 2026-10-17 17:00 UTC contains 473 Pull/Push
workflow runs. Both workflow listings extend before the start of the window,
so the listing limit did not truncate it. Detailed jobs and actual changed
files were inspected for 117 runs, sampled across event/outcome and time,
with extra documentation candidates whose diffs were verified. These are
sample medians, not weighted population estimates.

| Successful gate cohort | Samples | Median created-to-gate time |
| --- | ---: | ---: |
| Code PR | 9 | 16m 56s |
| Code merge group | 8 | 15m 54s |
| Docs PR | 1 | 4m 01s |
| Docs merge group | 2 | 6m 58s |
| CI/config merge group | 2 | 2m 55s |

The sparse docs samples do not establish a stable typical duration.
Across the full listing, merge groups had 35 successful, 163 canceled and
6 failed runs. The sampled canceled code merge groups alone consumed
194.45 runner-minutes. That is observed sampled waste, not an extrapolated
total, and the run outcomes alone do not establish why cancellation occurred.
The window spans earlier CI/build rollouts; this baseline cannot attribute
a later difference to any single cache or concurrency change.

[Summary, outcome counts, listing coverage and per-lane medians](ci-evidence/2026-10-16_17-ci-baseline-summary.json)
and [sample IDs, diffs, gate and runner times](ci-evidence/2026-10-16_17-ci-baseline-samples.json)
are retained. Cache inventory in the summary is a snapshot at collection
time, not a reconstruction of cache contents at the historical window end.
The full raw run/job/cache responses remain under `build_ci_audit/metrics-before/`
in the measurement checkout; the collector reproduces that evidence.

`ci/cache_metrics.py` resolves source cohorts from actual compare diffs,
including an unambiguous PR branch/lifetime fallback for older runs whose
API metadata no longer contains a PR association. Missing/ambiguous diffs
remain unknown. It reports latest run attempts only and does not reconstruct
overwritten attempts. In addition, every merge-group CI Gate summary now
lists completed setup/cache step durations from the already-fetched job
metadata, without adding another API request to the gate.

Post-change measurements must use a separate window and preserve these
cohort/coverage distinctions. The day-long post-change comparison remains
open until the configuration changes have landed and that window exists.

## Unused gate compiler-cache layer

Master run [39056485478](https://github.com/obs-rust/obs-studio/actions/runs/39056485478),
Fast Gate job 117465447413, restored a 1.2 KB build-cache archive (four
files, 1.8 KB inflated) in 0.7 s. Its final session counters were zero
hits, misses and compilations; post-job save skipped the zero-compile
delta. The local gate explicitly bypasses this Rust compiler cache for
lint, tests and cross checks because of the verified correctness blocker.

Disable this layer only for deferred PR/merge-group Fast Gate runs,
retaining cook and registry snapshots. The full-day sample contains 26
PR and 18 merge-group Fast Gate session reports with no positive compiler
hits (26 and 16 respectively had zero compilations). In contrast, 5/24
master Fast Gate samples and 3/8 PR/queue cross-lint samples had positive
hits. Keep full/master and cross-lint caching rather than extrapolating
from the initial zero-counter run.

The Rust wrapper bypass does not establish that every compiler-cache
consumer is inactive: dependency cooking and C compiler work may still
use it. [Captured sample coverage](ci-evidence/2026-10-17-gate-cache-consumption.json).
This is a small measured operation to remove from the deferred path, not
a claim of a large CI gain. Cold cook may lose potential compiler-cache
reuse after a lock change; retain the published post-job cook snapshots
and measure total live setup/consumer time. Concurrent action phase
times must not be summed as sequential wall time.

## Additional cache inventory: CodeQL overlays

The cache API snapshot at 2026-10-17 18:39 UTC contained 43 entries totaling
4,389,323,635 bytes (below the 10 GB repository budget). This is a live
snapshot, not historical baseline occupancy. Qt's newest entry was
1,114,881,014 bytes, reinforcing the pending sizing correction.

GitHub's default CodeQL setup writes `codeql-overlay-base-database-*`
entries outside the repository's explicit workflows. The snapshot included
a 131,294,279-byte C++ base and Python bases around 11 MB. These keys have
no matching declared family, so the janitor treats them as unknown after
the grace window. They are consumed: PR run 39057119695, C++ job
117467259478, restored the master overlay in about 3.7 s (18:13:17.924 to
18:13:21.606). Master run 39057995138 saved a new base in about 5.1 s.

A bounded retention family merits evaluation; blanket deletion discards
observed reuse, but these logs alone do not measure the analysis speedup
against a cold run. Keep this separate from compiler-cache hit rates and
from the required merge gate, which does not wait for CodeQL.

## Post-job cook-profile comparison

The pinned action saves its cook archive **after** the job's consumers, so
a release-keyed archive can contain dev/test artifacts. A pre-consumer
snapshot is not an adequate model for disabling this cache.

The isolated local experiment used soldr 0.9.35, four Cargo jobs, identical
compiled Rust inputs and dependency environment, wrapper bypass enabled
for consumers, and post-job snapshots taken after workspace clippy and
`obs-util` tests. Each restored snapshot ran warm cook, workspace clippy
and the same tests. Policy order rotated across three trials per scenario.
A forced core rebuild touched the unchanged obs-util crate root and
required actual recompilation in the log; source bytes were preserved.

| Policy | Unchanged median (range), s | Core rebuild median (range), s | Archive bytes |
| --- | ---: | ---: | ---: |
| release cook | 57.259 (56.007–57.611) | 67.869 (67.801–90.592) | 205,118,438 |
| dev cook | 36.170 (35.849–40.269) | 82.736 (48.692–139.906) | 187,723,562 |
| no cook/archive | 72.136 (71.493–74.113) | 78.291 (73.830–79.011) | 0 |

[Raw trial results](ci-evidence/2026-10-17-soldr-cook-post-job.json) include
individual command, restore and warm-cook durations and load averages.
The totals include local restore plus consumer commands, excluding network
transfer, formatters, Dylint and post-job save. Initial archive creation
took 8.534 s for release and 7.203 s for dev. Local archives cover selected
tests and are smaller than the 637,542,999-byte live CI release snapshot;
their sizes are not predictions of full-workspace CI archives.

Dev cooking clearly wins the unchanged-input scenario here. Core rebuild
variance is large and the ranges overlap; its median favors release, so
these data do not establish a universally fastest profile. Disabling cook
loses useful post-job artifact reuse. Retain release cooking on core work
until live comparisons justify changing it, and evaluate dev cooking for
non-core Fast Gate runs with unchanged workspace inputs. Match explicit
release/static-library consumers to release; do not infer cross-target
benefits from this host-only experiment.

Reproduce using a fresh directory in the same checkout and the same Nix
dependency environment (existing target caches are left intact):

```sh
nix-shell rust/tools/sanitize/shell.nix --run \
  'uv run ci/benchmark_soldr_cache.py --directory build_ci_audit/cook-new --soldr /absolute/path/to/soldr --trials 3 --jobs 4'
```

Keep compiled sources fixed throughout. The recorded run reused completed
seed consumers at their original target paths, then modeled actual
post-job saves. A measurement assertion initially expected Cargo to label
a forced rebuild only “Checking”; Cargo correctly used “Compiling”. The
assertion was corrected, that attempt's logs were preserved, and only the
remaining unrecorded trials resumed. All published trials passed.


## Dylint setup-soldr feature audit

Fast Gate currently invokes `soldr cargo dylint` after stable-profile
clippy. In master run 39056485478, job 117465447413, catalogued Dylint
tools/driver downloaded from 18:05:40.654 to 18:05:41.944. Nightly
`rustc-dev`, `rust-src` and `llvm-tools` preparation then lasted until
18:06:06.119, roughly 24 seconds. The repo plugin pins
`nightly-2026-05-28`; caching that foundation is a plausible opportunity.

However, pinned setup-soldr b782f606a2f3beadc35b9e3bf5b4565e005f52ed
[resolve-setup.ts](https://github.com/zackees/setup-soldr/blob/b782f606a2f3beadc35b9e3bf5b4565e005f52ed/src/lib/resolve-setup.ts)
forcibly sets `inputs.prebuildDeps = "none"` whenever `dylint: true`,
including explicit cook inputs. That mode assumes a Dylint-only job; it
is not a drop-in improvement for this mixed stable-check/test/Dylint job.
Its foundation cache also owns exact nightly paths, while legacy
`dylint-cache: true` alone does not include those paths by default.

**Decision:** retain the current mixed-job mode and ordinary cook reuse.
Do not enable `dylint: true` indiscriminately to maximize feature count.
The measured post-job cook benefit would be lost, and the network/restore
tradeoff of a separate foundation has not yet been measured. A dedicated
Dylint lane or an explicit mixed-job foundation implementation would need
its own setup/restore/consumer/save benchmark, required-gate integration,
nightly/target identity checks and bounded retention before adoption.
No workflow or cache family was added for this rejected rollout.
