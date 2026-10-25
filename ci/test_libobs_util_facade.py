#!/usr/bin/env -S uv run --script
"""libobs/util host branches move into the host files (#874)."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent
UTIL = ROOT / "libobs" / "util"


class LibobsUtilFacadeTest(unittest.TestCase):
    def test_profiler_csv_gz_closes_without_a_host_branch(self):
        # gzclose dispatches to gzclose_w for a write handle on every host,
        # so the Windows-only gzclose_w branch is not needed.
        text = (UTIL / "profiler-io.c.h").read_text()
        self.assertNotIn("gzclose_w", text)
        self.assertEqual(text.count("gzclose(gz);"), 1)

    def test_shared_files_have_no_host_branch(self):
        for name in ("platform-file.c.h", "profiler-io.c.h"):
            text = (UTIL / name).read_text()
            for macro in ("_WIN32", "_MSC_VER", "__APPLE__", "__linux__"):
                self.assertNotIn(macro, text, f"{name} still branches on {macro}")

    def test_file_bodies_live_in_the_host_files(self):
        # One body per host for each function the shared file branched on.
        for host in ("windows", "posix"):
            text = (UTIL / f"platform-file-host-{host}.c.h").read_text()
            for fn in ("os_wfopen(", "os_fopen(", "os_fseeki64(", "os_ftelli64("):
                self.assertIn(fn, text, f"platform-file-host-{host}.c.h lacks {fn}")
            self.assertIn("profiler_gzopen(", (UTIL / f"profiler-io-host-{host}.c.h").read_text())
        self.assertIn("int os_stat(", (UTIL / "platform-file-host-windows.c.h").read_text())

    def test_host_files_build_in_their_host_groups(self):
        build = ROOT / "libobs" / "build"
        for host, group in (("windows", "ported-windows-platform"), ("posix", "ported-posix")):
            text = (build / f"libobs.{group}+.h.c").read_text()
            for name in ("platform-file", "profiler-io"):
                self.assertIn(f'#include "libobs/util/{name}-host-{host}.c.h"', text)


if __name__ == "__main__":
    unittest.main()
