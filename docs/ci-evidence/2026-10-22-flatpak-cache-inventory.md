# Flatpak builder-state inventory experiment (#1269)

The retained master builder archive observed in the earlier full validation
is 2,747,488,816 compressed bytes. A later real consumer reused six dependency
modules, but neither observation identifies which state directories dominate
the archive. Do not reduce its allowance or remove module state on that basis.

The pinned action [v6.8 implementation](https://github.com/flatpak/flatpak-github-actions/blob/79327416609af08178ad73b352877e51450790b3/flatpak-builder/index.js)
uses `.flatpak-builder` as its default state/cache path and calls cache save
inside the action after builder execution, subject to `save-cache` and exact-hit
conditions. This repository preserves the same action pin, paths and guards.

The added step runs after that action, before the SDK ccache reporting/pruning
step. It records job status and GNU `du` apparent bytes, to directory depth two,
without following symlinks. It changes no cache contents. A missing state path
is explicitly marked absent; a failing build's inventory is not qualified as
a successful warm build. The report has a two-minute timeout.

These are filesystem observations after the build. They are **not** compressed
archive sizes, guaranteed reclaimable space, safe deletion candidates, or a
claim that a branch/exact-hit run published this state. GNU du counts shared
inodes once; per-directory attribution must be interpreted accordingly.

## Local RED → GREEN

RED `bc176219a` fails because the workflow has no inventory step. The GREEN
fixture executes the actual workflow shell against known downloads, object,
compiler and build directories. It verifies reported categories, unchanged
files/timestamps and directory inventory, no outside-symlink traversal, and
explicit absent-state reporting without creating a cache directory.

## Defined hosted measurement

Launch the existing full manual `ci.yml` workflow once on the pushed GREEN
source, with `native-cache-probe=false`. This preserves its complete build/test
selection and adds no new workflow or dispatch mode. The planned Flatpak job
must actually restore a master seed, build/test successfully and emit the
inventory. Preserve the run/source/job IDs and raw report before interpreting
sizes. A PR fast gate alone does not execute or qualify this full job.

No cache pruning, smaller Flatpak reservation, workflow rerun, packaged-content
change, or representative-day speedup is part of this unit. A later experiment
can target specific redundant state only if the measured contents and actual
module reuse support it. Keep watcher cancellation disabled for the intentional
full measurement, so an independent failure cannot silently erase its evidence.

Hosted Flatpak observation is recorded below. Global allocation and native
architecture retention are unresolved under #1269; this is the only selected issue.

## Observed full-run Flatpak job

The defined run [39713423365](https://github.com/obs-rust/obs-studio/actions/runs/39713423365)
on GREEN source `e8eb1327e29dcaf55796b6e504718911a24acfe3` produced successful
Flatpak job [119609912965](https://github.com/obs-rust/obs-studio/actions/runs/39713423365/job/119609912965).
It restored the 2,747,488,816-byte builder archive and 60,766,818-byte compiler
archive, reused four obs-deps modules plus CEF and the Rust toolchain, built
OBS and passed existing AppStream/build-directory/repository validation.
The logged input was `build-bundle: false`; no distributable bundle or
saved-profile runtime execution is claimed.
The report step passed. The parent full workflow was still in progress at
capture; its remaining platform jobs are not claimed successful here.

[Recorded data and literal excerpts](2026-10-22-flatpak-cache-inventory.json):

| Directory | Apparent bytes after builder |
| --- | ---: |
| Entire state | 5,670,286,365 |
| cache | 4,582,107,102 |
| cache/objects (within cache) | 4,582,106,385 |
| downloads | 1,026,740,380 |
| ccache | 61,438,581 |
| build | 0 |

Rows overlap and must not be added together. The compiler stats printed
157/360 hits, but the restored counters were not reset after the final state
restore; they are cumulative observations, **not** this-build cache-hit rates.
No pruning or smaller allowance is adopted. The object store dominates the
apparent working state, followed by downloads; determine which content is
necessary for warm reuse before designing any removal experiment.

## Separately observed restore-prefix mismatch

A [read-only adapter probe](2026-10-22-flatpak-fallback-probe.cjs) extracts the
pinned distributed action's actual Configuration/prepareBuild bodies. Its
[results](2026-10-22-flatpak-fallback-probe.json) use the observed master key
and a hypothetical changed manifest hash. The exact custom key matches; the
changed key misses because the action's fallback `flatpak-builder-x86_64`
does not prefix our `flatpak-builder-<hash>-x86_64` namespace.

This is a verified namespace mismatch using injected prefix matching, not
a real changed-manifest transport miss, build or performance measurement.
The action body SHA-256 is
`676fd5f2f7dd0116164a1db0d3abb1a8cb3f5488d764a64f5b4a8e126e05bcae`.
Reproduce after downloading the distributed file at the action pin above
and obtaining a read-only cache listing in the probe's recorded format:

```sh
node docs/ci-evidence/2026-10-22-flatpak-fallback-probe.cjs /path/to/pinned-dist.js /path/to/builder-keys.json
```

A future correction needs a same-architecture namespace and a migration that
preserves the known useful master seed. This unit changes no keys or restore
behavior. Keep that correctness/performance question separate from whether
any measured directory is redundant.
