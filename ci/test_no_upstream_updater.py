#!/usr/bin/env -S uv run --script
"""OBS-Studio-Rust updates only from certified obs-rust releases, never upstream.

Usage:
    uv run python -m unittest ci/test_no_upstream_updater.py

Upstream's Sparkle feed and key, and the Windows updater's obsproject.com
manifests, made OBS-Studio-Rust offer and accept upstream OBS Studio as an
update (#1729, #1730, #1736). Both updaters stay on and read obs-rust's feed:
https://github.com/obs-rust/obs-studio/releases/latest/download/..., which
GitHub resolves to the newest full (certified) release only. ci/update_feed.py
writes those assets when a maintainer certifies a daily build (see
ci/test_update_feed.py). Metadata is RSA-signed with obs-rust's key
(frontend/data/OBSPublicRSAKey.pem); DMGs are EdDSA-signed for Sparkle.
"""

import importlib.util
import json
import plistlib
import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
UPSTREAM_KEY = "HQ5/Ba9VHOuEWaM0jtVjZzgHKFJX9YTl+HNVpgNF0iM="
FEED_PREFIX = "https://github.com/obs-rust/obs-studio/releases/latest/download/"
OBS_RUST_SPARKLE_KEY = "x7PaevvHxyhTaZdEFDhjr6+SqKf631gWXuIHowm6D3I="
# First base64 line of obs-rust's RSA-4096 update metadata key (OBSPublicRSAKey.pem).
OBS_RUST_RSA_KEY_LINE = "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAramOrg+JnU+xAYh7WYbU"
UPSTREAM_RSA_KEY_LINE = "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAl3sverw9HQ+rYQNn9Ca7"
UPDATER_SOURCES = (
    "frontend/utility/AutoUpdateThread.cpp.hpp",
    "frontend/utility/OBSSparkle.mm.hpp",
    "frontend/utility/MacUpdateThread.cpp.hpp",
)


def cache_value(preset: dict, name: str) -> str:
    value = preset.get("cacheVariables", {}).get(name)
    if isinstance(value, dict):
        value = value.get("value")
    return value or ""


def text(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


class NoUpstreamUpdaterTests(unittest.TestCase):
    def test_presets_do_not_use_upstream_sparkle_feed(self) -> None:
        presets = json.loads(text("CMakePresets.json"))
        for preset in presets.get("configurePresets", []):
            with self.subTest(preset=preset.get("name")):
                self.assertNotIn("obsproject.com", cache_value(preset, "SPARKLE_APPCAST_URL"))
                self.assertNotEqual(cache_value(preset, "SPARKLE_PUBLIC_KEY"), UPSTREAM_KEY)

    def test_macos_updater_is_on_with_obs_rust_feed_and_key(self) -> None:
        # feature-sparkle.cmake turns Sparkle on when both are set.
        presets = json.loads(text("CMakePresets.json"))
        macos = next(p for p in presets["configurePresets"] if p.get("name") == "macos")
        self.assertEqual(cache_value(macos, "SPARKLE_APPCAST_URL"), FEED_PREFIX + "updates_$(ARCHS)_v2.xml")
        self.assertEqual(cache_value(macos, "SPARKLE_PUBLIC_KEY"), OBS_RUST_SPARKLE_KEY)

    def test_updater_sources_use_obs_rust_feed(self) -> None:
        for rel in UPDATER_SOURCES:
            with self.subTest(file=rel):
                self.assertNotIn("obsproject.com", text(rel))
                self.assertIn(FEED_PREFIX, text(rel))

    def test_update_metadata_key_is_obs_rust(self) -> None:
        pem = text("frontend/data/OBSPublicRSAKey.pem")
        self.assertNotIn(UPSTREAM_RSA_KEY_LINE, pem)
        self.assertIn(OBS_RUST_RSA_KEY_LINE, pem)

    def test_windows_updater_is_on_and_does_not_build_upstream_updater_exe(self) -> None:
        cmake = text("frontend/cmake/os-windows.cmake")
        self.assertNotIn("ENABLE_WIN_UPDATER", cmake, "the Windows updater has no off switch")
        self.assertIn("obs_frontend_add_sources(WINDOWS)", cmake)
        windows = json.loads(text("frontend/build/source-sets.json"))["WINDOWS"]
        self.assertIn("build/frontend.windows-autoupdatethread+.hpp.cpp", windows)
        # frontend/updater/ is upstream's per-file installer that downloads
        # from obsproject.com's CDN; obs-rust installs its release ZIP instead.
        self.assertIsNone(re.search(r"^\s*add_subdirectory\(updater\)", cmake, re.M))
        source = text("frontend/utility/AutoUpdateThread.cpp.hpp")
        self.assertIn("FetchAndVerifyFile", source)
        self.assertIn("QCryptographicHash::Sha256", source)
        self.assertNotIn("updater.exe", source)

    def test_shared_frontend_still_starts_the_updaters(self) -> None:
        self.assertRegex(text("frontend/utility/platform.hpp"),
                         r"#if defined\(_WIN32\) \|\| defined\(ENABLE_SPARKLE_UPDATER\)\n#define OBS_PLATFORM_HAS_UPDATE_BRANCHES")
        self.assertIn("new AutoUpdateThread(", text("frontend/widgets/OBSBasic_Updater.cpp.hpp"))

    def test_upstream_appcast_jobs_are_gone(self) -> None:
        publish = text(".github/workflows/publish.yaml")
        self.assertNotIn("sparkle-appcast", publish)
        self.assertNotIn("cdn-fastly.obsproject.com", publish)

    def test_packaged_bundle_check_requires_obs_rust_feed(self) -> None:
        # release-rust.yaml's macos_smoke runs `app_identity_check.py bundle` on the
        # app copied out of the released DMG; it must reject upstream's feed and key.
        spec = importlib.util.spec_from_file_location("app_identity_check", ROOT / "ci/app_identity_check.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)

        def bundle(scratch: str, feed: str, key: str) -> Path:
            app = Path(scratch) / "OBS-Studio-Rust.app"
            (app / "Contents/MacOS").mkdir(parents=True)
            (app / "Contents/MacOS/OBS-Studio-Rust").write_bytes(b"")
            (app / "Contents/Frameworks/Sparkle.framework").mkdir(parents=True)
            info = {
                "CFBundleIdentifier": module.APP_ID,
                "CFBundleExecutable": "OBS-Studio-Rust",
                "CFBundleDisplayName": "OBS-Studio-Rust",
                "SUFeedURL": feed,
                "SUPublicEDKey": key,
            }
            with (app / "Contents/Info.plist").open("wb") as stream:
                plistlib.dump(info, stream)
            return app

        with tempfile.TemporaryDirectory() as scratch:
            module.check_bundle(bundle(scratch, FEED_PREFIX + "updates_arm64_v2.xml", OBS_RUST_SPARKLE_KEY))
        for feed, key in (
            ("https://obsproject.com/osx_update/updates_arm64_v2.xml", UPSTREAM_KEY),
            (FEED_PREFIX + "updates_arm64_v2.xml", UPSTREAM_KEY),
            (FEED_PREFIX + "updates_$(ARCHS)_v2.xml", OBS_RUST_SPARKLE_KEY),
            ("https://github.com/obs-rust/obs-studio/releases/download/update-feed/updates_arm64_v2.xml",
             OBS_RUST_SPARKLE_KEY),
            ("", ""),
        ):
            with self.subTest(feed=feed, key=key), tempfile.TemporaryDirectory() as scratch:
                with self.assertRaises(RuntimeError):
                    module.check_bundle(bundle(scratch, feed, key))


if __name__ == "__main__":
    unittest.main()
