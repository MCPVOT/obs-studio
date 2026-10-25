#!/usr/bin/env -S uv run --script
"""No uv older than 0.13.4 is pinned anywhere (#2370).

uv 0.13.4 drops the compromised eMudhra/emSign root from its certificate
store (GHSA-hq85-5cpj-mr62), and 0.13.5 rejects chains through the
compromised intermediates. Older pinned uv still trusts them for every
download it makes over TLS.

    uv run python -m unittest ci/test_uv_floor.py
"""

import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOOR = (0, 13, 4)

# A pin that installs uv some other way and waits on another repository.
# Each entry names the issue that removes it.
PENDING = {
    # The image installs uv from an obs-rust/obs-studio-libs release, and
    # there is no uv-0.13.5 one yet.
    "build-aux/appimage/manylinux/Dockerfile": "#2370",
}

PATTERNS = (
    # setup-uv: `version: '0.13.1'` in a step that uses astral-sh/setup-uv.
    re.compile(r"astral-sh/setup-uv@[^\n]*\n(?:[ \t]++(?!- )[^\n]*\n){0,6}?[ \t]+version:\s*['\"]?(\d+\.\d+\.\d+)"),
    re.compile(r"ghcr\.io/astral-sh/uv:(\d+\.\d+\.\d+)"),
    re.compile(r"\bUV_VERSION=(\d+\.\d+\.\d+)"),
)


def pins() -> list[tuple[str, str]]:
    files = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, check=True, capture_output=True).stdout
    found = []
    for name in files.decode().split("\0"):
        if not name or not (name.startswith(".github/") or "Dockerfile" in name):
            continue
        path = ROOT / name
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for pattern in PATTERNS:
            found += [(name, m.group(1)) for m in pattern.finditer(text)]
    return found


def version(text: str) -> tuple[int, ...]:
    return tuple(int(part) for part in text.split("."))


class UvFloor(unittest.TestCase):
    def test_no_pinned_uv_below_the_floor(self):
        found = pins()
        self.assertTrue(found, "found no uv pins; the patterns no longer match")
        old = [f"{name}: uv {ver}" for name, ver in found if version(ver) < FLOOR and name not in PENDING]
        self.assertEqual(old, [], "uv older than 0.13.4 trusts the compromised eMudhra root")

    def test_pending_entries_are_still_needed(self):
        stale = [name for name in PENDING if not any(n == name and version(v) < FLOOR for n, v in pins())]
        self.assertEqual(stale, [], "remove these from PENDING: their uv is at the floor now")


if __name__ == "__main__":
    unittest.main()
