# Valgrind app shutdown recheck (#448)

The deferred verification in [#448](https://github.com/obs-rust/obs-studio/issues/448)
was run against `06b9f8fcd0efe9f765d72d1b7c43fa202929bb9b` using the existing
sanitizer harness. OBS completed its shutdown after SIGINT. The four allocation
families questioned after the original crash did not appear in either of the
two definitely-lost records. This is not an all-clean Valgrind result.

## Run and reproduction

- [Workflow run 39506926132](https://github.com/obs-rust/obs-studio/actions/runs/39506926132),
  Valgrind job `118918113955`, October 20, 2026.
- [Complete sanitizer artifact](https://github.com/obs-rust/obs-studio/actions/runs/39506926132/artifacts/12305634074):
  `sanitize-valgrind`, 146,783,406 bytes, 14-day retention.
- Ubuntu 26.04, Valgrind 3.26.0, Qt 6.10.2, Debug build, C libobs
  (`rust_libobs=false`), Xvfb 1920×1080×24, software llvmpipe rendering.
- Workflow parameters: `tools=valgrind`, `launch_seconds=45`,
  `rust_libobs=false`, `fail_on_findings=false`. A successful workflow
  conclusion therefore does not prove absence of findings.

Dispatch the existing workflow at the source revision to reproduce:

```sh
gh workflow run sanitizers.yaml --repo obs-rust/obs-studio --ref test/448-valgrind-clean-exit -f tools=valgrind -f launch_seconds=45 -f rust_libobs=false -f fail_on_findings=false
```

The workflow runs `rust/tools/sanitize/sanitize.sh valgrind 45`. It builds
the app, runs the cmocka executables, then launches a fresh profile under
`dbus-run-session`. The app command recorded by Valgrind was:

```text
/home/runner/work/obs-studio/obs-studio/.sanitize/build-valgrind/rundir/Debug/bin/obs --disable-shutdown-check --disable-updater --verbose
```

The wrapper supplies `valgrind --error-exitcode=99 --child-silent-after-fork=yes
--errors-for-leak-kinds=definite --leak-check=full --show-leak-kinds=definite
--num-callers=40 --track-origins=yes --fullpath-after=
--log-file=/home/runner/work/obs-studio/obs-studio/.sanitize/valgrind/app.vg.log`.
The harness exports `QT_ENABLE_REGEXP_JIT=0`, `QT_QPA_PLATFORM=xcb` and
`LIBGL_ALWAYS_SOFTWARE=1`. For Valgrind it expands the requested 45 seconds
to 180 seconds before SIGINT, with a further 240-second shutdown grace period.

## Shutdown evidence

The harness recorded `app exit=99 (exited)`, rather than `killed`. Exit 99
is the configured Valgrind diagnostic status, not an OBS crash status.
The app launch started at 22:58:23 UTC and returned at 23:01:34 UTC.
The app log records shutdown, scene/source destruction, output destruction,
`Freeing OBS context data`, destruction of canvas `Main`, final profiler
output and `Number of memory leaks: 0`. Valgrind reaches its final heap and
error summaries without a fatal-signal termination report. No forced-kill
backtrace was produced. The OBS allocator count alone is not a substitute
for Valgrind's independent results.

Full, unedited app logs (Valgrind compressed with gzip) are retained in the repository so this evidence
survives artifact expiry:

- [App stdout/stderr](2026-10-20-valgrind-app/app.log.txt)
- [Valgrind log and all allocation stacks](2026-10-20-valgrind-app/app.vg.log.txt.gz)
- [Harness exit record](2026-10-20-valgrind-app/app-exit.txt)
- [Structured results and SHA256 checksums](2026-10-20-valgrind-app/results.json)

## Allocation triage

| Allocation family from #448 | Observation after shutdown |
| --- | --- |
| Config items | Neither definitely-lost record contains a config-item allocation stack. |
| Canvas | The app logs destruction of `Main`. The graphics leak below includes canvas rendering callers, but not a canvas-object allocation stack. |
| Portal inhibitor | Neither definitely-lost record contains the portal inhibitor allocation path. The fresh headless profile does not exercise every desktop portal operation. |
| X11 hotkey display | Hotkey initialization and final hotkey-thread profiler output are present. Neither definitely-lost record identifies its `XOpenDisplay` allocation path. |

The final Valgrind totals are 312 bytes definitely lost in two blocks,
120 bytes indirectly lost in three blocks, 2,704 bytes possibly lost in
four blocks, and 653,608 bytes still reachable. The two definite records are:

1. **56 direct + 56 indirect bytes**, allocated through `calloc` followed
   by unsymbolized graphics addresses. Several addresses are identified
   as `libgallium-26.0.8-1ubuntu0.3.so` in earlier diagnostics in the same
   log; the first named OBS caller is `gs_draw`. This suggests the graphics
   driver path, but missing symbols prevent a definitive ownership claim.
   OBS rendering callers do not by themselves prove an OBS-owned leak.
2. **256 direct + 64 indirect bytes**, allocated in fontconfig, through
   expat/fontconfig configuration parsing and Qt's application-font loading.
   The OBS caller is `run_program` loading application fonts. The allocation
   belongs to the fontconfig call chain; this is not a config-file item or
   portal allocation.

No reproducible OBS-owned leak in the four named families was established,
so no separate OBS leak issue was filed from this run. The graphics record
remains unattributed; obtaining matching driver debug symbols is needed
before assigning its ownership. Neither record is suppressed or discarded
from the retained evidence.

## Limits and other diagnostics

Valgrind reports 62 errors from 15 contexts. The full log includes Qt/XCB
and graphics-library diagnostics and an OBS transition-duration
uninitialized-value diagnostic. Those are not leak records and are not
resolved by this verification report.

The harness summary reports 286/327 unit tests clean, two known tool
limitations (`test_pipe`, `test_svt_av1`), and two skipped ASan-only tests
(`test_formatted_filename`, `test_utf8`). Nonzero unit results and their
raw logs remain in the complete artifact; this report does not claim a
passing unit suite or infer app leaks from failed unit tests.

This observation covers one fresh-profile startup and shutdown under the
harness's reduced plugin configuration. It does not establish leak freedom
for Rust libobs, streaming, recording, imported profiles, every portal
operation, or hardware drivers. Browser, WebSocket, AJA, WebRTC, NVENC,
QSV, VST, scripting, MPEGTS output and VLC are disabled by the harness.
