#!/usr/bin/env -S uv run --script
"""Every zsh script under .github/scripts parses (`zsh -n`).

An apostrophe in a comment inside setup_ubuntu's single-quoted jq program
(#1948) ended the quote early: every Ubuntu app build then stopped in setup
with "parse error near `|'" before compiling anything. Uses $ZSH, else the zsh
on PATH; skips where there is none.
"""

from pathlib import Path
import os
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / ".github/scripts"


def zsh_scripts() -> list[Path]:
    files = sorted((SCRIPTS / "utils.zsh").iterdir())
    files += sorted(SCRIPTS.glob(".*.zsh"))
    return [f for f in files if f.is_file()]


class ZshScriptsParseTest(unittest.TestCase):
    def test_every_script_parses(self):
        zsh = os.environ.get("ZSH") or shutil.which("zsh")
        if not zsh:
            self.skipTest("zsh not installed")
        scripts = zsh_scripts()
        self.assertIn(SCRIPTS / "utils.zsh/setup_ubuntu", scripts)
        for script in scripts:
            with self.subTest(script=script.relative_to(ROOT).as_posix()):
                result = subprocess.run([zsh, "-n", str(script)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
