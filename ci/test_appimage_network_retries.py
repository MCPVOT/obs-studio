#!/usr/bin/env -S uv run --script
"""AppImage CI survives a slow mirror or registry (#2182).

    uv run python -m unittest ci/test_appimage_network_retries.py

Two master-side AppImage runs on 2026-10-24/25 failed on the network, not the
code:
- 40145813015: an AlmaLinux mirror served packages at under 1 kB/s
  ("Operation too slow") until "Build the glibc 2.28 image" hit its 20 minute
  timeout.
- 40131480584: Docker Hub answered the almalinux:8 pull with an error
  ("failed to resolve reference"), failing that test job.
dnf now drops a stalled mirror quickly and prefers fast ones, every download
in the image retries, the image build is retried once, and each test job pulls
its stock image with retries and backoff before running scenarios.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCKERFILE = (ROOT / "build-aux/appimage/manylinux/Dockerfile").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/appimage.yaml").read_text(encoding="utf-8")


def step(name: str) -> str:
    match = re.search(rf"(?ms)^      - name: {re.escape(name)}\n(.*?)(?=^      - (?:name|uses): |\Z)", WORKFLOW)
    assert match, name
    return match.group(1)


class AppImageNetworkRetries(unittest.TestCase):
    def test_dnf_drops_stalled_mirrors_before_any_install(self) -> None:
        conf = DOCKERFILE.index("/etc/dnf/dnf.conf")
        self.assertLess(conf, DOCKERFILE.index("dnf install"))
        block = DOCKERFILE[conf - 400:conf + 200]
        for setting in ("retries=", "minrate=", "timeout=", "fastestmirror=True"):
            self.assertIn(setting, block)

    def test_every_curl_in_the_image_retries(self) -> None:
        curls = re.findall(r"(?:RUN|&&) curl [^\n]*", DOCKERFILE)
        self.assertTrue(curls)
        for line in curls:
            self.assertIn("--retry", line, line)

    def test_rustup_retries_its_downloads(self) -> None:
        self.assertRegex(DOCKERFILE, r"RUSTUP_MAX_RETRIES=\d+")

    def test_image_build_is_retried(self) -> None:
        body = step("Build the glibc 2.28 image 🐳")
        self.assertRegex(body, r"for attempt in 1 2")
        self.assertIn("docker build", body)
        self.assertIn("sleep", body)

    def test_stock_image_is_pulled_with_backoff_before_scenarios(self) -> None:
        pull = WORKFLOW.index("- name: Pull the stock image 🐳")
        run = WORKFLOW.index("- name: Run the AppImage in stock containers 🐳")
        self.assertLess(pull, run)
        body = step("Pull the stock image 🐳")
        self.assertIn("docker pull", body)
        self.assertRegex(body, r"for attempt in 1 2 3 4 5")
        self.assertRegex(body, r"sleep \$\(\(attempt \* \d+\)\)")


if __name__ == "__main__":
    unittest.main()
