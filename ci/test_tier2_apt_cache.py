#!/usr/bin/env -S uv run --script
"""Tier 2 Linux jobs reuse their apt downloads (#1055).

    uv run python -m unittest ci/test_tier2_apt_cache.py

Every Tier 2 Linux job (build + 6 shards) downloaded the same 339 MB of
.debs (~20 s) into the stock ubuntu:26.04 container. The jobs now keep the
downloads (the image's docker-clean apt config deletes them) and cache
/var/cache/apt/archives, keyed on the package list and the image digest.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parent.parent / ".github" / "workflows" / "build-project.yaml"


def job(name: str) -> str:
    text = WORKFLOW.read_text(encoding="utf-8")
    m = re.search(rf"^  {re.escape(name)}:\n(.*?)(?=^  [a-z0-9-]+:\n|\Z)", text, re.S | re.M)
    assert m, f"no job {name}"
    return m.group(1)


class AptCacheTest(unittest.TestCase):
    def test_every_tier2_linux_job_caches_apt_archives(self):
        for name in ("rust-linux-tier2-build", "rust-linux-tier2"):
            j = job(name)
            self.assertIn("docker-clean", j, name)
            self.assertIn("path: /var/cache/apt/archives", j, name)
            self.assertIn("hashFiles('rust/tools/linux-validate/apt-packages.txt')", j, name)

    def test_cache_is_restored_before_the_install(self):
        for name in ("rust-linux-tier2-build", "rust-linux-tier2"):
            j = job(name)
            self.assertLess(j.index("path: /var/cache/apt/archives"), j.index("Install Tier 2 Packages"), name)
            self.assertLess(j.index("actions/checkout@"), j.index("path: /var/cache/apt/archives"), name)


if __name__ == "__main__":
    unittest.main()
