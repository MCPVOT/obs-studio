#!/usr/bin/env -S uv run --script
"""Output-delay diagnostics must link without the Windows-private printer (#1580)."""

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_settings_platform_facade import function  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "test/cmocka/test_output_delay_e2e.c"


class OutputDelayDiagnosticsTest(unittest.TestCase):
    def test_no_dependency_on_private_cmocka_printer(self):
        self.assertNotRegex(SOURCE.read_text(), r"\bfail_msg\s*\(")

    def test_failure_paths_link_and_still_fail_with_their_diagnostics(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler unavailable; Windows Tier 2 remains required")
        text = SOURCE.read_text()
        record = function(text, "struct record {") + ";"
        helpers = "\n".join(function(text, signature) for signature in (
            "static void wait_event(", "static size_t check_delivered_order(",
            "static void check_delivered("))
        stop = function(text, "static void delayed_start_packets_and_stop(")
        # Compile the actual final stop assertion/loop, with only its setup
        # supplied by the fixture. No live encoder/audio thread is needed to
        # force each diagnostic path; the real e2e still runs in Tier 2.
        start = stop.index("size_t last = snap.delivered - 1;")
        loop = function(stop, "for (size_t i = last + 1;")
        prefix = stop[start:stop.index("for (size_t i = last + 1;", start)]
        stop_helper = "static void check_stop_snapshot(uint64_t before, uint64_t after) {\n" + prefix + loop + "\n}"
        preamble = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_PACKETS 2048
#define FRAME_SIZE 1024
#define WAIT_MS 10000
#define fail() do { exit(77); } while (0)
#define assert_true(value) do { if (!(value)) fail(); } while (0)
#define assert_int_equal(a, b) assert_true((a) == (b))
/* Model the observed Windows link contract: a declaration is available,
 * but no cm_print_error definition is supplied to the linker. */
void cm_print_error(const char *format, ...);
#define fail_msg(format, ...) do { cm_print_error("ERROR: " format "\n", __VA_ARGS__); fail(); } while (0)
typedef int os_event_t;
static int event_result;
static int os_event_timedwait(os_event_t *event, unsigned long timeout) {
    (void)event; (void)timeout; return event_result;
}
'''
        main = r'''
int main(int argc, char **argv) {
    if (argc != 2) return 90;
    snap.encoded = snap.delivered = 3;
    for (size_t i = 0; i < 3; ++i) {
        snap.encoded_ns[i] = i * 1000;
        snap.delivered_pts[i] = (int64_t)i * FRAME_SIZE;
        snap.delivered_ns[i] = snap.encoded_ns[i] + 1000000000ULL;
    }
    if (strcmp(argv[1], "timeout") == 0) {
        event_result = 1; wait_event(NULL, "the delayed stop");
    } else if (strcmp(argv[1], "order") == 0) {
        snap.delivered_pts[1] = 0; check_delivered(1000000000ULL);
    } else if (strcmp(argv[1], "early") == 0) {
        snap.delivered_ns[0] = 500000000ULL; check_delivered(1000000000ULL);
    } else if (strcmp(argv[1], "missing") == 0) {
        snap.delivered = 2; snap.delivered_pts[1] = 2 * FRAME_SIZE;
        snap.delivered_ns[1] = snap.encoded_ns[2] + 1000000000ULL;
        check_delivered(1000000000ULL);
    } else if (strcmp(argv[1], "stop") == 0) {
        snap.delivered = 1; check_stop_snapshot(2000000000ULL, 2000000000ULL);
    } else {
        wait_event(NULL, "success"); check_delivered(1000000000ULL);
        check_stop_snapshot(0, 2000000000ULL);
    }
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="obs-delay-diagnostic-") as tmp:
            source, binary = Path(tmp) / "probe.c", Path(tmp) / "probe"
            source.write_text(preamble + record + "\nstatic struct record snap;\n" + helpers + stop_helper + main)
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", str(source),
                                     "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            for case, diagnostic in (
                ("valid", ""), ("timeout", "timed out waiting for the delayed stop"),
                ("order", "packet 1 has pts 0 after 0 (out of order)"),
                ("early", "packet 0 reached the output 500 ms after it was encoded, before the 1000 ms delay"),
                ("missing", "1 packets never reached the output"),
                ("stop", "packet 1, encoded before the stop, never reached the output"),
            ):
                with self.subTest(case=case):
                    result = subprocess.run([str(binary), case], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0 if case == "valid" else 77, result.stderr)
                    self.assertIn(diagnostic, result.stderr)


if __name__ == "__main__":
    unittest.main()
