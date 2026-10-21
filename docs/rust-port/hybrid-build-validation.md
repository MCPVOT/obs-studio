# Hybrid default validation (#1587)

Implementation PR: [#1588](https://github.com/obs-rust/obs-studio/pull/1588).
The developer default is hybrid libobs. Release approval remains pending;
this report does not waive release-criteria.md or authorize publication.

## Build timing

[Raw commands and measurements](benchmarks/hybrid-build-2026-10-20.json)
compare baseline `90e4edc41` with hybrid implementation `174109fad`. Both use
one Linux dependency image, GCC 15.2.0, Rust 1.98.1, CMake 4.2.3,
RelWithDebInfo native code, release Rust code, and four compilation jobs on
a Ryzen 7 3700X. Each variant starts with a separate empty compiler cache and
fresh output tree; crate downloads can be warm. Configuration is timed
separately. The baseline builds the two default libobs products (`libobs`,
`libobs-with-rust`); the hybrid builds only production `libobs`.

| Scoped libobs operation | Baseline | Hybrid |
| --- | ---: | ---: |
| Configure | 8.223 s | 7.544 s |
| Fresh outputs and initially empty compiler cache | 11.406 s | 14.041 s |
| No changes, median of three runs | 0.115 s | 0.120 s |
| Touch generated configuration header, warm cache | 7.970 s | 5.995 s |
| Append a comment to that header | 11.184 s | 5.779 s |

The fresh build was slower; the warm and edited-header rebuilds were faster
in these samples. The shared host was busy (load averages are recorded), and
there is only one sample for each rebuild type. These figures cannot establish
a general build-speed improvement. They exclude frontend/plugins, packaging,
image setup and full-application build time. An earlier exploratory pass used
a shared compiler cache and is deliberately excluded from this comparison.
The raw child-CPU fields exclude detached cache-daemon work and are not total
compiler CPU measurements.

To reproduce, create baseline and hybrid source snapshots, mount them read-only
as `/baseline` and `/source` in the recorded image, and use separate `/build`
output and compiler-cache directories for each variant. Execute the recorded
configure/build commands. After the fresh build, run the same build three times;
then touch its generated `config/obsconfig.h` and rebuild. Finally append an
otherwise inert comment to that header and rebuild. Keep the same toolchain,
profile, job count and source snapshots; record host load and all outcomes.

## Compatibility and test time

- The real Linux production and C-reference libraries export identical sets
  of **1,766 symbols**. Their SONAMEs match. The Ninja graph confirms all
  **61 selected ported C implementations** are absent from production and
  present in the explicit reference.
- With HEVC disabled, a separate real build exports **1,762 identical symbols**,
  with no HEVC symbols leaking from the aggregate Rust archive.
- The unchanged C suite registered **341 tests per backend**, with no failures
  and the same three hardware-dependent skips. Loader-path checks verified
  the C reference for OFF and production hybrid for ON. Wall times were
  **392.31 s** and **390.62 s**, respectively. These are backend runs of the
  same suite, not before/after test-speed evidence.
- The full Rust workspace unit, differential and documentation tests pass.
  The opt-in native integration category also passed with `libobs-integration`:
  The normal native production entry point also built a hybrid with 1,766
  exports and no C-reference output. The explicit pair ran
  **59 unchanged C executables per backend** (118 runs), selected-library
  loader checks, matching exports, data lifecycle and plugin checks. This run
  disabled per-target native sanitizers; the separate CMake Tier 2 run kept
  its existing sanitizer configuration.
  An obsolete NVENC test requiring the installed C loader was updated to
  require the production Rust loader and explicit C reference instead.
- `./lint` passed after the platform/packaging fixes (60.5 s); subsequent native
  validation fixes and final PR results are recorded on the PR.

Compatibility test coverage has not been reduced to create a speed claim.
Ordinary application builds avoid the C reference; an explicit compatibility
run still builds and exercises both implementations. No test-time reduction
is established by these measurements.

## Packaged application and remaining release evidence

A production glibc-2.28 AppImage built successfully through the existing bosn
`appimage-228` task, including the unchanged C++ websocket plugin. Its first
attempt encountered a compiler-cache daemon outage; the successful retry
used four compilation jobs with caching disabled and reused completed objects.
This is functional build evidence, not a clean full-app timing comparison.

The AppImage passed the saved-profile scenario on Debian 13: screen capture,
camera and microphone reached streaming state and produced a recording. The
recording verifier decoded 437 frames at 320x240 (14.6 seconds) in the first run,
found no screen/camera freezes, and measured the 440 Hz microphone tone without
silence. A second run also passed and retained `/proc` maps proving libobs loaded
from inside the extracted package. AppImage SHA-256:
`218f8f17afd4eb51daebb913a51983778e4f27166abe5d1b4f849995e3d6584a`.

The unchanged websocket suite passed **206/206 checks**, exercising all
**147 documented request types**, against the real executable extracted from
that artifact. Running the suite against the AppImage wrapper initially gave
three process/library false failures: its PID checks found the wrapper rather
than the OBS executable. No protocol assertions were weakened. The saved-profile
scenario separately exercised the actual AppImage entry point. SRT reconnect
checks requiring a host ffmpeg with SRT support were skipped in that container.

An explicit C rollback build also linked successfully without the libobs Rust
archive. Substituting that library into a copy of the same AppDir and rebuilding
the AppImage made the packaged saved-profile check fail with
`packaged libobs does not contain the production Rust implementation`. This is
the negative control for the shipping-library regression; the C reference tests
remain valid independently.

Verification-only CI exposed two integration problems, now fixed: the macOS
script still explicitly requested the removed `libobs-with-rust` target, and
Flatpak debug extraction corrupted the prebuilt Rust compiler's ELF libraries.
The compiler module now disables stripping and debug extraction, as described by
the [Flatpak build options reference](https://docs.flatpak.org/en/latest/flatpak-builder-command-reference.html).
Local locked Cargo vendoring also succeeded and its generated configuration parsed.
A further cold-CI run found rustup's welcome text in soldr's stdout before Cargo's
TOML. Preparation now separates raw console output, extracts source configuration,
and validates the rewritten TOML before entering Flatpak. A focused cold-output
regression failed before that fix and passes afterward.

## Platform CI evidence

[PR CI 39531640340](https://github.com/obs-rust/obs-studio/actions/runs/39531640340)
passed at implementation commit `4ea443714da0befcaf79418d5046e8fb6766e1e4`,
including the Linux shards, macOS/Windows Tier 2, Rust tests and cross lint.
The platform logs establish:

| Compatibility host | Tests per backend | Matching exports | C / hybrid wall time |
| --- | ---: | ---: | ---: |
| Windows x64 | 165 | 1,794 | 77.40 / 77.09 s |
| macOS arm64 | 191 | 1,877 | 124.13 / 124.10 s |

[Verification-only run 39531699676](https://github.com/obs-rust/obs-studio/actions/runs/39531699676)
built Linux, Windows x64/arm64 and macOS x64/arm64 packages, the AppImage and
offline Flatpak. Native packaged startup passed on both Windows and both macOS
architectures. The first attempt also passed AppImage checks on NixOS, Arch and
Ubuntu. Two independent failures require targeted retries: the known real-time
VLC audio timing flake [#838](https://github.com/obs-rust/obs-studio/issues/838)
in the C-reference Linux shard, and an AlmaLinux ImageMagick mirror download
failure before its bundle scenario. The targeted C-reference shard retry passed
without code changes. AlmaLinux's targeted retry also passed its bundle and
first-start scenarios. The verification workflow finished **successfully** after
these two targeted retries; no assertions were changed to accommodate either
failure. It ran with `verify_only=true` and published no release.

Hardware recording performance and fourteen consecutive daily stability
builds remain separate release requirements. The checked-in release evidence
remains unapproved, and both publication workflows fail closed until its review
is recorded.

The native-engine compatibility run additionally exposed stale test-build
metadata: a missing output-delay accessor stub and an old 52-test expectation
for a manifest that now lists 59. The metadata/count are synchronized without
changing C test bodies. Static archives precede shared dependencies, and the
selected libobs remains a declared runtime dependency even for standalone C
characterizations with no core imports. Explicit comparison helpers reject
identical backend paths, preserving C-versus-Rust validation.

The unrelated Windows `cm_print_error` link failure is fixed upstream by
[#1591](https://github.com/obs-rust/obs-studio/pull/1591), tracked as
[#1580](https://github.com/obs-rust/obs-studio/issues/1580). Final platform
verification incorporates that upstream fix rather than rewriting the C test
in this migration.
