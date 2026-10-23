#!/usr/bin/env -S uv run --script
"""OBS-Studio-Rust keeps only the obsproject.com URLs it has decided to keep (#1732).

What's New reads obs-rust's release feed (#1882) and the About
box no longer downloads upstream's Patreon list. Every other obsproject.com URL
in frontend/ sources and plugins/*/data/package.json must be on the allow-list
below with a reason, so a new upstream runtime feed is a deliberate decision.

uv run python -m unittest ci.test_no_upstream_runtime_feeds
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent

URL = re.compile(r"https?://(?:[a-z0-9-]+\.)*obsproject\.com[^\"' )<>\\]*")
SOURCE_SUFFIXES = {".c", ".h", ".cpp", ".hpp", ".mm", ".cmake", ".txt", ".ui", ".in", ".json"}

# url -> reason it stays. Runtime data feeds first, then static links.
# What's New reads the obs-rust release assets, like the update feeds (#1882).
WHATSNEW_FEED = "https://github.com/obs-rust/obs-studio/releases/latest/download/whatsnew.json"

ALLOWED = {
    # Kept runtime data feeds (#1732): data only, no code, useful to users.
    "https://obsproject.com/obs2_update/rtmp-services/v5": "streaming-service list update (data only)",
    "https://obsproject.com/obs2_update/win-capture/v1": "game-capture compatibility list update (data only)",
    # Static links and placeholders, not fetched as data feeds (#1732 item 5).
    "https://obsproject.com/placeholders/youtube-chat": "YouTube chat dock placeholder page",
    "https://obsproject.com/assets/images/new_icon_small-r.png": "logo on the OAuth listener page",
    "https://obsproject.com/visual-studio-2022-runtimes": "VC runtime help link",
    "https://obsproject.com/contribute": "About box donate link",
    "https://obsproject.com/developer-contributing": "About box get-involved link",
    "https://obsproject.com/tools/analyzer": "log analyzer link",
    "https://obsproject.com/go/legacy-plugin-locations": "plugin manager help link",
    "https://obsproject.com/forum/threads/165863/": "DLL blocklist comment",
    "https://obsproject.com/help": "Help menu link",
    "https://obsproject.com": "website link",
    "https://obsproject.com/": "website link",
    "https://obsproject.com/discord": "Help menu link",
    "https://obsproject.com/kb/encoder-missing#%1": "encoder help link",
    "https://obsproject.com/assets/images/OBSDemoApp301Flathub.png": "metainfo screenshot",
    "https://auth.obsproject.com/": "OAuth base URL default",
    # Updater, update feeds and log/crash uploads: tracked in their own issues (#1727).
    "https://cdn-fastly.obsproject.com/": "updater (tracked separately)",
    "https://cdn-fastly.obsproject.com/update_studio": "updater (tracked separately)",
    "https://obsproject.com/update_studio/getpatchmanifest": "updater (tracked separately)",
    "https://obsproject.com/update_studio/": "updater (tracked separately)",
    "https://obsproject.com/update_studio/manifest.json": "updater (tracked separately)",
    "https://obsproject.com/update_studio/branches.json": "updater (tracked separately)",
    "https://obsproject.com/update_studio/updater.exe": "updater (tracked separately)",
    "https://obsproject.com/osx_update/updates_arm64_v2.xml": "Sparkle feed (tracked separately)",
    "https://obsproject.com/logs/upload": "log/crash upload (tracked separately)",
}


def scanned_files():
    for path in sorted((ROOT / "frontend").rglob("*")):
        if path.is_file() and path.suffix in SOURCE_SUFFIXES and "data" not in path.relative_to(ROOT).parts:
            yield path
    yield from sorted(ROOT.glob("plugins/*/data/package.json"))


class NoUpstreamRuntimeFeedsTest(unittest.TestCase):
    def test_whatsnew_defaults_off(self):
        cmake = (ROOT / "frontend/cmake/feature-whatsnew.cmake").read_text()
        match = re.search(r"option\(ENABLE_WHATSNEW\s+\"[^\"]*\"\s+(\w+)\)", cmake)
        self.assertIsNotNone(match, "ENABLE_WHATSNEW option not found")
        self.assertEqual(
            match.group(1),
            "OFF",
            "ENABLE_WHATSNEW must default OFF: it fetches upstream's whatsnew.json",
        )

    def test_whatsnew_reads_the_obs_rust_release_feed(self):
        source = (ROOT / "frontend/utility/WhatsNewInfoThread.cpp.hpp").read_text()
        urls = re.findall(r"#define \w+_WHATSNEW_URL \"([^\"]+)\"", source)
        self.assertEqual(len(urls), 3, "expected Mac, Windows and Linux What's New URLs")
        for url in urls:
            self.assertEqual(url, WHATSNEW_FEED, "What's New must read obs-rust's release feed (#1882)")

    def test_about_box_does_not_fetch_patreon_list(self):
        for path in scanned_files():
            self.assertFalse(
                "patreon/about-box.json" in path.read_text(errors="replace"),
                f"{path.relative_to(ROOT)} fetches upstream's Patreon list",
            )

    def test_every_obsproject_url_is_allowed(self):
        unknown = []
        for path in scanned_files():
            for line_no, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
                for url in URL.findall(line):
                    if url not in ALLOWED:
                        unknown.append(f"{path.relative_to(ROOT)}:{line_no}: {url}")
        self.assertEqual(
            unknown,
            [],
            "new obsproject.com URL; decide keep/remove and add it to ALLOWED with a reason",
        )

    def test_allow_list_has_no_patreon_feed(self):
        self.assertFalse(any("patreon" in url for url in ALLOWED))


if __name__ == "__main__":
    unittest.main()
