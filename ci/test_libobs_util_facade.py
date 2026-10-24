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


if __name__ == "__main__":
    unittest.main()
