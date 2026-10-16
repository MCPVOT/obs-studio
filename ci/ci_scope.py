#!/usr/bin/env -S uv run --script
"""Which CI jobs a change needs (obs-rust/obs-studio#436).

    uv run ci/ci_scope.py <base-ref>        # writes core/macos/windows to $GITHUB_OUTPUT

The Linux Rust tests and Tier 2 always run. On top of them:
- macos:   macOS code (or shared build/CI config) changed -> macOS Rust
           tests and Tier 2;
- windows: Windows code (or shared build/CI config) changed -> Windows Rust
           tests and Tier 2;
- qt:      frontend/Qt code or a Qt-linked cmocka test changed -> the Linux
           Qt frontend build and its unit tests (#992);
- core:    libobs or the Rust workspace changed (informational).
Platform is decided by path alone (#897, ci/platform_paths.py): a changed
file of a platform (`win-capture/`, `platform-osx.mm`, `threading-posix.c`,
`rust/*/src/platform/windows/`, ...) starts that platform's jobs. Core files
carry no platform defines (ci/platform_ifdef_ratchet.py), so a core change
runs on Linux. `.github/`, `cmake/common/`, `cmake/finders/`, `libobs/cmake/`,
the root CMake files, `platform/mod.rs` and the C++ cmocka tests (with their
tests.d registrations) stay shared: every platform consumes them.
Every OBS app build (Ubuntu, macOS, Windows, Flatpak) runs only in the
release gate (`full`). A job that is skipped here can't hide a break: it
runs at the release gate.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

from platform_paths import MACOS, WINDOWS, platforms

CORE_PREFIXES = ("libobs/", "rust/")
CORE_FILES = {"Cargo.toml", "Cargo.lock", "rust-toolchain.toml"}
# Build and CI configuration every platform consumes.
SHARED_PREFIXES = (".github/", "cmake/common/", "cmake/finders/", "libobs/cmake/")
SHARED_FILES = {"CMakeLists.txt", "CMakePresets.json", "buildspec.json"}
SHARED_PATTERN = re.compile(r"^rust/[^/]+/src/platform/mod\.rs$")
# Every platform's Tier 2 compiles these with its own compiler and C++
# library; plain C tests stay Linux-only (#610).
SHARED_TEST = re.compile(r"^test/cmocka/(CMakeLists\.txt|[^/]+\.(cpp|hpp))$")
# A tests.d registration is shared when it registers a C++ test.
TESTS_D = re.compile(r"^test/cmocka/tests\.d/([^/]+)\.cmake$")


@dataclass(frozen=True)
class Scope:
    core: bool
    macos: bool
    windows: bool
    qt: bool = False


def is_shared(path: str, exists: Callable[[str], bool]) -> bool:
    if path.startswith(SHARED_PREFIXES) or path in SHARED_FILES or SHARED_PATTERN.match(path) or SHARED_TEST.match(path):
        return True
    m = TESTS_D.match(path)
    return bool(m) and any(exists(f"test/cmocka/{m.group(1)}.{ext}") for ext in ("cpp", "hpp"))


# The Linux Qt frontend job (#992) runs only when the change can break the Qt
# frontend build or a Qt-linked unit test; master, schedule and ci-full always
# run it (build-project.yaml).
QT_PREFIXES = ("frontend/", "shared/qt/", "shared/properties-view/", "plugins/frontend-tools/", "ci/linux_qt_frontend")
QT_FILES = {
    "ci/fetch_libs_asset.py",
    "ci/test_qt_fastpath.py",
    "test/cmocka/CMakeLists.txt",
    ".github/workflows/build-project.yaml",
    "cmake/finders/FindFFmpeg.cmake",
}
TEST_SOURCE = re.compile(r"^test/cmocka/([^/]+)\.(?:cpp|hpp|c|h)$")


def is_qt(path: str, read: Callable[[str], str]) -> bool:
    """Frontend/Qt code, the job's own files, or a Qt-linked cmocka test: a
    tests.d fragment that finds Qt6, or the test source it registers."""
    if path.startswith(QT_PREFIXES) or path in QT_FILES:
        return True
    m = TESTS_D.match(path) or TEST_SOURCE.match(path)
    return bool(m) and "Qt6" in read(f"test/cmocka/tests.d/{m.group(1)}.cmake")


def scope(
    files: list[str],
    exists: Callable[[str], bool] = lambda path: False,
    read: Callable[[str], str] = lambda path: "",
) -> Scope:
    """`exists(path)` says whether a file exists in the checkout, `read(path)`
    returns its text ("" when missing)."""
    core = macos = windows = qt = False
    for path in files:
        qt |= is_qt(path, read)
        if path.startswith(CORE_PREFIXES) or path in CORE_FILES:
            core = True
        if is_shared(path, exists):
            macos = windows = True
            continue
        found = platforms(path) or frozenset()
        macos |= MACOS in found
        windows |= WINDOWS in found
    return Scope(core, macos, windows, qt)


def changed_files(base: str) -> list[str]:
    out = subprocess.run(
        ["git", "diff", "--name-only", f"{base}...HEAD"], check=True, capture_output=True, text=True
    ).stdout
    return [line for line in out.splitlines() if line]


def read_text(path: str) -> str:
    try:
        return Path(path).read_text()
    except OSError:
        return ""


def main() -> int:
    files = changed_files(sys.argv[1])
    result = scope(files, lambda path: Path(path).is_file(), read_text)
    lines = [f"{k}={str(v).lower()}" for k, v in vars(result).items()]
    print("\n".join(lines))
    if out := os.environ.get("GITHUB_OUTPUT"):
        with open(out, "a") as f:
            f.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
