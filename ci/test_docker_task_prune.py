#!/usr/bin/env -S uv run --script
"""docker-task.sh never prunes the image it is about to run (#1218).

prune_images keeps the newest tags by CreatedAt. Images built from the same
inputs can share one creation time, and the tie then falls to the tag string:
the image just built could sort last and be removed before its container
started. A fake `docker` on PATH lists such images and logs every removal.
"""

import os
from pathlib import Path
import re
import stat
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "ci" / "docker-task.sh"

FAKE_DOCKER = """#!/bin/sh
if [ "$1 $2" = "image ls" ]; then
  for t in 0000 1111 2222 9999; do
    printf '2026-01-01 00:00:00 +0000 UTC\\t%s\\n' "$t"
  done
elif [ "$1 $2" = "image rm" ]; then
  echo "$3" >>"$REMOVED"
fi
"""


def function(name: str) -> str:
    text = SCRIPT.read_text()
    match = re.search(rf"^{name}\(\) {{.*?^}}$", text, re.M | re.S)
    assert match, f"{name}() not found in {SCRIPT}"
    return match.group(0)


def prune_call() -> str:
    """The prune_images call made right after building the task image."""
    calls = [line.strip() for line in SCRIPT.read_text().splitlines() if line.strip().startswith("prune_images ")]
    assert calls, "no prune_images call after the build"
    return calls[0]


class PruneKeepsCurrentImage(unittest.TestCase):
    def test_image_just_built_survives(self):
        with tempfile.TemporaryDirectory() as tmp:
            bin_dir = Path(tmp) / "bin"
            bin_dir.mkdir()
            docker = bin_dir / "docker"
            docker.write_text(FAKE_DOCKER)
            docker.chmod(docker.stat().st_mode | stat.S_IXUSR)
            removed = Path(tmp) / "removed"
            removed.touch()

            script = "\n".join(
                [
                    'quiet() { "$@" 2>/dev/null || true; }',
                    function("prune_images"),
                    # The run just built the tag that sorts last of the ties.
                    'image=obs-task keep_images=1 tag="obs-task:0000"',
                    prune_call(),
                ]
            )
            env = dict(os.environ, PATH=f"{bin_dir}:{os.environ['PATH']}", REMOVED=str(removed))
            subprocess.run(["bash", "-c", script], check=True, env=env)

            gone = removed.read_text().split()
            self.assertNotIn("obs-task:0000", gone)
            self.assertEqual(len(gone), 2, f"keep 1 newest plus the image in use, removed {gone}")


if __name__ == "__main__":
    unittest.main()
