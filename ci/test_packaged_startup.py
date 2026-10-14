#!/usr/bin/env -S uv run --script
"""Saved-profile startup checks reject missing capture plugins and wrong CPU packages.

    uv run python -m unittest ci/test_packaged_startup.py
"""

from __future__ import annotations

import importlib.util
import json
import struct
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("packaged_startup", ROOT / "test/packaged-startup.py")
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)


class PackagedStartupTest(unittest.TestCase):
    def test_fixture_must_target_the_frontends_actual_config_directory(self):
        with tempfile.TemporaryDirectory() as root, patch.dict("os.environ", {"APPDATA": root}):
            smoke.require_config_directory(Path(root) / "obs-studio", "windows")
            with self.assertRaisesRegex(RuntimeError, "frontend configuration"):
                smoke.require_config_directory(Path(root) / "unrelated", "windows")
        with patch.object(Path, "home", return_value=Path("/example")):
            smoke.require_config_directory(Path("/example/Library/Application Support/obs-studio"), "macos")
            with self.assertRaisesRegex(RuntimeError, "frontend configuration"):
                smoke.require_config_directory(Path("/example/unrelated"), "macos")

    def inputs(self, platform):
        return [
            {"inputName": name, "inputKind": kind, "unversionedInputKind": kind}
            for name, kind in zip(smoke.NAMES, smoke.KINDS[platform], strict=True)
        ]

    def validate(self, platform, kinds=None, inputs=None, scene=None, profile=None):
        smoke.validate_profile(
            platform, list(smoke.KINDS[platform]) if kinds is None else kinds,
            self.inputs(platform) if inputs is None else inputs,
            {"currentProgramSceneName": smoke.SCENE} if scene is None else scene,
            {"currentProfileName": smoke.PROFILE} if profile is None else profile,
        )

    def test_saved_fixture_contains_the_sources_and_selects_the_profile(self):
        for platform in smoke.KINDS:
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as root:
                config = Path(root) / "config"
                smoke.write_profile(config, platform, 4455, "test-password")
                collection = json.loads((config / f"basic/scenes/{smoke.PROFILE}.json").read_text())
                self.assertEqual({source["id"] for source in collection["sources"]}, {*smoke.KINDS[platform], "scene"})
                self.assertEqual({item["name"] for item in collection["sources"][-1]["settings"]["items"]}, set(smoke.NAMES))
                self.assertIn(f"Profile={smoke.PROFILE}", (config / "user.ini").read_text())
                self.assertTrue(json.loads((config / "plugin_config/obs-websocket/config.json").read_text())["auth_required"])

    def test_existing_user_config_is_preserved(self):
        with tempfile.TemporaryDirectory() as root:
            config = Path(root) / "config"
            config.mkdir()
            marker = config / "user.ini"
            marker.write_text("user data")
            with self.assertRaisesRegex(RuntimeError, "existing configuration"):
                smoke.write_profile(config, "windows", 4455, "test-password")
            self.assertEqual(marker.read_text(), "user data")

    def test_live_profile_validation(self):
        for platform in smoke.KINDS:
            with self.subTest(platform=platform):
                self.validate(platform)
                with self.assertRaisesRegex(RuntimeError, "capture plugins are missing"):
                    self.validate(platform, kinds=list(smoke.KINDS[platform])[:-1])
                with self.assertRaisesRegex(RuntimeError, "saved input"):
                    self.validate(platform, inputs=self.inputs(platform)[:-1])
                with self.assertRaisesRegex(RuntimeError, "saved scene"):
                    self.validate(platform, scene={"currentProgramSceneName": "Default"})
                with self.assertRaisesRegex(RuntimeError, "saved profile"):
                    self.validate(platform, profile={"currentProfileName": "Default"})

    def test_package_architecture_is_checked_even_on_an_emulating_host(self):
        with tempfile.TemporaryDirectory() as root:
            app = Path(root) / "obs64.exe"
            for arch, machine in (("x64", 0x8664), ("arm64", 0xAA64)):
                with self.subTest(arch=arch):
                    header = bytearray(0x86)
                    header[:2] = b"MZ"
                    struct.pack_into("<I", header, 0x3C, 0x80)
                    header[0x80:0x84] = b"PE\0\0"
                    struct.pack_into("<H", header, 0x84, machine)
                    app.write_bytes(header)
                    smoke.require_windows_arch(app, arch)
                    with self.assertRaisesRegex(RuntimeError, "is not"):
                        smoke.require_windows_arch(app, "arm64" if arch == "x64" else "x64")

    def test_release_runs_arm64_package_on_a_native_runner_before_publication(self):
        workflow = (ROOT / ".github/workflows/release-rust.yaml").read_text()
        self.assertTrue("windows_arm64_smoke:" in workflow, "release never launches the arm64 package")
        self.assertTrue("windows-11-vs2026-arm" in workflow, "arm64 package has no native Windows runner")
        self.assertTrue("test/packaged-startup.py" in workflow, "release never reloads a saved capture profile")
        self.assertTrue("needs.windows_arm64_smoke.result == 'success'" in workflow, "publication skips native startup validation")


if __name__ == "__main__":
    unittest.main()
