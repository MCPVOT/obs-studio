#!/usr/bin/env python3
"""The Rust toolchain is pinned and every crate declares the MSRV
(obs-rust/obs-studio#136).

    python3 -m unittest ci/test_toolchain.py
"""

from __future__ import annotations

import re
import tomllib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def version(text: str) -> tuple[int, ...]:
    return tuple(int(part) for part in text.split("."))


class ToolchainTest(unittest.TestCase):
    def setUp(self):
        self.toolchain = tomllib.loads((ROOT / "rust-toolchain.toml").read_text())["toolchain"]
        self.workspace = tomllib.loads((ROOT / "Cargo.toml").read_text())

    def test_channel_is_an_exact_version(self):
        # "stable" floats: a new release can change clippy -D warnings or
        # codegen for everyone at once.
        self.assertRegex(self.toolchain["channel"], r"^\d+\.\d+\.\d+$")

    def test_workspace_declares_msrv(self):
        msrv = self.workspace["workspace"]["package"].get("rust-version")
        self.assertIsNotNone(msrv, "[workspace.package] rust-version is missing")
        self.assertRegex(msrv, r"^\d+\.\d+(\.\d+)?$")
        self.assertLessEqual(version(msrv), version(self.toolchain["channel"]))

    def test_crates_inherit_msrv(self):
        for manifest in sorted(ROOT.glob("rust/*/Cargo.toml")):
            package = tomllib.loads(manifest.read_text())["package"]
            with self.subTest(crate=manifest.parent.name):
                self.assertEqual(package.get("rust-version"), {"workspace": True})


if __name__ == "__main__":
    unittest.main()
