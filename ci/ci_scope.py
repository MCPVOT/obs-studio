#!/usr/bin/env -S uv run --script
"""Which CI jobs a change needs (obs-rust/obs-studio#436).

    uv run ci/ci_scope.py <base-ref>        # writes core/macos/windows to $GITHUB_OUTPUT

The Linux Rust tests and Tier 2 always run. On top of them:
- macos:   macOS code (or shared build/CI config) changed -> macOS Rust
           tests and Tier 2;
- windows: Windows code (or shared build/CI config) changed -> Windows Rust
           tests and Tier 2;
- core:    libobs or the Rust workspace changed (informational).
Platform is decided by path and, for otherwise shared C/C++/ObjC/CMake files,
by content (#698, ci/scope_content.py), read from `git diff -U0`:
- a changed line naming `__APPLE__`, `TARGET_OS_*`, `OS_MACOS`, `APPLE` (macOS)
  or `_WIN32`, `_MSC_VER`, `OS_WINDOWS`, `WIN32`, `MSVC` (Windows), or a hunk
  inside an open `#if`/`if()` block selecting that platform (negations and
  `#else` branches count), turns that platform on;
- a `test/cmocka/tests.d/*.cmake` registration turns a platform on only when
  it mentions that platform (or `.m`/`.mm`) and both when it adds a `.cpp` or
  `.hpp` test (#610); a plain C test stays Linux-only;
- `rust/*/src/platform/{macos,windows,unix}...` map to their platform
  (`unix` also reaches macOS); `platform/mod.rs` stays shared;
- whatever cannot be decided stays off: Linux covers it and the release gate
  covers the rest. `.github/`, `cmake/common/`, `cmake/finders/`,
  `libobs/cmake/`, the root CMake files and `test/cmocka/CMakeLists.txt`
  stay shared (every platform consumes them).
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

from scope_content import MAC, WIN, Hunk, parse_diff, touched

TOKEN = r"(?:^|[/_.\-])({})(?:[/_.\-]|$)"
MACOS = re.compile(
    TOKEN.format("mac|macos|osx|cocoa|apple|darwin|coreaudio|avcapture|syphon|videotoolbox|unix"),
    re.I,
)
WINDOWS = re.compile(TOKEN.format("win|win32|windows|wasapi|dshow|d3d9|d3d11|d3d12|dxgi|w32-pthreads"), re.I)
MACOS_SUFFIXES = (".m", ".mm", ".plist", ".xcconfig", ".entitlements")
WINDOWS_SUFFIXES = (".rc", ".manifest", ".ps1")
CORE_PREFIXES = ("libobs/", "rust/")
CORE_FILES = {"Cargo.toml", "Cargo.lock", "rust-toolchain.toml"}
# Build and CI configuration every platform consumes.
SHARED_PREFIXES = (".github/", "cmake/common/", "cmake/finders/", "libobs/cmake/")
SHARED_FILES = {"CMakeLists.txt", "CMakePresets.json", "buildspec.json"}
SHARED_PATTERN = re.compile(r"^rust/[^/]+/src/platform/mod\.rs$")
# Every platform's Tier 2 compiles these with its own compiler and C++
# library; plain C tests stay Linux-only (#610).
SHARED_TEST = re.compile(r"^test/cmocka/(CMakeLists\.txt|[^/]+\.(cpp|hpp))$")
# A tests.d registration is shared only when what it changes mentions a
# platform or registers a C++ test; a plain C test stays Linux-only (#698).
TESTS_D = re.compile(r"^test/cmocka/tests\.d/[^/]+\.cmake$")
CPP_TEST = re.compile(r"\.(?:cpp|hpp)\b")
OBJC_SOURCE = re.compile(r"\.mm?\b")


@dataclass(frozen=True)
class Scope:
    core: bool
    macos: bool
    windows: bool


def tests_d_platforms(hunks: list[Hunk] | None, new_text: str | None) -> set[str]:
    """Platforms a tests.d registration touches; no diff means every one."""
    if hunks is None:
        return {MAC, WIN}
    out = touched("tests.d/x.cmake", hunks, new_text)
    for hunk in hunks:
        text = "\n".join(hunk.changed)
        if CPP_TEST.search(text):
            out |= {MAC, WIN}
        if OBJC_SOURCE.search(text):
            out.add(MAC)
    return out


def scope(
    files: list[str],
    diff: str = "",
    read_file: Callable[[str], str | None] = lambda path: None,
) -> Scope:
    """`diff` is `git diff -U0` text; `read_file(path)` returns the new file."""
    hunks = parse_diff(diff)
    core = macos = windows = False
    for path in files:
        if path.startswith(CORE_PREFIXES) or path in CORE_FILES:
            core = True
        if path.startswith(SHARED_PREFIXES) or path in SHARED_FILES or SHARED_PATTERN.match(path) or SHARED_TEST.match(path):
            macos = windows = True
            continue
        found: set[str] = set()
        if TESTS_D.match(path):
            found = tests_d_platforms(hunks.get(path), read_file(path))
        elif path in hunks:
            found = touched(path, hunks[path], read_file(path))
        if path.startswith("cmake/macos/") or path.endswith(MACOS_SUFFIXES) or MACOS.search(path) or MAC in found:
            macos = True
        if path.startswith("cmake/windows/") or path.endswith(WINDOWS_SUFFIXES) or WINDOWS.search(path) or WIN in found:
            windows = True
    return Scope(core, macos, windows)


def changed_files(base: str) -> list[str]:
    out = subprocess.run(
        ["git", "diff", "--name-only", f"{base}...HEAD"], check=True, capture_output=True, text=True
    ).stdout
    return [line for line in out.splitlines() if line]


def changed_diff(base: str) -> str:
    return subprocess.run(
        ["git", "diff", "-U0", "--no-color", f"{base}...HEAD"], check=True, capture_output=True, text=True
    ).stdout


def head_file(path: str) -> str | None:
    done = subprocess.run(["git", "show", f"HEAD:{path}"], capture_output=True, text=True)
    return done.stdout if done.returncode == 0 else None


def main() -> int:
    files = changed_files(sys.argv[1])
    result = scope(files, changed_diff(sys.argv[1]), head_file)
    lines = [f"{k}={str(v).lower()}" for k, v in vars(result).items()]
    print("\n".join(lines))
    if out := os.environ.get("GITHUB_OUTPUT"):
        with open(out, "a") as f:
            f.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
