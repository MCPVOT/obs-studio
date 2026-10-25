#!/usr/bin/env -S uv run --script
"""Every Linux command the release notes name is one the Linux packages
install (#2303): the GUI name from cmake/common/app-identity.cmake, or a
CMake executable. The Rust obs-studio-rust CLI is built by cargo only.

    uv run --no-project ci/test_release_linux_binaries.py
"""

from __future__ import annotations

import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def packaged() -> set[str]:
    identity = (ROOT / "cmake/common/app-identity.cmake").read_text()
    names = set(re.findall(r"set\(OBS_UNIX_GUI_NAME\s+([\w-]+)\)", identity))
    cmake = subprocess.run(
        ["git", "ls-files", "*CMakeLists.txt", "*.cmake"], cwd=ROOT, check=True, capture_output=True, text=True
    ).stdout.split()
    for path in cmake:
        names |= set(re.findall(r"add_executable\(\s*([\w-]+)", (ROOT / path).read_text(errors="replace")))
    return names


class ReleaseLinuxBinariesTest(unittest.TestCase):
    def test_named_linux_commands_are_packaged(self):
        body = (ROOT / ".github/workflows/release-rust.yaml").read_text()
        sentence = re.search(r"Linux installs [^\n]*?(?=macOS installs)", body)
        self.assertIsNotNone(sentence, "release notes lost their 'Linux installs' sentence")
        names = re.findall(r"\\`([\w-]+)\\`", sentence.group(0))
        self.assertTrue(names)
        have = packaged()
        for name in names:
            with self.subTest(command=name):
                self.assertTrue(name in have, f"the release notes name {name}, but no Linux package installs it")


if __name__ == "__main__":
    unittest.main()
