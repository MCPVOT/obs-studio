# Rust implementation release criteria

This is the decision for #141. A port is **complete** when the applicable
four testing tiers in [testing-policy.md](testing-policy.md) pass. It is
**shipped** when a release artifact uses that Rust implementation by default.
The normal developer/application build now links and installs the hybrid
`libobs`: completed Rust ports plus unported legacy C/C++. The C reference is
explicit testing infrastructure (`OBS_BUILD_C_REFERENCE=ON`), excluded from
normal builds and installation. `OBS_LIBOBS_BACKEND=C` is temporary rollback;
product identity (`OBS_BRAND_RUST`) does not select an implementation.

## Release approval for the hybrid default

Changing developer defaults does not satisfy the production release gates.
Publishing from `release-rust.yaml` or the AppImage tag workflow requires a
reviewed `hybrid-release-evidence.json`, checked by `ci/hybrid_release_gate.py`.
It starts **unapproved**. Candidate builds and validation may run while approval
is pending; publication fails closed. Keep the first-release C rollback path.

A maintainer review must establish every requirement below on the recorded
release candidate, link the platform compatibility, application and performance
artifacts, record at least fourteen consecutive successful stability dates,
and review release blockers before approving that evidence file. Merely filling
in URLs is not proof: the review must inspect their coverage and results.

1. **Compatibility:** Tier 2 passes with OFF and ON on Linux, macOS, and
   Windows, on both x64 and arm64 wherever this project ships that platform.
   Both configurations export the same public libobs symbols and pass the
   unchanged C tests. Record the CI runs and release-candidate commit.
2. **Running application:** a full ON app builds and launches on every
   supported release platform. The unchanged obs-websocket Tier 4 suite
   passes with the existing C++ plugin against C-reference and hybrid libobs. A headless smoke run loads a
   scene collection containing a text source, records five seconds, and
   exits cleanly. Record artifacts and logs, not just a green job summary.
3. **Performance:** record the C and Rust results from #142 on the same
   machine, toolchain, build profile, and input set. No tracked hot-path
   benchmark may regress by more than 10% in median time or throughput;
   the full-app recording workload may not regress by more than 5% in frame
   time or dropped-frame rate. Repeat each benchmark enough times to report
   a median and its run-to-run range. A regression over a bound blocks the
   flip until fixed or explicitly accepted in a separate issue with the
   measured user impact.
4. **Nightly stability:** run ON release-like artifacts on every supported
   release platform for 14 consecutive daily builds. Each build must pass
   the startup, scene-load, recording, and clean-exit smoke run. Archive the
   logs and crash reports. A new crash signature attributable to the Rust
   build or a failed smoke run resets the 14-day count after its fix. Review
   open release-blocking issues before the flip.

If a platform cannot meet these gates, release approval remains pending. Do
not infer success from another platform's runs or waive a gate by changing
the developer build default.

## Record C/Rust benchmark evidence (#142)

The Criterion suite lives in the test-only `obs-c-oracle` crate. It compares
production C ABI shims with the original C sources, compiled in the same
optimized benchmark executable. Run the fixture and ownership checks first:

```sh
soldr cargo bench -p obs-c-oracle --bench port_comparison -- --test
```

Record five complete runs and their median times, run-to-run ranges, and
per-run confidence intervals:

```sh
uv run rust/tools/benchmarks/record.py --cpu 7 --output baseline.json
```

`--cpu` pins the runner on Linux; omit it on systems without that affinity
API. The report records the CPU, affinity, OS, Rust/LLVM and C compiler
versions, relevant environment flags, command, and a SHA-256 fingerprint of
the benchmark and implementation sources. The recorder refuses incomplete
paired measurements and source changes during measurement. Criterion's raw
samples remain under `target/criterion/<benchmark>/<baseline>/`.

The suite covers all exported bmem and vec2/3/4 shims, CRC at 64/4096/65536
bytes, dstr concatenation/safe formatting/replacement at 128/4096 bytes,
and the AVC/HEVC/AV1 parser entry points on structured synthetic packets.
AVC start-code cases scan a 256/16384-byte prefix. Packet comparisons include
the allocation's reference-count prefix and metadata. Allocating batches
hold at most 64 live outputs; allocation/free and reallocation have separate
timings. Setup and output cleanup for batched string/reallocation operations
are excluded, while codec timings include output cleanup.

Variadic `dstr_printf` remains C in both OFF and ON. Its two control cases
are labeled `printf_control` and excluded from Rust regression decisions.
The C string and codec oracles use the shared libobs allocator; bmem cases
compare the independent original C allocator with the Rust allocator. The
standalone oracle uses Cargo's optimized C build, without the full OBS C IPO
configuration. These measurements describe shim costs and synthetic inputs;
they do not establish full-application performance or caller inlining.

The report flags medians above the existing **1.10 Rust/C time ratio**. That
classification is descriptive, not a claim of statistical significance.
Inspect confidence intervals and run-to-run variation, and reproduce on
release hardware before making a default-flip decision. The 5% recording
workload bound and the other release gates above still apply.

### Recorded baseline

The [2026-10-14 baseline](benchmarks/baseline-2026-10-14.json) records five
runs of 70 paired comparisons on a Ryzen 7 3700X, pinned to CPU 7, using
Rust 1.98.1 and GCC 15.2.0. Twenty-one measured medians exceed the 10%
bound. The largest is `dstr/safe_printf/4096`: 59.47 times the C median,
with paired run ratios from 57.96 to 67.83. These results identify work
needed before the default flip; they do not satisfy its performance gate.

### Cross-language LTO investigation

The Linux x86_64 ON probe used Rust 1.98.1 (LLVM 22.1.8), Clang/LLD
22.1.2, ThinLTO on C/C++, and Rust linker-plugin LTO. This follows the
[rustc cross-language LTO instructions](https://doc.rust-lang.org/rustc/linker-plugin-lto.html).
With the usual OBS Linux build dependencies installed, reproduce with:

```sh
printf '#!/bin/sh\nexec soldr cargo "$@"\n' > /tmp/obs-cargo-soldr
chmod +x /tmp/obs-cargo-soldr
export RUSTUP_TOOLCHAIN=1.98.1
export RUSTFLAGS='-Clinker-plugin-lto -Crelocation-model=pic'
cmake -S . -B build-lto -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-22 -DCMAKE_CXX_COMPILER=clang++-22 \
  -DCMAKE_C_FLAGS=-flto=thin -DCMAKE_CXX_FLAGS=-flto=thin \
  -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld-22 \
  -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld-22 \
  -DRust_CARGO=/tmp/obs-cargo-soldr \
  -DENABLE_FRONTEND=OFF -DENABLE_UNIT_TESTS=ON \
  -DENABLE_BROWSER=OFF -DENABLE_WEBSOCKET=OFF -DENABLE_AJA=OFF \
  -DENABLE_WEBRTC=OFF -DENABLE_NVENC=OFF -DENABLE_FFMPEG_NVENC=OFF \
  -DENABLE_QSV11=OFF -DENABLE_VST=OFF -DENABLE_SCRIPTING=OFF \
  -DENABLE_VLC=OFF -DENABLE_NEW_MPEGTS_OUTPUT=OFF
cmake --build build-lto --target libobs -j4
```

The stock configuration stops on Clang warnings treated as errors:
double promotion in `obs-data.c` and a missing format attribute in
`graphics.c`. To investigate the linker independently, the experiment
added `-Wno-error=double-promotion -Wno-error=missing-format-attribute` to
both C/C++ flag strings. This reached the shared-library link, which failed:

```text
ld.lld: error: relocation R_X86_64_TPOFF32 against thread_graphics
cannot be used with -shared
```

The linker command included `-fPIC` and `-shared`; inspected C and Rust
bitcode both recorded PIC level 2. Repeating with
`--target=x86_64-unknown-linux-gnu` in both C/C++ flag strings to match
Rust's target triple produced the same relocation failure. Its cause is
unresolved. Cross-language LTO is therefore not enabled by this change,
and no LTO performance improvement or successful full ON build is claimed.
The warning downgrades were confined to the probe. A future LTO change
must resolve the build failures and pass the normal compatibility gates
before its performance can be compared with this baseline.

## Record full-app recording evidence (#1305)

The 5% recording bound needs runs of the real application on release
hardware, not shim benchmarks. Use a hardware renderer (never llvmpipe or
Xvfb), one saved scene and profile, the same build for both backends (the
explicit C reference or installed hybrid `libobs`, selected by the
library path), and no profiler in the timed runs.

1. Before each run, OBS must be running with the backend under test. The
   run checks the OBS log and the mapped libraries first
   (`ci/recording_evidence_check.py`) and refuses a software renderer, the
   other backend or libraries from outside `--prefix`.
2. Record one run:

   ```sh
   uv run rust/tools/benchmarks/recording.py run --log <OBS log> --pid <OBS pid> \
       --libobs rust --prefix <install prefix> --encoder obs-nvenc --seconds 120 \
       --ws-config <obs-websocket config.json> --delete-recording --output rust-1.json
   ```

3. Restart OBS with the other backend and record again. Interleave the
   backends (C, Rust, C, Rust, ...) until each has at least five runs.
4. Compare:

   ```sh
   uv run rust/tools/benchmarks/recording.py report c-*.json rust-*.json --markdown report.md
   ```

The report refuses runs with different conditions (video settings,
recording profile, scene, source version, renderer, graphics module) or
two builds of one backend. It gives the median and range of the frame
render time, CPU, and the render, output and encoder skip rates per
backend, and the Rust/C ratio. Exit code 2 means the gate failed. Each run
JSON also keeps the raw per-second samples and the encoder log lines.

## Retire the C implementation of a unit

Keep the original C file while reference testing and temporary rollback need it.
After one stable hybrid release, a separate PR may retire the
C fallback for a unit only when the supported C rollback build remains linkable
through another implementation or the C rollback option itself is retired. That PR
must show the unchanged C tests, public symbol/layout checks, and Tier 3
parity on the supported configurations. Move the original C source to
`rust/obs-c-oracle/legacy/` in the same PR, update the oracle wrapper to use
it, and keep differential tests running. Preserve the file's upstream
copyright and license notices. Removing a production C file must not silently
remove the behavioral baseline used to test its Rust replacement.

## Certified releases (in-app updates)

Every daily beta from `release-rust.yaml` is published as a GitHub
**pre-release**, after the release gate and every platform build pass. It is
never marked Latest, and the in-app updaters never offer it.

A **certified** release is a daily beta a maintainer promotes to a full
release:

```sh
gh workflow run release-rust.yaml -f certify=obs-studio-rust-v<version>
```

The `certify` job accepts only a published `obs-studio-rust-v*` release
(published only after the gate). Its "Check the release" step runs
`ci/update_feed.py check-certifiable` (#1757), which refuses the tag unless:

- the `release-rust.yaml` run that published it (the newest run for the
  tag's commit whose "Publish pre-release" job succeeded) concluded success,
  with every job successful. Only the certify job and a reusable workflow's
  own jobs (`Release gate 🚦 / ...`, `AppImage 📦 / ...`) may be skipped;
- the release has been out at least `min_age_days` days (default 2), so a
  daily gets two days of use as a beta first. For an emergency,
  `-f min_age_days=0` skips the age check and logs a warning.

It then checks its assets against `SHA256SUMS`, and
runs `ci/update_feed.py`. That script writes the Sparkle appcasts
(`updates_<arch>_v2.xml`, DMGs signed with `OBS_RUST_SPARKLE_PRIVATE_KEY`),
the Windows `manifest.json` (ZIP URL, SHA-256 and size) and `branches.json`,
both with `.sig` files signed by `OBS_RUST_UPDATE_RSA_PRIVATE_KEY`. The job
uploads them to the release, and only then clears the pre-release flag and
marks it Latest. Both updaters read
`https://github.com/obs-rust/obs-studio/releases/latest/download/`, which
GitHub resolves to the newest full release. Certify a build only after a
maintainer has run it. Certifying an older release makes it Latest again,
but an app newer than that release won't downgrade.
