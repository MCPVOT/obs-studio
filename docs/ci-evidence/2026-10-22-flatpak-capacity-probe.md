# Optional Flatpak capacity experiment (#1269)

The trusted producer published a2,748,017,078-byte builder archive. Its
working downloads directory is about1,026,740,380 apparent bytes, and the
builder state also carries compiler entries published separately. These
observations alone do not prove a safe or beneficial exclusion.

The existing full manual workflow accepts `flatpak-cache-probe`, default
false. It adds a bounded probe **after Flatpak output validation**, preserving
all full-build/test selection and existing production cache flags, keys,
publication guards and limits. No new workflow file or pruning is introduced.
The native-cache-probe input remains independent and false for this trial.

## Measurements

`ci/flatpak_cache_probe.py` records installed builder/zstd versions and:

1. Copies downloaded sources into an isolated temporary warm state. Measures
   `flatpak-builder --download-only` with the actual prepared manifest/SDK.
2. Measures the same acquisition command against a separate fresh state.
   Both build-directory arguments are temporary; neither command receives
   the original state directory. Copy/setup time is outside the acquisition
   timers, so the pair is not total restore/build latency.
3. Streams canonical POSIX tar through zstd and counts compressed bytes for
   full state, excluding downloads, and excluding downloads plus compiler
   state. No compressed archive files are written. These are **proxy sizes**,
   not the pinned Actions cache implementation's actual archives/transport.

Acquisition commands are bounded by GNU timeout at180seconds, compression
pipelines at120seconds, with a ten-second kill grace. The optional step has
a15-minute deadline. Temporary copies are cleaned on normal/error paths.
Scratch inside original state and symbolic links in downloaded sources are
rejected rather than permitting recursive compression or writes through a
copied link. Original state is read only for copying and compression; no
original files are moved, deleted or recompressed. Reading may affect atime.

A command failure remains a failure. The CLI writes a failed-status JSON
and raises; the measurement artifact is uploaded after failure too. No
continue-on-error bypass is used. Seven-day artifact retention applies only
to the small measurement JSON, not a relocated binary cache backend.

## RED → GREEN / local verification

RED `f1062b947` fails the absent helper and absent opt-in wiring. Fixtures
execute real GNU tar/zstd/timeout with a local acquisition executable. They
check the exclusions' measured byte differences, copied-state isolation,
original contents/mtimes, scratch cleanup on failure, opt-in defaults and
placement after output validation. Additional boundary RED evidence rejects
scratch inside the original state before the guard is implemented.

CI tests load the helper through standard-library importlib, following the
repository's standard-library-only rule. No Rust/C source or packaged content
changes; no artificial port tiers are claimed for an infrastructure probe.

## Defined hosted trial and adoption gate

After #1711's currently active new-namespace consumer39720695807 completes,
run exactly one full workflow on the pushed probe GREEN branch with
flatpak-cache-probe=true and native-cache-probe=false. Preserve the resolved
source, job IDs, command logs, versions and JSON. Do not rerun a full workflow
for an unrelated failure. Keep watcher cancellation disabled for the trial.

The hosted result below is one paired observation. One source-fetch pair does
not establish representative network cost or an end-to-end speedup. It also
does not prove a pruned state can rebuild the application: any adoption needs
actual changed-cache producer/consumer build/output validation, required
coverage and an honest global allocation. Native architecture retention and
repository headroom remain unresolved under #1269; no smaller native limit
or production exclusion is adopted by this unit.


## Observed single hosted experiment

Run [39722540093](https://github.com/obs-rust/obs-studio/actions/runs/39722540093)
on `3531d3d6796c90f29bc68f29e86ce6183cf935a4` completed Flatpak
job119639155538 successfully, including existing output validation followed
by the optional measurement. Full parent status was still in progress at
this capture. The [hosted record](2026-10-22-flatpak-capacity-hosted.json)
retains job metadata, actual JSON, log hash and command/source excerpts.

Installed tools: flatpak-builder 1.4.9, zstd 1.5.7. Downloads apparent bytes:
1,026,740,380. Isolated warm source acquisition took 0.046 seconds; fresh
acquisition took 26.691 seconds. Copy/setup is excluded from those timers.

| Canonical compression proxy | Bytes | Compression seconds |
| --- | ---: | ---: |
| Full state | 2,703,989,420 | 42.675 |
| Without downloads | 1,684,447,411 | 12.606 |
| Without downloads or compiler state | 1,624,465,624 | 11.701 |

The full-to-no-downloads proxy difference is 1,019,542,009 bytes; excluding
compiler state too differs by 1,079,523,796 bytes. These are matched proxy
differences on this state, not measured Actions upload/restore savings.
One fresh network observation does not establish representative acquisition
cost, and download-only success does not prove an application rebuild from
a pruned cache. Existing full build/output validation happened before the
probe with the original state.

Decision: retain the measurement helper, but do not adopt a production
exclusion from this observation alone. Next validate an actual isolated
pruned-state build and cache producer/consumer behavior before changing
production transport. Global allocation and architecture retention remain
unresolved; no end-to-end speedup or completed #1269 claim.


### Later selected-test coverage

The same full experiment completed all selected build/test jobs successfully.
Linux OFF/ON each registered 354 tests, executed 351, skipped three Pulse
cases and failed zero (168.21/168.65 seconds). Windows completed four
174-test CTest passes plus its Rust differential test. macOS completed four
199-registered/198-executed passes with one SVT-AV1 skip each, plus its Rust
differential test. Corresponding log hashes/excerpts are in the hosted record.
The final parent gate was still queued at this follow-up; full workflow
success remains unclaimed until that gate is observed. The earlier failed
namespace consumer remains a separate recorded observation in #1711.


### Terminal result

Full run39722540093 subsequently completed **SUCCESS**, including its
required final gate. Terminal API run/job metadata is appended to the hosted
record; earlier capture-time status remains preserved. This completes the
defined measurement trial, not production transport adoption, representative
cost/durability evidence or the broader #1269 requirements. No workflow
retry was used.
