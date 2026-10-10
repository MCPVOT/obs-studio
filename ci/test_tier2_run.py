#!/usr/bin/env python3
"""Tests for rust/tools/linux-validate/run.sh (obs-rust/obs-studio#135).

    python3 -m unittest ci/test_tier2_run.py

run.sh is executed from two throwaway checkouts with stub `docker` and `git`
commands on PATH that record their arguments, so no container is started.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "rust" / "tools" / "linux-validate"

STUB = """#!/bin/sh
printf '%s\\n' "$*" >> "$STUB_LOG"
exit 0
"""


class Tier2VolumeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="tier2-run-"))
        self.bin = self.tmp / "bin"
        self.bin.mkdir()
        for name in ("docker", "git"):
            stub = self.bin / name
            stub.write_text(STUB)
            stub.chmod(0o755)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def checkout(self, name: str) -> Path:
        """A fake checkout holding a copy of the linux-validate tools."""
        repo = self.tmp / name
        dest = repo / "rust" / "tools" / "linux-validate"
        shutil.copytree(TOOLS, dest)
        for sub in ("plugins/obs-browser", "plugins/obs-websocket"):
            (repo / sub).mkdir(parents=True)
            (repo / sub / "placeholder").write_text("")
        return repo

    def docker_run_mounts(self, repo: Path, extra_env=None) -> dict[str, str]:
        self.calls = getattr(self, "calls", 0) + 1
        log = self.tmp / f"{repo.name}-{self.calls}.log"
        env = {k: v for k, v in os.environ.items() if not k.startswith(("OBS_TIER2", "OBS_CCACHE"))}
        env.update(PATH=f"{self.bin}:{env['PATH']}", STUB_LOG=str(log), **(extra_env or {}))
        subprocess.run(["bash", str(repo / "rust/tools/linux-validate/run.sh")], env=env, check=True)
        runs = [line.split() for line in log.read_text().splitlines() if line.startswith("run ")]
        self.assertEqual(len(runs), 1, log.read_text())
        args = runs[0]
        mounts = {}
        for flag, value in zip(args, args[1:]):
            if flag == "-v":
                source, target = value.split(":")[:2]
                mounts[target] = source
        return mounts

    def test_build_volume_is_per_checkout(self):
        a = self.docker_run_mounts(self.checkout("a"))
        b = self.docker_run_mounts(self.checkout("b"))
        self.assertNotEqual(a["/build"], b["/build"])
        self.assertNotEqual(a["/build"], "obs-rust-linux-validate-build")

    def test_build_volume_is_stable(self):
        repo = self.checkout("a")
        self.assertEqual(self.docker_run_mounts(repo)["/build"], self.docker_run_mounts(repo)["/build"])

    def test_ccache_is_shared(self):
        a = self.docker_run_mounts(self.checkout("a"))
        b = self.docker_run_mounts(self.checkout("b"))
        self.assertIn("/ccache", a)
        self.assertEqual(a["/ccache"], b["/ccache"])

    def test_volume_override(self):
        mounts = self.docker_run_mounts(self.checkout("a"), {"OBS_TIER2_VOLUME": "custom-vol"})
        self.assertEqual(mounts["/build"], "custom-vol")


if __name__ == "__main__":
    unittest.main()
