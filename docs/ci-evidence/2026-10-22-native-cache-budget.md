# Windows cache budget experiment: reject 350M (#1269)

The opt-in probe from [#1681](https://github.com/obs-rust/obs-studio/pull/1681)
ran once in [39699009288](https://github.com/obs-rust/obs-studio/actions/runs/39699009288),
on `c45c3c285847426a59165ba6dc18b4f62887be84`. Both Windows architectures
completed all four full clean rebuilds. The workflow **failed overall** in
Linux ARM64 release cross-lint because target `gio-2.0.pc` is missing; that
existing setup failure is recorded in
[#1395](https://github.com/obs-rust/obs-studio/issues/1395#issuecomment-6329744841).
Every other build/test job passed. The failed CI Gate remains a failure.

The [data](2026-10-22-native-cache-budget.json) preserve both complete native
reports, artifact IDs, run/job metadata, literal test summaries/skips, Flatpak
consumer evidence and the capacity snapshot. The original artifacts expire in
two days; their measurement contents are preserved here. This manual experiment
is excluded from code-PR and merge-group latency cohorts.

## Native method and result

Each architecture used one runner (image `20261007.264.1`), the same source,
compiler and existing full Windows CMake build preset. After the ordinary app
build, four independent copies of its compiler cache were measured in fixed
`2G`, `350M`, `350M`, `2G` order. Each copy was explicitly cleaned to its limit
before its trial. The probe verified its effective cache directory, reset
counters and ran the preset with `--clean-first`. Every trial recorded cacheable
C/C++ calls. The original publication cache and production `2G` setting were
unchanged. Preparation/copy time is recorded separately from rebuild time.

| Architecture | Limit | Rebuild seconds, both trials | Median seconds | Cache misses, both trials |
| --- | --- | --- | ---: | --- |
| x64 | 2G | 197.515, 198.375 | 197.945 | 0, 0 |
| x64 | 350M | 386.875, 380.687 | 383.781 | 157, 154 |
| ARM64 | 2G | 149.204, 141.484 | 145.344 | 0, 0 |
| ARM64 | 350M | 266.938, 305.437 | 286.188 | 107, 120 |

The observed bounded/baseline median ratios are 1.94 for x64 and 1.97 for
ARM64. Each x64 trial had 289 cacheable calls; each ARM64 trial had 263.
Hits mean direct plus preprocessed hits, not direct misses plus cache misses.
The native reports preserve all counters rather than substituting a hit-rate
headline for build time. Timing includes Rust and build-system work; ccache
counters describe C/C++ only.

**Decision: reject 350M as the production limit.** It roughly doubled rebuild
time in both observed architecture samples. Do not trade away demonstrated
warm reuse merely to make retention declarations add up. Two samples per
condition, fixed ordering and one runner per architecture do not establish a
representative day, a statistical guarantee or a universal size recommendation.
Do not compare the two architectures as a controlled hardware experiment.

## Configured size is not publication size

| Architecture | Initial seed bytes | After pre-trial 350M cleanup | After bounded rebuilds, bytes |
| --- | ---: | ---: | --- |
| x64 | 1,063,412,164 | 326,929,096 | 610,156,913; 688,046,171 |
| ARM64 | 902,623,312 | 284,972,305 | 660,161,554; 516,897,038 |

The pinned ccache reports `2G` as 2,000,000,000 bytes and `350M` as
350,000,000 bytes (its machine counter reports the latter truncated to
341,796 KiB). The working directories exceeded the configured smaller limit
after rebuilding. These are recorded filesystem sizes at that observation,
not a strict instantaneous or quiescent bound, nor compressed GitHub archive
sizes. No post-build cleanup or private-cache publication was performed.
Any future publication experiment needs explicit post-build cleanup and real
archive-size measurement as well as consumer timing.

## Real Flatpak consumer

The same run's Flatpak job
[119563480263](https://github.com/obs-rust/obs-studio/actions/runs/39699009288/job/119563480263)
passed. It restored the master compiler archive
`flatpak-ccache-x86_64-f1783cf62e4f9ad34ee7abe3d00fc351d2ed4921`
(60,766,818 bytes) and the exact master builder key
`flatpak-builder-7bb71f0a48a885ca4bfe218d34814d0c108cb0ee80df664119c10c6a24945d77-x86_64`
(2,747,488,816 bytes). The builder restore ran from 00:21:41.837 through
00:22:13.440 UTC; literal log lines are preserved in the data.

The action reused six dependency modules (four obs-deps buildstream modules,
CEF and the Rust toolchain), then built OBS and completed bundle/metadata
validation. Compiler statistics report 360 cacheable calls: 161 hits and 199
misses, out of 932 total calls. Dependency/compiler restores stayed enabled;
`save-cache: false` and the skipped action save prove the branch did not
publish a replacement. This qualifies a real warm consumer of #1672's master
producer. It is not a matched cold/warm timing comparison, a saved-profile
runtime test, or proof of durable retention.

## Coverage stayed visible

Manual dispatch retained the existing full build/test selection; the probe
did not replace any lane. The Tier 2 logs report:

| Platform/pass | Registered | Executed | Environment skips |
| --- | ---: | ---: | ---: |
| Linux OFF | 353 | 350 | 3 |
| Linux ON | 353 | 350 | 3 |
| Windows OFF and ON, each | 174 | 174 | 0 |
| macOS OFF and ON, each | 199 | 198 | 1 |
| macOS Cargo C reference and Cargo hybrid, each | 199 | 198 | 1 |

Linux skips are `test_pulse_monitor_device`, `test_pulse_stream_restart` and
`test_pulse_two_sources`. Each of the four macOS passes skips `test_svt_av1`.
They are not counted as executed passes. Literal summaries, skip lines and
log hashes are preserved. No C test, public header, compiler correctness
safeguard or required gate was changed for this experiment.

## Capacity remains open

The read-only inventory captured on `5a18f14c7` listed 8,531,576,542 bytes.
Holding that entire inventory fixed and adding three missing, previously
observed native archives reconstructs 10,921,746,168 bytes: 184,327,928 bytes
above the unchanged 10 GiB budget. Their cache IDs are Windows x64 9309072322,
macOS Intel 9309009667 and macOS ARM64 9309055771; the existing Windows ARM64
entry is not added again. Their original records come from
[the prior hosted evidence](2026-10-21-full-cache-validation.json).

This is a hypothetical coexistence scenario, not an atomic observed peak or
an optimized minimum working set. It holds possibly replaceable entries
fixed, assumes no absent AppImage/sanitizer/CodeQL/cargo payloads, and adds no
growth or writer-overlap reserve. Usage and listing are separate observations.
The raw inventory, exact policy blob, pinned offline audit and missing archive
records are preserved so the arithmetic can be recomputed. Declared capacity
remains 14,688,662,322 bytes; lowering declarations alone does not establish
useful retention.

#1269 remains active for an allocation based on current producers, including
ON-only Ubuntu app production and both native architectures, post-publication
headroom and real retention under traffic. Recompute obsolete reservations,
measure the next payload/retention tradeoff, and retain the demonstrated warm
consumers. No smaller production limit, paid storage expansion, manual live
cache deletion, or measured CI speedup was adopted here. Representative-day
PR/queue latency, runner cost and retention evidence remain outstanding.

## Reproduction

The shipped opt-in path is `gh workflow run ci.yml --ref master -f
native-cache-probe=true`. It runs full validation plus eight additional clean
rebuilds, so use it for a defined experiment rather than a routine gate.
The exact commands and environment identities from this run are in each
native observation. No repeat run is needed to inspect these recorded results.
For the capacity arithmetic, sum `size_in_bytes` from the preserved inventory
and add the three preserved missing records; obtain its exact config with
`git show <capacity_scenario.inventory.revision>:ci.toml` and verify the recorded
`policy_config_blob`. The earlier pinned read-only audit helper can replay the
saved inventory/config without live deletes.
