#!/usr/bin/env -S uv run --script
"""rust/tools/sanitize/triage.py: LeakSanitizer reports.

    uv run python -m unittest ci/test_sanitize_triage.py

A leak allocated inside a library's one-time initialization that was unloaded
before LeakSanitizer reported (every frame above OBS's own is
`(<unknown module>)` or pthread_once; the OpenGL driver under gs_create) is
not an OBS finding: it is listed as external, not counted. Each leak in a
report is triaged on its own, so a real OBS leak next to it still counts.
"""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TRIAGE = ROOT / "rust/tools/sanitize/triage.py"

DRIVER_LEAK = """\
Direct leak of 128 byte(s) in 1 object(s) allocated from:
    #0 0x78f44f328e9f in realloc ../../../../src/libsanitizer/asan/asan_malloc_linux.cpp:81
    #1 0x74f439a65216  (<unknown module>)
    #2 0x78f44d8b14bb in __pthread_once_slow nptl/pthread_once.c:116
    #3 0x78f44d8b1538 in ___pthread_once nptl/pthread_once.c:143
    #4 0x74f43c3335aa  (<unknown module>)
    #5 0x78f44e97c5f2 in gs_create /home/runner/work/obs-studio/obs-studio/libobs/graphics/graphics.c.h:232
    #6 0x78f44e804248 in obs_init_graphics /home/runner/work/obs-studio/obs-studio/libobs/obs.c.h:234

"""
OBS_LEAK = """\
Direct leak of 64 byte(s) in 1 object(s) allocated from:
    #0 0x7b7254b29ddf in malloc ../../../../src/libsanitizer/asan/asan_malloc_linux.cpp:67
    #1 0x7b725428f712 in bmalloc (/build/libobs/libobs.so.30+0x68f712)
    #2 0x7b72540e7b16 in obs_canvas_insert_source /home/runner/work/obs-studio/obs-studio/libobs/obs-canvas.c.h:330

"""
HEADER = "=================================================================\n==1==ERROR: LeakSanitizer: detected memory leaks\n\n"


def triage(*leaks):
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "asan"
        (out / "tests").mkdir(parents=True)
        (out / "tests" / "summary.txt").write_text("test_x exit=0\n")
        (out / "tests" / "test_x.asan.1").write_text(HEADER + "".join(leaks))
        run = subprocess.run(
            ["uv", "run", "--no-project", str(TRIAGE), str(out), "--fail-on-findings"],
            capture_output=True, text=True,
        )
        return run.returncode, run.stdout


class LeakTriageTest(unittest.TestCase):
    def test_unloaded_library_init_leak_is_external(self):
        code, md = triage(DRIVER_LEAK)
        self.assertEqual(code, 0, md)
        self.assertIn("External", md)
        self.assertIn("libobs/graphics/graphics.c.h:232", md)

    def test_obs_leak_next_to_it_still_counts(self):
        code, md = triage(DRIVER_LEAK, OBS_LEAK)
        self.assertEqual(code, 1, md)
        self.assertIn("libobs/obs-canvas.c.h:330", md)


if __name__ == "__main__":
    unittest.main()
