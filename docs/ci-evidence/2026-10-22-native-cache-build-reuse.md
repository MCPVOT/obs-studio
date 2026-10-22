# Native release-cache build reuse: failed qualification (#1269)

[Run 39788036193](https://github.com/obs-rust/obs-studio/actions/runs/39788036193)
completed with **failure**, including a failed required CI Gate. It ran once,
on frozen master `cfd212d348bcdbad4a04ee5377e5a62eecd49977`, consuming the
required producer `24a96a131986ef6eeca28a07ecc74ff14586d4d2`.
There was no manual cancellation or unchanged retry.

The final API inventory contains **33 completed job entries: 17 successful,
11 skipped and 5 failed**. An earlier snapshot had 32 entries; use the final
inventory. Known non-skipped job wall time totals **9265 seconds**, with no
inverted timestamps. This API timing proxy includes setup and scheduling
uncertainties; it is not billed runner time or an optimization benefit.

## Native results

| Axis | Required-source restore | Native app build | Installed-binary freshness |
| --- | --- | --- | --- |
| Windows ARM64 | Cold: compiler identity differs | Skipped | Skipped |
| Windows x64 | Cold: compiler identity differs | Skipped | Skipped |
| macOS ARM64 | Hit; 18.528 s restore | Success; 650/660 cacheable tasks hit | Failed harness path lookup |
| macOS x86_64 | Hit; 14.764 s restore | Success; 650/660 cacheable tasks hit | Failed harness path lookup |

Windows runner images were `20261007.264.1` (ARM64) and `20261018.275.1`
(x64). Only the compiler field differed from the corresponding retained seed;
SDK, architecture, configuration and cache-format fields matched. Both consumers
reported `status: cold` with empty transport error lists. Rejecting a different
profile is required behavior. The builds did not start; these are not successful
Windows compiler-reuse observations. Do not weaken identity checks or retry
unchanged hoping for another image.

Both macOS consumers restored the exact required producer and matched their
compiler/SDK/cache profiles. Their app builds completed and compiler counters
showed 98% cache hits. The source overlay had a run-specific marker and changed
the original source hash; both cleanup reports restored the exact original
hash. These facts establish compilation using the restored cache, but **do not
prove freshness of the installed binary**.

The freshness harness looked for Linux-style executable names inside the macOS
bundle. [#1792](https://github.com/obs-rust/obs-studio/pull/1792) subsequently
corrected the path to the macOS target's explicit output name and added focused
regressions. That repair does not retroactively pass this qualification.
Native packaging and Actions compiler-cache restores/saves were skipped in this
reuse mode. Production backend adoption remains unqualified.

## Successful compatibility and application coverage

Counts below come from completed logs, not merely successful routing jobs.
Repeated suites are reported separately rather than counted as unique tests.

| Suite | Registered per execution | Passed | Skipped | CTest seconds |
| --- | ---: | ---: | ---: | --- |
| Linux C, Rust OFF | 362 | 358 | 4 | 183.67 |
| Linux hybrid, Rust ON | 362 | 358 | 4 | 184.33 |
| Windows CMake/Cargo C/hybrid, four executions | 179 | 179 each | 0 | 87.11 / 85.09 / 83.42 / 82.61 |
| macOS CMake/Cargo C/hybrid, four executions | 204 | 203 each | 1 each | 147.96 / 127.46 / 118.23 / 124.03 |
| Qt | 19 | 19 | 0 | 6.68 |

Linux skipped the Pulse monitor-device, monitor-samples, stream-restart and
two-sources tests. Each macOS execution skipped `test_svt_av1`. Recorded
case-name sets agree within each platform and between Linux OFF/ON.

The Ubuntu app job succeeded. Its actual installed app passed **7/7 GPU and
packaged-library checks**, including reporting llvmpipe without a false green
GPU claim, and **206/206 WebSocket Tier 4 checks**. Flatpak, Rust validation,
Fast Gate and cross checks also succeeded; this does not replace the failed
native freshness coverage.

## Host acquisition and retention observations

Qt dependency acquisition took **1199 seconds**, including fetching 104 MB in
19m25s and 347 `Ign` records. The job subsequently built and passed its tests.
The one-retry/15-second acquisition settings did not make this bulk acquisition
fast. Late HTTPS archive fetches succeeded; the underlying network cause is
not established. [#1794](https://github.com/obs-rust/obs-studio/pull/1794)
changes host mirror preference while retaining fallback URLs and package lists.
This run predates that change and cannot demonstrate its effect.

All four external release inventories retained the same asset IDs, names,
sizes and digests between preflight and final capture: **12 assets totaling
5,087,795,135 bytes**. This includes older ARM64 generations; it is not a
new-storage or billing estimate.

The separate Actions cache loss is attributable in part: the #1789 queue
janitor job `119844572491` deleted both qualified ARM64 Actions seeds at
13:11:38 UTC. The following #1790 janitor deleted none. The companion preserves
these logs' hashes/excerpts and cache snapshots. This does not identify the
actor responsible for the earlier Flatpak builder-cache disappearance.
No budget increase or paid storage setting was adopted.

## Evidence and disposition

The companion contains **80 exact saved JSON records**, including final run/job
inventories, four native artifact sets, compatibility case summaries, release
and Actions inventories, plus **14 complete-log hashes with numbered excerpts**.
Saved collector bytes are retained in base64 with byte counts and SHA256;
these are not HTTP wire-byte claims. It contains source restoration state;
there are no application binaries or compiler-cache archives in the companion.

Companion SHA256:
`34bc147f4b6c891ae42dc4a9de9556d3ec4a92b9233786c18dc9d573908b1d61`.

Validate each JSON record by base64-decoding its `base64` field, checking its
`bytes` length and `sha256`, then parsing it. Recompute outcomes and wall time
from `jobs-final.json`; preserve skipped entries and reject negative durations.
Log hashes identify the complete downloaded logs; only the selected numbered
excerpts are embedded.

This attempt is **rejected as a full native qualification**. The next action is
to verify the repaired macOS freshness check under an explicitly planned trial,
and disposition Windows image/profile instability without relaxing correctness.
Global Actions allocation and Flatpak retention remain unresolved with #1269,
#782 and #1760. [#1793](https://github.com/obs-rust/obs-studio/issues/1793)
separately tracks historical runner-acquisition gate admission; this run's
correct gate failure does not resolve it. No owning issue is completed here.
