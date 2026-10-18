#!/usr/bin/env -S uv run --script
"""Tests for rust/tools/linux-validate/build-errors.sh.

    uv run python -m unittest ci/test_tier2_build_errors.py

A failed Tier 2 build printed only lines matching ": error" and "FAILED:".
cargo's messages ("error[E0308]: ...", "error: ...", a build script's
panic) match neither, so #814's ON build showed just
"FAILED: [code=101] .../libobs_preflight_ffi.a" and no reason.
"""

from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "rust" / "tools" / "linux-validate" / "build-errors.sh"

LOG = """\
[12/900] Building C object libobs/CMakeFiles/libobs.dir/util/dstr.c.o
/src/libobs/util/dstr.c:10:5: error: unknown type name 'foo'
FAILED: libobs/CMakeFiles/libobs.dir/util/dstr.c.o
   Compiling obs-preflight v0.1.0 (/src/rust/obs-preflight)
error[E0308]: mismatched types
  --> rust/obs-preflight/src/lib.rs:42:9
   |
42 |         "nope"
   |         ^^^^^^ expected `u32`, found `&str`

error: could not compile `obs-preflight` (lib) due to 1 previous error
thread 'main' panicked at rust/obs-preflight-ffi/build.rs:7:5:
cbindgen failed
FAILED: [code=101] obs-preflight-cargo/release/libobs_preflight_ffi.a
   Compiling obs-util v0.1.0 (/src/rust/obs-util)
warning: unused variable: `x`
"""


class BuildErrorsTest(unittest.TestCase):
    def test_recovery_preserves_first_failure_and_keeps_one_retry(self):
        source = (SCRIPT.parent / 'validate.sh').read_text()
        recovery = source.split('  tier2_targets=', 1)[1].split('  if [ -n "${OBS_EXTRA_CFLAGS:-}"', 1)[0]
        recovery = 'tier2_targets=' + recovery
        # Run the same helper through bash on hosts without /bin/bash.
        recovery = recovery.replace('"$SRC"/rust/tools/linux-validate/build-errors.sh',
                                    'bash "$SRC"/rust/tools/linux-validate/build-errors.sh')
        for retry_status, daemon_status in ((0, 0), (0, 1), (1, 0), (1, 1)):
            with self.subTest(retry_status=retry_status, daemon_status=daemon_status), tempfile.TemporaryDirectory() as directory:
                prefix = '''
cmake() {
  if [ ! -f "$build.calls" ]; then
    echo 1 > "$build.calls"
    echo 'FAILED: [code=125] first.o'
    echo 'zccache[err][D]: original daemon failure'
    return 1
  fi
  echo 2 > "$build.calls"
  echo "retry cache bypass: ${ZCCACHE_DISABLE:-unset}"
  echo 'FAILED: [code=1] second.o'
  return "$retry_status"
}
zccache() { echo "daemon diagnostic: $*"; return "$daemon_status"; }
'''
                result = subprocess.run(
                    ['bash', '-euo', 'pipefail', '-c', prefix + recovery],
                    env={**os.environ, 'SRC': str(ROOT),
                         'build': directory + '/build', 'retry_status': str(retry_status),
                         'daemon_status': str(daemon_status),
                         'GITHUB_OUTPUT': directory + '/step-output'},
                    capture_output=True, text=True)
                self.assertEqual(result.returncode, retry_status)
                self.assertEqual(Path(directory, 'build.calls').read_text().strip(), '2')
                self.assertIn('original daemon failure', result.stdout)
                self.assertIn('daemon diagnostic: status', result.stdout)
                self.assertIn('daemon diagnostic: stop', result.stdout)
                self.assertIn('retry cache bypass: ' + ('1' if daemon_status else 'unset'),
                              Path(directory, 'build.build.log').read_text())
                self.assertIn('original daemon failure',
                              Path(directory, 'build.build-first-failure.log').read_text())
                output = Path(directory, 'step-output')
                fields = output.read_text() if output.exists() else ''
                self.assertEqual('cache-healthy=false' in fields, bool(daemon_status))

    def test_unhealthy_cache_is_not_published(self):
        workflow = (ROOT / '.github/workflows/build-project.yaml').read_text()
        build = workflow.split('- name: Build Tier 2 (libobs and libobs-with-rust)', 1)[1].split('\n      - ', 1)[0]
        save = workflow.split('- name: Save Tier 2 zccache', 1)[1].split('\n      - ', 1)[0]
        self.assertIn('id: tier2-build', build)
        self.assertIn("steps.tier2-build.outputs.cache-healthy != 'false'", save)

    def run_script(self, log: str):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "build.log"
            p.write_text(log)
            return subprocess.run(["bash", str(SCRIPT), str(p)], capture_output=True, text=True)

    def test_keeps_c_and_ninja_errors(self):
        out = self.run_script(LOG).stdout
        self.assertIn("dstr.c:10:5: error: unknown type name 'foo'", out)
        self.assertIn("FAILED: [code=101] obs-preflight-cargo/release/libobs_preflight_ffi.a", out)

    def test_shows_rustc_errors_with_their_location(self):
        out = self.run_script(LOG).stdout
        self.assertIn("error[E0308]: mismatched types", out)
        self.assertIn("--> rust/obs-preflight/src/lib.rs:42:9", out)
        self.assertIn("could not compile `obs-preflight`", out)

    def test_shows_build_script_panics(self):
        out = self.run_script(LOG).stdout
        self.assertIn("panicked at rust/obs-preflight-ffi/build.rs:7:5", out)
        self.assertIn("cbindgen failed", out)

    def test_drops_noise(self):
        out = self.run_script(LOG).stdout
        self.assertNotIn("Compiling obs-util", out)
        self.assertNotIn("warning: unused variable", out)

    def test_shows_launcher_failure_reason_before_failed_edge_limit(self):
        reason = "zccache[err][D]: daemon unavailable at /cache/daemon.sock (cannot connect); refusing to run cc uncached."
        log = "".join(f"FAILED: [code=125] object-{i}.o\n{reason}\n" for i in range(213))
        out = self.run_script(log).stdout
        self.assertIn(reason, out)
        self.assertEqual(out.count(reason), 1)


if __name__ == "__main__":
    unittest.main()
