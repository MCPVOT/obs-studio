#!/usr/bin/env -S uv run --script
"""libobs CMake platform boundaries: per-OS lists live in libobs/cmake/os-<os>.cmake (#888)."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent
LIBOBS = ROOT / "libobs"
HOST_CONDITION = re.compile(r"\b(?:if|elseif)\(([^)]*\bOS_(?:WINDOWS|MACOS|LINUX|FREEBSD|OPENBSD)\b[^)]*)\)", re.S)
# The multi-arch bootstrap and the single per-OS include dispatch stay in the
# shared file; every other host branch belongs in an os-<os>.cmake file.
ALLOWED = [
    "OS_WINDOWS AND NOT OBS_PARENT_ARCHITECTURE STREQUAL CMAKE_VS_PLATFORM_NAME",
    "OS_WINDOWS",
    "OS_MACOS",
    "OS_LINUX",
    "OS_FREEBSD OR OS_OPENBSD",
]


class LibobsCMakeFacadeTest(unittest.TestCase):
    def test_shared_file_has_only_bootstrap_and_dispatch(self):
        text = (LIBOBS / "CMakeLists.txt").read_text()
        self.assertEqual(HOST_CONDITION.findall(text), ALLOWED)

    def test_host_public_headers_are_appended_by_their_os_file(self):
        for host, header in (("windows", "util/windows/ComPtr.hpp"),
                             ("windows", "util/threading-windows.h"),
                             ("macos", "util/apple/cfstring-utils.h")):
            with self.subTest(host=host, header=header):
                source = (LIBOBS / f"cmake/os-{host}.cmake").read_text()
                self.assertIn("APPEND PROPERTY OBS_PUBLIC_HEADERS", " ".join(source.split()))
                self.assertIn(header, source)
                self.assertNotIn(header, (LIBOBS / "CMakeLists.txt").read_text())


if __name__ == "__main__":
    unittest.main()
