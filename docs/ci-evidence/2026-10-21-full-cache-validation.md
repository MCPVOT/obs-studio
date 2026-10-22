# Hosted cache validation and remaining capacity gap (#1269)

One full validation ran on `f1783cf62e4f9ad34ee7abe3d00fc351d2ed4921`:
[run 39691274808](https://github.com/obs-rust/obs-studio/actions/runs/39691274808).
It **failed overall** in release cross-lint and macOS Rust tests. The six
full app jobs (Ubuntu hybrid, two Windows, two macOS, Flatpak), Qt, Fast Gate
and all Tier 2 jobs passed. This is one manual full run, not a representative
day, a code-PR/merge-group cohort, or a measured speed improvement.

The [evidence](2026-10-21-full-cache-validation.json) preserves reduced run/job
metadata, raw cache snapshots, pinned offline janitor reports, test counts
and the bounded sanitized Ubuntu journal. Cache policy snapshots use the
`ci.toml` from `d414607ef` (after the three unused families were retired);
the full producer ran on the earlier `f1783cf62`. The retired families had
no entries and no consumers; active family limits are the same. This distinction
must not become a full-run claim on the later policy revision.

## What the hosted run proves

The upgraded Flatpak action received `cache: true`, `restore-cache: true`
and `save-cache: true` on master. It built and bundled successfully, then
published these master entries, verified through the cache API:

| Snapshot | ID | Archive bytes |
| --- | ---: | ---: |
| Flatpak builder | 9309385671 | 2,747,488,816 |
| Flatpak compiler | 9309392332 | 60,766,818 |
| Ubuntu ON compiler | 9309124360 | 905,383,834 |

This qualifies the real Flatpak action and master producer, rather than only
the earlier injected-adapter probe. It does not qualify a subsequent warm
consumer or prove long-term retention. The Flatpak job validates its existing
bundle/metadata contracts; it is not an added saved-profile app runtime test.

Tier 2 retained both implementations and reported zero failed CTests:

| Platform | Registered per mode | Executed per mode | Environment skips per mode |
| --- | ---: | ---: | ---: |
| Linux OFF and ON | 353 | 350 | 3 |
| macOS OFF and ON | 199 | 198 | 1 |
| Windows OFF and ON | 174 | 174 | 0 |

Linux skips were `test_pulse_monitor_device`, `test_pulse_stream_restart`,
and `test_pulse_two_sources`. macOS skipped `test_svt_av1`. These skips remain
visible; they are not counted as executed passes. No existing C test or
assertion was edited by these cache changes.

The Ubuntu journal artifact `native-cache-ubuntu-ON-39691274808` was downloaded
before its two-day expiry. It reports status `ok`, zero malformed lines,
neither limit reached, and a successful build step. Its observed records
contain 160 hits and 193 misses. The separate compiler summary reports 353
calls: 160 cached, 177 cold and 16 noncacheable. These scopes must remain
separate; 193 journal misses are not 193 cold compilations.

## Retention remains unresolved

The in-flight snapshot listed 10,416,640,360 bytes with no Flatpak builder
entry yet. The post-run snapshot listed 10,376,532,769 bytes, including the
new 2.75 GB builder entry. Their separate usage endpoint reported
8,566,659,114 bytes in both captures. These are non-atomic observations;
they do not establish an exact peak or identify who removed an entry.

The native app producers published both architectures, but their family
allowances cannot hold the observed pair even with unlimited global space:

| Family | Observed pair bytes | Family allowance bytes |
| --- | ---: | ---: |
| Windows x64 + ARM64 | 1,964,404,299 | 838,860,800 |
| macOS Intel + ARM64 | 1,326,806,717 | 524,288,000 |

The new Intel macOS seed (9309009667) disappeared between saved snapshots.
A later native-family listing also lacks the new Windows x64 seed
(9309072322). It retains Windows ARM64 but has no macOS app Xcode entries;
the new macOS ARM64 seed (9309055771) has disappeared too. A full unfiltered
listing confirms that absence. These are observed losses,
not merely a hypothetical executor plan. The pinned offline janitor selects
the same older architecture under the pooled LRU limits after grace; that
prediction alone is not attribution of the actual deletion.

The inspected master janitor at 23:19 UTC explicitly removed the *previous*
Windows x64 generation (9283077092) after its replacement was present. It
does not record deleting either newly lost ID. Later janitors and GitHub's
own eviction remain possible actors until their logs prove otherwise.

Old target compiler/apt IDs also disappeared, but new target-family seeds
were present afterward. That is a generation change, not evidence that
those two families became empty.

Declared capacity is now 14,688,662,322 bytes against the unchanged 10 GiB
budget. Removing stale reservations and orphaned branch entries has not
resolved whole-repository allocation. Keep that budget unchanged while
evaluating bounded native compiler payloads and architecture-aware retention;
do not simply inflate it or label publication as durable reuse. GitHub
[bills expanded cache storage beyond the included allowance](https://docs.github.com/en/billing/concepts/product-billing/github-actions).

Current master has an ON-only full Ubuntu app matrix, while Tier 2 retains
OFF/ON. The previous two-app-mode historical example is not the current
producer contract. Recalculate any future Ubuntu allocation against the
active matrix and actual payload sizes rather than silently preserving an
obsolete assumption.

## Full-run failures

- Release cross-lint fails for Linux ARM64 because the target sysroot lacks
  `gio-2.0.pc`; the exact local lane reproduces it. Keep GIO code and dependency
  checks enabled and supply the proper target setup.
- macOS Rust tests fail the Objective-C rebuild/load fixture. The fresh-process
  correction is already in #1669, which was not in this run's source.

Both are recorded in [#1395](https://github.com/obs-rust/obs-studio/issues/1395#issuecomment-6328869000).
No workflow rerun, test skip, feature disablement or gate bypass was used.
The full run's independent successful jobs remain useful evidence, but its
overall result remains failure. #1269 stays open for the allocation decision,
real consumer/retention validation, and representative traffic evidence.
