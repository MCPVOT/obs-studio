#!/usr/bin/env -S uv run --script
"""libobs CMake platform boundaries: per-OS lists live in libobs/cmake/os-<os>.cmake (#888)."""

from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from platform_paths import platforms  # noqa: E402

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

    def test_rust_link_setup_dispatches_to_host_files(self):
        text = (LIBOBS / "cmake/rust.cmake").read_text()
        self.assertEqual(HOST_CONDITION.findall(text), ["OS_LINUX OR OS_FREEBSD OR OS_OPENBSD", "OS_WINDOWS", "OS_MACOS"])
        for host, platform in (("nix", "linux"), ("windows", "windows"), ("macos", "macos")):
            with self.subTest(host=host):
                self.assertEqual(platforms(f"libobs/cmake/rust-os-{host}.cmake"), frozenset({platform}))
                self.assertEqual(text.count(f'include("${{CMAKE_CURRENT_LIST_DIR}}/rust-os-{host}.cmake")'), 1)


if __name__ == "__main__":
    unittest.main()
