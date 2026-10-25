#!/usr/bin/env -S uv run --script
"""The Flatpak ships under obs-rust's own app id, not upstream's (#2354).

com.obsproject.Studio is upstream OBS's Flathub id: installing a Flatpak with
it replaces a user's Flathub OBS, and we could never publish it. The .deb,
AppImage and macOS bundle already use io.github.obs_rust.OBSStudioRust.

    uv run python -m unittest ci/test_flatpak_identity.py
"""

import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
APP_ID = "io.github.obs_rust.OBSStudioRust"
MANIFEST = ROOT / "build-aux" / f"{APP_ID}.json"
UPSTREAM = "com.obsproject.Studio"


class FlatpakIdentity(unittest.TestCase):
    def manifest(self) -> dict:
        self.assertTrue(MANIFEST.is_file(), f"{MANIFEST.relative_to(ROOT)} is missing")
        return json.loads(MANIFEST.read_text())

    def test_manifest_is_named_after_the_app_id(self):
        self.assertFalse((ROOT / "build-aux" / f"{UPSTREAM}.json").exists())
        self.assertEqual(self.manifest()["id"], APP_ID)

    def test_command_is_the_branded_gui(self):
        self.assertEqual(self.manifest()["command"], "obs-studio-rust-gui")

    def test_obs_builds_branded(self):
        obs = next(m for m in self.manifest()["modules"] if isinstance(m, dict) and m.get("name") == "obs")
        self.assertIn("-DOBS_BRAND_RUST=ON", obs["config-opts"])
        self.assertNotIn("-DOBS_BRAND_RUST=OFF", obs["config-opts"])

    def test_plugin_extension_point_is_ours(self):
        extensions = self.manifest()["add-extensions"]
        self.assertIn(f"{APP_ID}.Plugin", extensions)
        self.assertNotIn(f"{UPSTREAM}.Plugin", extensions)

    def test_ci_checks_the_branded_build_share(self):
        workflow = (ROOT / ".github/workflows/build-project.yaml").read_text()
        self.assertNotIn(UPSTREAM, workflow)
        self.assertIn(f"metainfo/{APP_ID}.metainfo.xml", workflow)


if __name__ == "__main__":
    unittest.main()
