"""Regression coverage for OBS version detection in tagged and tagless checkouts."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

# The host's git config must not change what the test repo does: tag or
# commit signing, or a missing identity, would make git tag/commit fail.
GIT_ISOLATION = ("-c", "tag.gpgSign=false", "-c", "commit.gpgSign=false", "-c", "user.name=t", "-c", "user.email=t@t")


VERSIONCONFIG = Path(__file__).resolve().parents[1] / "cmake/common/versionconfig.cmake"


class VersionConfigTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="obs-versionconfig-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.git("init", "-q")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "user.name", "Test")
        (self.root / "marker.txt").write_text("first\n", encoding="utf-8")
        self.git("add", "marker.txt")
        self.git("commit", "-qm", "initial commit")
        self.driver = self.root / "probe.cmake"
        self.driver.write_text(
            'set(_obs_default_version "0" "0" "1")\n'
            'set(_obs_release_candidate 0)\n'
            'set(_obs_beta 0)\n'
            'include("${VERSIONCONFIG}")\n'
            'message(STATUS "OBS_TEST_VERSION=${OBS_VERSION}")\n'
            'message(STATUS "OBS_TEST_CANONICAL=${OBS_VERSION_CANONICAL}")\n',
            encoding="utf-8",
        )

    def git(self, *arguments: str) -> None:
        subprocess.run(["git", *GIT_ISOLATION, *arguments], cwd=self.root, check=True, capture_output=True)

    def probe(self, override: str | None = None) -> subprocess.CompletedProcess[str]:
        command = ["cmake", f"-DVERSIONCONFIG={VERSIONCONFIG}"]
        if override is not None:
            command.append(f"-DOBS_VERSION_OVERRIDE={override}")
        command.extend(["-P", str(self.driver)])
        return subprocess.run(command, cwd=self.root, text=True, capture_output=True)

    def assert_versions(self, result: subprocess.CompletedProcess[str], *, version: str, canonical: str) -> None:
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f"OBS_TEST_VERSION={version}", result.stdout + result.stderr)
        self.assertIn(f"OBS_TEST_CANONICAL={canonical}", result.stdout + result.stderr)

    def test_tagless_checkout_uses_default_version(self) -> None:
        self.assert_versions(self.probe(), version="0.0.1", canonical="0.0.1")

    def test_non_version_tag_uses_default_version(self) -> None:
        self.git("tag", "not-a-version")
        self.assert_versions(self.probe(), version="0.0.1", canonical="0.0.1")

    def test_release_tag_is_preserved(self) -> None:
        self.git("tag", "32.0.2")
        self.assert_versions(self.probe(), version="32.0.2", canonical="32.0.2")

    def test_commit_after_release_tag_keeps_canonical_version(self) -> None:
        self.git("tag", "32.0.2")
        (self.root / "marker.txt").write_text("second\n", encoding="utf-8")
        self.git("commit", "-qam", "subsequent commit")
        result = self.probe()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        output = result.stdout + result.stderr
        self.assertIn("OBS_TEST_CANONICAL=32.0.2", output)
        self.assertRegex(output, r"OBS_TEST_VERSION=32\.0\.2-1-g[0-9a-f]+")

    def test_explicit_version_override_works_without_tags(self) -> None:
        self.assert_versions(self.probe("33.0.1-rc2"), version="33.0.1-rc2", canonical="33.0.1")

    def test_invalid_version_override_is_still_rejected(self) -> None:
        result = self.probe("not-a-version")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Invalid version supplied", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
