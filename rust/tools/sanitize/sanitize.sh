#!/bin/bash
# Builds OBS for one sanitizer tool, runs the cmocka tests under it, then
# launches OBS headless (Xvfb) and closes it with SIGINT after a timeout.
#
#   rust/tools/sanitize/sanitize.sh asan|tsan|valgrind [launch-seconds]
#
# Ubuntu: install deps with rust/tools/sanitize/install-ubuntu-deps.sh first.
# NixOS:  nix-shell rust/tools/sanitize/shell.nix --run "rust/tools/sanitize/sanitize.sh asan"
# Summarize the results with rust/tools/sanitize/triage.py <out>/<tool>.
#
# Environment:
#   SAN_OUT      output root (default rust/tools/sanitize/out); results go to
#                $SAN_OUT/<tool>, the build tree to $SAN_OUT/build-<tool>
#   RUST_LIBOBS  ON (default) uses the hybrid; OFF explicitly selects C rollback.
#   SKIP_TESTS   non-empty: only relaunch the app, keep earlier test results
set -uo pipefail
tool=${1:-}; secs=${2:-45}
case $tool in
asan)
  flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
  type=RelWithDebInfo
  ;;
tsan)
  flags="-fsanitize=thread -fno-omit-frame-pointer"
  type=RelWithDebInfo
  ;;
valgrind)
  flags="-fno-omit-frame-pointer"
  type=Debug
  ;;
*)
  echo "usage: $0 asan|tsan|valgrind [launch-seconds]" >&2
  exit 2
  ;;
esac

here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/../../.." && pwd)
root=${SAN_OUT:-$here/out}
out=$root/$tool
b=$root/build-$tool
inst=$b/rundir/$type
mkdir -p "$root" || exit 1
[ -n "${SKIP_TESTS:-}" ] || rm -rf "$out"
mkdir -p "$out"
[ -n "${OBS_VERSION_OVERRIDE:-}" ] || OBS_VERSION_OVERRIDE=$(git -C "$src" describe --always --tags --match "[0-9]*.[0-9]*.[0-9]*" 2>/dev/null || true)
# Compiler launcher: the pinned zccache (install-zccache.sh checks its
# SHA-256). ZCCACHE_CACHE_DIR is the dir CI restores and saves; stop the
# daemon on exit so its index is flushed there first.
"$src/rust/tools/linux-validate/install-zccache.sh" "$root/bin" >/dev/null || exit 1
PATH="$root/bin:$PATH"
export ZCCACHE_CACHE_DIR="${ZCCACHE_CACHE_DIR:-$root/zccache}"
trap 'zccache stop >/dev/null 2>&1 || true' EXIT

backend=HYBRID
[ "${RUST_LIBOBS:-ON}" != OFF ] || backend=C
echo "== [$tool] configure + build ($backend libobs)"
# Every build makes libobs-with-rust. Install the toolchain
# rust-toolchain.toml pins instead of relying on rustup's deprecated
# auto-install (rust-lang/rustup#4836).
(cd "$src" && rustup toolchain install) >"$out/rustup.log" 2>&1 || { tail -20 "$out/rustup.log"; exit 1; }
cmake -S "$src" -B "$b" -G Ninja -DCMAKE_BUILD_TYPE=$type -DENABLE_UNIT_TESTS=ON -DOBS_LIBOBS_BACKEND="$backend" -DOBS_BRAND_RUST=OFF \
  -DCMAKE_C_COMPILER_LAUNCHER=zccache -DCMAKE_CXX_COMPILER_LAUNCHER=zccache \
  -DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags" \
  -DCMAKE_EXE_LINKER_FLAGS="$flags -Wl,--enable-new-dtags" -DCMAKE_SHARED_LINKER_FLAGS="$flags -Wl,--enable-new-dtags" \
  -DCMAKE_MODULE_LINKER_FLAGS="$flags -Wl,--enable-new-dtags" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DENABLE_BROWSER=OFF -DENABLE_WEBSOCKET=OFF -DENABLE_AJA=OFF -DENABLE_WEBRTC=OFF \
  -DENABLE_NVENC=OFF -DENABLE_QSV11=OFF -DENABLE_VST=OFF -DENABLE_SCRIPTING=OFF \
  -DENABLE_NEW_MPEGTS_OUTPUT=OFF -DENABLE_VLC=OFF \
  ${OBS_VERSION_OVERRIDE:+-DOBS_VERSION_OVERRIDE=$OBS_VERSION_OVERRIDE} >"$out/configure.log" 2>&1 \
  || { tail -40 "$out/configure.log"; exit 1; }

# test_formatted_filename and test_utf8 hardcode -fsanitize=address,undefined
# (test/cmocka/CMakeLists.txt, obs-rust/obs-studio#159), which TSan rejects and
# valgrind can't run. They are covered by the asan pass, so the other tools
# build with -k 0 and skip them.
skip='^$'
[ $tool = asan ] || skip='^(test_formatted_filename|test_utf8)$'
cmake --build "$b" -- -k 0 >"$out/build.log" 2>&1 || [ $tool != asan ] || { grep -E "error" "$out/build.log" | head -40; exit 1; }
libobs=$(find "$inst" -name 'libobs.so.*' -print -quit 2>/dev/null)
if [ ! -x "$inst/bin/obs" ] || [ -z "$libobs" ]; then
  grep -E "error" "$out/build.log" | head -40
  exit 1
fi
libdir=$(dirname "$libobs")

# Full allocation stacks: distro FFmpeg has no frame pointers, and the fast
# unwinder stops at av_malloc (#2035).
export ASAN_OPTIONS="detect_leaks=1:fast_unwind_on_malloc=0:abort_on_error=0:halt_on_error=0:log_path=$out/asan"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0:log_path=$out/ubsan"
export TSAN_OPTIONS="halt_on_error=0:second_deadlock_stack=1:log_path=$out/tsan"
vg=(valgrind --error-exitcode=99 --child-silent-after-fork=yes --errors-for-leak-kinds=definite
  --leak-check=full --show-leak-kinds=definite --num-callers=40 --track-origins=yes --fullpath-after=)

# One headless display for the unit tests (several start OBS, which needs
# one on Linux) and the app launch.
disp=:$((90 + RANDOM % 9))
Xvfb $disp -screen 0 1920x1080x24 >/dev/null 2>&1 &
xpid=$!
export DISPLAY=$disp LIBGL_ALWAYS_SOFTWARE=1 QT_QPA_PLATFORM=xcb
sleep 2

if [ -z "${SKIP_TESTS:-}" ]; then
  echo "== [$tool] unit tests"
  mkdir -p "$out/tests"
  # Each test as ctest runs it: its command line with arguments and its
  # WORKING_DIRECTORY (libobs finds its data relative to it, #2043). Only
  # registered tests, so plugin modules named test_* are not run (#2045). A
  # wrapper in front of the binary (xvfb-run) is dropped: the display is ours.
  ctest --test-dir "$b" --show-only=json-v1 2>/dev/null \
    | jq -r '.tests[] | select(.command)
        | [.name, ([(.properties // [])[] | select(.name == "WORKING_DIRECTORY") | .value][0] // ""),
           (.command | join("\u001f"))] | @tsv' >"$out/tests/ctest.tsv"
  sort "$out/tests/ctest.tsv" | while IFS=$'\t' read -r n wd cmdline; do
    IFS=$'\x1f' read -r -a cmd <<<"$cmdline"
    while [ ${#cmd[@]} -gt 0 ] && [ "${cmd[0]##*/}" != "$n" ]; do
      cmd=("${cmd[@]:1}")
    done
    if [ ${#cmd[@]} -eq 0 ] || [ ! -x "${cmd[0]}" ]; then
      continue
    fi
    if [[ $n =~ $skip ]]; then
      echo "$n skipped (built with ASan)"
      continue
    fi
    if [ $tool = valgrind ]; then
      (cd "${wd:-$b/test/cmocka}" && timeout 600 "${vg[@]}" --log-file="$out/tests/$n.vg.log" "${cmd[@]}" </dev/null >"$out/tests/$n.log" 2>&1)
    else
      (cd "${wd:-$b/test/cmocka}" \
        && ASAN_OPTIONS="$ASAN_OPTIONS:log_path=$out/tests/$n.asan" \
          TSAN_OPTIONS="$TSAN_OPTIONS:log_path=$out/tests/$n.tsan" \
          UBSAN_OPTIONS="$UBSAN_OPTIONS:log_path=$out/tests/$n.ubsan" \
          timeout 600 "${cmd[@]}" </dev/null >"$out/tests/$n.log" 2>&1)
    fi
    echo "$n exit=$?"
  done | tee "$out/tests/summary.txt"
fi

echo "== [$tool] app launch (${secs}s)"
# PCRE2 JIT runs code from a dual-mapped memfd that valgrind cannot follow (SEGV in /memfd:sljit).
export QT_ENABLE_REGEXP_JIT=0
sleep 2
# A fresh profile each launch, so first-run paths are exercised every time.
HOME=$(mktemp -d "$out/profile.XXXX")
export HOME LD_LIBRARY_PATH=$libdir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
cmd=("$inst/bin/obs" --disable-shutdown-check --disable-updater --verbose)
# Seconds between SIGINT and SIGKILL: shutdown under valgrind is slow.
grace=60
if [ $tool = valgrind ]; then
  cmd=("${vg[@]}" --log-file="$out/app.vg.log" "${cmd[@]}")
  secs=$((secs * 4))
  grace=240
fi
cd "$inst/bin" || exit 1
dbus-run-session -- "${cmd[@]}" >"$out/app.log" 2>&1 &
spid=$!
sleep 1
# OBS, or valgrind running it; dbus-run-session's other child is dbus-daemon.
app=$(pgrep -P $spid -f "$inst/bin/obs" | head -1)
for _ in $(seq "$secs"); do kill -0 $spid 2>/dev/null || break; sleep 1; done
[ -n "$app" ] && kill -INT "$app" 2>/dev/null
status=closed
for _ in $(seq "$grace"); do kill -0 $spid 2>/dev/null || { status=exited; break; }; sleep 1; done
if [ $status = closed ] && [ -n "$app" ] && kill -0 "$app" 2>/dev/null; then
  # Still running after SIGINT + grace: record where it hangs (#462). gdb
  # needs sudo to attach to a non-child under ptrace_scope=1.
  gdbcmd=(gdb -batch -ex 'set pagination off' -ex 'info threads' -ex 'thread apply all bt' -p "$app")
  sudo -n "${gdbcmd[@]}" >"$out/hang-backtrace.txt" 2>&1 || "${gdbcmd[@]}" >"$out/hang-backtrace.txt" 2>&1
  kill -KILL "$app" $spid 2>/dev/null
  status=killed
fi
wait $spid
code=$?
echo "app exit=$code ($status)" | tee "$out/app-exit.txt"
cp -r "$HOME/.config/obs-studio/logs" "$out/obs-logs" 2>/dev/null || true
kill $xpid 2>/dev/null
exit 0
