# Second hosted native-cache qualification (#1269)

[Run 39775431768](https://github.com/obs-rust/obs-studio/actions/runs/39775431768)
used frozen master `24a96a131986ef6eeca28a07ecc74ff14586d4d2`, after #1781
and #1782 merged. All four native app builds, local archive checks,
publications and required-source transport restorations passed.

The **whole run concluded cancelled**: Qt dependency installation exceeded its
existing 30-minute job limit, and `CI Gate` failed. Qt compilation and tests
were not reached. No manual cancellation or unchanged retry was performed.
Production backend selection is unchanged.

## Transport results

The consumers ran on Linux and verified archives; they did not compile native
applications using those archives. Every report has `status: hit`, the required
source SHA above and an empty error list.

| Axis | New gzip archive bytes | Publisher packing (s) | Publisher CLI total (s) | Reader elapsed (s) |
| --- | ---: | ---: | ---: | ---: |
| Windows x64 | 1,014,682,434 | 40.828 | 278.985 | 37.890 |
| Windows ARM64 | 852,264,560 | 27.360 | 204.140 | 19.522 |
| macOS x86_64 | 748,992,264 | 22.752 | 80.017 | 32.571 |
| macOS ARM64 | 731,223,038 | 26.430 | 61.367 | 18.519 |

Publisher totals include local validation and publication. They are not isolated
upload times or complete job durations. Reader elapsed time includes inventory,
download, verification and restoration; runner checkout/setup is additional.
These observations are not a controlled comparison of operating systems or a
causal end-to-end speedup estimate.

All four exact Actions seeds were observed under keys binding source,
architecture, configuration and compiler/SDK/cache profile. The Windows images
crossed between producer and copier:

| Axis | Producer image | Copier image | Different identity fields |
| --- | --- | --- | --- |
| Windows ARM64 | 20261018.275.1 | 20261007.264.1 | compiler |
| Windows x64 | 20261007.264.1 | 20261018.275.1 | compiler |

Publication reports match the producer profiles. The copier performs no native
compilation. Actual native consumers must still validate their own environment;
these successes do not qualify compilation with a different profile.

Four cache-only prereleases contain **12 assets / 5,087,795,135 external bytes**.
This includes the two older ARM64 generations. New payloads total
3,347,162,296 bytes; their four manifests add 2,547 bytes. Inventory matches the
publication reports and remains within the experimental protocol ceilings.
No allocation or paid-storage setting was changed.

## Producer cache conditions

These compiler observations belong to builds using the existing Actions route:

| Producer | Compiler observation |
| --- | --- |
| Windows x64 | 0/287 cacheable calls hit; 186 uncacheable calls |
| Windows ARM64 | 259/261 cacheable calls hit; 163 uncacheable calls |
| macOS x86_64 | 648/660 cacheable tasks hit |
| macOS ARM64 | Compiler counters not found; unknown |

Repeated printed counters are not summed. These hits are not release-backend
compiler reuse. The sources and cache conditions differ from earlier trials.
Actions seed archive sizes also differ from the gzip protocol payload sizes.

## Completed compatibility coverage

All case completion records were checked against each CTest summary. Case-name
sets match between Linux OFF/ON and across the four suites on each native host.
Executed counts below count passed completion records; skips remain separate.

| Coverage | Registered per suite | Executed per suite | Skipped per suite | CTest seconds |
| --- | ---: | ---: | ---: | --- |
| Linux OFF / ON | 362 | 358 | 4 Pulse cases | 185.66 / 187.95 |
| Windows, four CMake/Cargo C/hybrid suites | 179 | 179 | 0 | 83.97 / 82.99 / 81.73 / 81.82 |
| macOS, four CMake/Cargo C/hybrid suites | 204 | 203 | 1 SVT case | 139.46 / 120.43 / 123.17 / 113.04 |

Rust test jobs and the other selected builds passed. Their original summaries
are retained in the companion. Overall outcomes were **28 success, 9 skipped,
1 cancelled and 1 failure**, across 39 jobs. The cancelled job is Qt; the failed
job is `CI Gate`. Summed observed job wall time was 10,626 seconds, including the
cancelled work; this is not metered billing or queue wall time.

## Qt setup timeout

[Qt job 119813202156](https://github.com/obs-rust/obs-studio/actions/runs/39775431768/job/119813202156)
spent 1,801 seconds in `Install Build Dependencies`, until cancellation.
GitHub's annotation states that the job exceeded its maximum execution time of
30 minutes. The subsequent cache-report step also exited 127.

The dependency log contains repeated ignored fetches from the Azure Ubuntu
mirror and later successful fetches, including an HTTPS Ubuntu fallback. The
underlying network cause is not established. Dependency acquisition needs a
focused diagnosis; increasing the timeout or omitting packages is not a
validated performance improvement.

Two earlier successful Fast Gates provide separate setup observations:

| Run / event | Complete apt step | Rounded package-transfer summary |
| --- | ---: | --- |
| 39772110797 / PR | 275 s | 3053 kB in 4m14s |
| 39773235556 / merge group | 313 s | 3053 kB in 4m58s |

These are one observation per event, not a representative sample or a controlled
comparison. Index refresh, package transfer and complete step runtime differ.

## Actions storage and retention remain unresolved

| Collected snapshot | Entries | API-reported bytes |
| --- | ---: | ---: |
| Trial start | 38 | 9,257,420,132 |
| First two native producers | 41 | 11,324,554,250 |
| All native producers | 34 | 9,260,613,722 |
| After publication | 38 | 9,771,406,277 |
| Terminal collection | 40 | 10,143,090,565 |

The largest collected snapshot exceeds the repository's 10 GiB reference. This
is not a continuous peak trace. Nineteen entries appeared and seventeen were
absent at final collection; concurrent repository work is included.

Flatpak builder entry **9336855619**, key
`flatpak-builder-x86_64-508d72a1704f060f3e4925fa397583a664b08980c3bba2f84dc43f74a7d7a7dc-x86_64`,
was initially present at 2,717,224,066 bytes and absent by the all-producer
snapshot. Its deletion actor is unproven. All four qualification seeds remained
visible at terminal collection. Neither that presence nor successful transport
proves retention under subsequent janitor traffic or global budget sufficiency.
Held candidate #1723 remains unadopted.

## Evidence and remaining work

The compressed companion preserves **86 exact saved JSON records** and hashes,
including artifact bytes, API collections, inputs and separately marked derived
analyses. It also preserves complete-log hashes and numbered excerpts for
23 logs. Windows artifact CRLF bytes are hashed before JSON parsing; hashes are
not computed from display-normalized text. Some API collections use the `gh`
pagination wrapper and are not HTTP wire-byte claims. Complete per-suite case
records and cache snapshot differences are included.

Remaining work under #1269/#1667:

1. Diagnose and repair package acquisition, preserving required dependencies
   and the complete Qt build/test path. Do not retry this whole run unchanged.
2. Qualify actual native compilation with restored release data, current
   compiler/SDK/configuration checks and changed-source freshness evidence.
   Measure whole cache/build costs against an appropriate control.
3. Reconcile allocation and staging, including useful Flatpak retention and
   real janitor activity. External bytes remain separately accounted.
4. Publish representative PR/merge-group latency, cost and executed/skipped
   coverage with explicit sample sizes and unmatched conditions.

This report establishes four-axis publication and transport restoration. Native
compiler reuse, full validation, production adoption and backlog completion
remain unproven.
