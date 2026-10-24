#!/usr/bin/env -S uv run --script
"""The AppImage build gives tracked files the commit's time (#2182).

    uv run python -m unittest ci/test_appimage_stable_mtimes.py

rcc writes each resource file's mtime into the generated qrc_*.cpp, and a
fresh checkout gives every file a new mtime, so obs-websocket's
qrc_resources.cpp and the frontend's qrc_obs.cpp missed the compiler cache
on every build of the same commit (their resource trees differed only in
those timestamps, runs 40134709710 and 40136130433).
QT_RCC_SOURCE_DATE_EPOCH changed nothing, and SOURCE_DATE_EPOCH is part of
zccache's compile key, so a per-commit value missed all 322 compilations
(run 40119674697). The build sets tracked files' mtimes to the commit time.
"""

from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

spec = importlib.util.spec_from_file_location("appimage_build", ROOT / "build-aux" / "appimage" / "build.py")
build = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = build
spec.loader.exec_module(build)


def mtime_lines() -> str:
    script = build.BUILD
    return script[script.index('epoch="$(git log'):script.index("cmake --build build_ubuntu")]


class AppImageStableMtimes(unittest.TestCase):
    def test_tracked_files_get_the_commit_time_before_compiling(self) -> None:
        script = build.BUILD
        self.assertIn('epoch="$(git log -1 --format=%ct)"', script)
        self.assertLess(script.index("touch -h -c -d"), script.index("cmake --build build_ubuntu"))

    def test_source_date_epoch_is_never_set(self) -> None:
        self.assertNotRegex(build.BUILD, r"(?m)^[^#]*\bSOURCE_DATE_EPOCH=")

    def test_script_sets_tracked_and_submodule_files_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            sub, top = Path(tmp, "sub"), Path(tmp, "top")
            env = {**os.environ, "GIT_AUTHOR_DATE": "@1700000000 +0000", "GIT_COMMITTER_DATE": "@1700000000 +0000",
                   "GIT_CONFIG_COUNT": "1", "GIT_CONFIG_KEY_0": "protocol.file.allow", "GIT_CONFIG_VALUE_0": "always"}

            def git(cwd: Path, *args: str) -> None:
                subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args], cwd=cwd, env=env,
                               check=True, capture_output=True)

            for repo in (sub, top):
                repo.mkdir()
                git(repo, "init", "-q")
            (sub / "res.qrc").write_text("s")
            git(sub, "add", ".")
            git(sub, "commit", "-qm", "s")
            (top / "a.png").write_text("a")
            git(top, "add", ".")
            git(top, "submodule", "add", "-q", str(sub), "plugins/sub")
            git(top, "commit", "-qm", "t")
            (top / "untracked.txt").write_text("u")
            subprocess.run(["bash", "-e", "-c", mtime_lines()], cwd=top, env=env, check=True)
            self.assertEqual(os.stat(top / "a.png").st_mtime, 1700000000)
            self.assertEqual(os.stat(top / "plugins/sub/res.qrc").st_mtime, 1700000000)
            self.assertNotEqual(os.stat(top / "untracked.txt").st_mtime, 1700000000)


if __name__ == "__main__":
    unittest.main()
