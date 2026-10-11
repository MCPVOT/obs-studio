"""Verify AppStream release metadata in tagged, tagless, and archive builds."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path

# The host's git config must not change what the test repo does: tag or
# commit signing, or a missing identity, would make git tag/commit fail.
GIT_ISOLATION = ("-c", "tag.gpgSign=false", "-c", "commit.gpgSign=false", "-c", "user.name=t", "-c", "user.email=t@t")


ROOT = Path(__file__).resolve().parents[1]
RELEASE_DATE = ROOT / "frontend/cmake/linux/appdata-release-date.cmake"
METADATA = ROOT / "frontend/cmake/linux/com.obsproject.Studio.metainfo.xml.in"


class AppdataReleaseDateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="obs-appdata-")
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.driver = self.repo / "probe.cmake"
        self.driver.write_text(
            'include("${RELEASE_DATE_HELPER}")\n'
            'configure_file("${METADATA_TEMPLATE}" "${METADATA_OUTPUT}" @ONLY)\n',
            encoding="utf-8",
        )
        (self.repo / "CMakeLists.txt").write_text("cmake_minimum_required(VERSION 3.28)\n", encoding="utf-8")
        self.git("init", "-q")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        self.commit("initial", "2024-02-03T12:00:00+0000")

    def git(self, *arguments: str, when: str | None = None) -> None:
        env = dict(os.environ)
        if when:
            env.update(GIT_AUTHOR_DATE=when, GIT_COMMITTER_DATE=when)
        subprocess.run(["git", *GIT_ISOLATION, *arguments], cwd=self.repo, env=env, check=True, capture_output=True)

    def commit(self, message: str, when: str) -> None:
        self.git("add", "CMakeLists.txt")
        self.git("commit", "--allow-empty", "-qm", message, when=when)

    def release(self, override: str | None = None) -> tuple[subprocess.CompletedProcess[str], ET.Element]:
        output = self.repo / "output.metainfo.xml"
        cmd = [
            "cmake",
            f"-DRELEASE_DATE_HELPER={RELEASE_DATE}",
            f"-DMETADATA_TEMPLATE={METADATA}",
            f"-DMETADATA_OUTPUT={output}",
            "-DOBS_VERSION=0.0.1",
            "-DGIT_HASH=deadbeef",
        ]
        if override is not None:
            cmd.append(f"-DAPPDATA_RELEASE_DATE={override}")
        cmd += ["-P", str(self.driver)]
        result = subprocess.run(cmd, cwd=self.repo, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        release = ET.parse(output).getroot().find("./releases/release")
        self.assertIsNotNone(release)
        self.assertEqual(release.get("version"), "0.0.1")
        return result, release

    def test_no_release_tags_uses_head_commit_date(self) -> None:
        _, release = self.release()
        self.assertEqual(release.get("date"), "2024-02-03")

    def test_release_tag_uses_tagged_commit_date(self) -> None:
        self.git("tag", "33.0.0")
        self.commit("later", "2025-09-01T12:00:00+0000")
        _, release = self.release()
        self.assertEqual(release.get("date"), "2024-02-03")

    def test_nonrelease_tag_does_not_produce_an_empty_date(self) -> None:
        self.git("tag", "dev-checkpoint")
        _, release = self.release()
        self.assertEqual(release.get("date"), "2024-02-03")

    def test_explicit_override_is_preserved(self) -> None:
        _, release = self.release("2022-11-12")
        self.assertEqual(release.get("date"), "2022-11-12")

    def test_source_archive_uses_file_timestamp(self) -> None:
        shutil.rmtree(self.repo / ".git")
        stamp = datetime(2025, 2, 3, 12, 0, tzinfo=timezone.utc).timestamp()
        os.utime(self.repo / "CMakeLists.txt", (stamp, stamp))
        _, release = self.release()
        self.assertEqual(release.get("date"), "2025-02-03")


if __name__ == "__main__":
    unittest.main()
