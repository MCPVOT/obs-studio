#!/usr/bin/env -S uv run --script
"""Reconnect policy must not assume an exact OS timer lower bound (#1598)."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_settings_platform_facade import function

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'test/cmocka/test_output_delay_e2e.c'


class ReconnectContractTest(unittest.TestCase):
    def test_capture_is_wired_and_reset(self):
        text = SOURCE.read_text()
        self.assertIn('signal_handler_connect(sh, "reconnect", on_reconnect, NULL);', text)
        setup = function(text, 'static int setup(')
        self.assertIn('rec.reconnect_calls = 0;', setup)
        self.assertIn('rec.reconnect_timeout_sec = 0;', setup)

    def test_actual_reconnect_case_with_early_timeout_and_bad_policy(self):
        compiler = shutil.which('cc')
        if not compiler:
            self.skipTest('C compiler unavailable; native validation remains required')
        text = SOURCE.read_text()
        record = function(text, 'struct record {') + ';'
        case = function(text, 'static void preserve_holds_queue_through_reconnect(')
        callback = (function(text, 'static void on_reconnect(') if 'static void on_reconnect(' in text
                    else 'static void on_reconnect(void *unused, calldata_t *params) {(void)unused;(void)params;}')
        source = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define UNUSED_PARAMETER(x) (void)(x)
#define MAX_PACKETS 2048
#define NS_PER_SEC 1000000000ULL
#define OBS_OUTPUT_DELAY_PRESERVE 1
#define OBS_OUTPUT_DISCONNECTED 2
#define assert_true(x) do { if (!(x)) { fprintf(stderr,"assert: %s\n",#x); exit(77); } } while(0)
#define assert_int_equal(x,y) assert_true((x)==(y))
#define skip() exit(88)
typedef struct { int64_t timeout; } calldata_t;
static int64_t calldata_int(calldata_t *p, const char *key) { (void)key; return p->timeout; }
static int mutex;
static void pthread_mutex_lock(int *p) { (void)p; }
static void pthread_mutex_unlock(int *p) { (void)p; }
''' + record + r'''
static struct record rec, snap;
static bool obs_up = true;
static int *output;
static struct { int started, reconnected, deactivated; } ev;
static uint64_t elapsed;
static int clock_calls, requested, observed, bad;
static uint64_t os_gettime_ns(void) { return 10000000000ULL + (clock_calls++ ? elapsed : 0); }
static void obs_output_set_delay(int *out, int delay, int flags) {(void)out;(void)delay;(void)flags;}
static void obs_output_set_reconnect_settings(int *out, int retries, int seconds) {
 (void)out; assert_int_equal(retries,3); requested=seconds;
}
static bool obs_output_start(int *out) { (void)out; return true; }
static void wait_event(int event, const char *name) {(void)event;(void)name;}
static void wait_delivered(size_t n) {(void)n;}
''' + callback + r'''
static void obs_output_signal_stop(int *out, int code) {
 (void)out; (void)code; calldata_t params={observed}; on_reconnect(NULL,&params);
}
static bool obs_output_reconnecting(int *out) {(void)out;return true;}
static int start_calls(void) {return bad==1 ? 3 : 2;}
static size_t delivered_count(void) {return 10;}
static int obs_output_get_active_delay(int *out) {(void)out;return bad==2 ? 1 : 3;}
static void snapshot(void) {snap=rec;snap.encoded_ns[10]=bad==3 ? 11000000000ULL : 9000000000ULL;snap.stop_calls=bad==4;}
static void check_delivered(uint64_t delay) {assert_int_equal(delay,NS_PER_SEC);assert_true(bad!=5);}
static void obs_output_force_stop(int *out) {(void)out;}
''' + case + r'''
int main(int argc, char **argv) {
 assert_int_equal(argc,4); elapsed=strtoull(argv[1],NULL,10); observed=atoi(argv[2]);bad=atoi(argv[3]);
 void *state=NULL;preserve_holds_queue_through_reconnect(&state);assert_int_equal(requested,2);
 return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='obs-reconnect-contract-') as tmp:
            p = Path(tmp) / 'contract.c'; p.write_text(source)
            binary = Path(tmp) / 'contract'
            result = subprocess.run([compiler, '-std=c11', str(p), '-o', str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            for elapsed in (1999999999, 2000000000, 2000010000):
                with self.subTest(elapsed=elapsed):
                    result = subprocess.run([str(binary), str(elapsed), '2', '0'], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
            for timeout in (0, 1, 3):
                result = subprocess.run([str(binary), '2000000000', str(timeout), '0'], capture_output=True)
                self.assertEqual(result.returncode, 77, f'wrong reconnect timeout {timeout} accepted')
            for bad in range(1, 6):
                result = subprocess.run([str(binary), '2000000000', '2', str(bad)], capture_output=True)
                self.assertEqual(result.returncode, 77, f'existing behavior assertion {bad} lost')


if __name__ == '__main__':
    unittest.main()
