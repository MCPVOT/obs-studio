#!/usr/bin/env -S uv run --script
"""linux_qt_frontend.sh's real-Qt unittest command imports its modules
(#2311 removed the sys.path edits that used to put ci/ on the path). Run the
script's own command with no Qt prefix: every test must load, then skip.

    uv run --no-project ci/test_qt_frontend_unittest_imports.py
"""

from __future__ import annotations

import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def unittest_command() -> str:
    script = (ROOT / "ci/linux_qt_frontend.sh").read_text()
    found = re.search(r"^(\S*OBS_QT_TEST_PREFIX=\"\$qt\".*?-m unittest \\\n(?:.*\\\n)*.*)$", script, re.M)
    assert found, "linux_qt_frontend.sh has no real-Qt unittest command"
    return found.group(1).replace('OBS_QT_TEST_PREFIX="$qt"', 'OBS_QT_TEST_PREFIX=""')


class QtFrontendUnittestImportsTest(unittest.TestCase):
    def test_every_module_imports(self):
        env = {k: v for k, v in os.environ.items() if k not in ("PYTHONPATH", "OBS_QT_TEST_PREFIX")}
        out = subprocess.run(["bash", "-c", unittest_command()], cwd=ROOT, env=env, capture_output=True, text=True)
        self.assertNotIn("ModuleNotFoundError", out.stderr)
        self.assertEqual(out.returncode, 0, out.stderr[-2000:])


if __name__ == "__main__":
    unittest.main()
