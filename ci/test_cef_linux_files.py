#!/usr/bin/env -S uv run --script
"""Every CEF file the Linux obs-browser build copies or installs must be in the
pinned CEF bundle (#1989).

cmake/linux/helpers.cmake lists the CEF files that obs-browser's post-build
step copies (`copy_if_different`) and that `install(FILES ...)` installs. A
file the bundle does not ship fails the build: CEF 8078 dropped the ANGLE
libraries (libEGL.so, libGLESv2.so) on Linux and the Flatpak build broke.

ci/data/cef-linux-x86_64-files.txt is the bundle's Release/ and Resources/
file list. It names the bundle's sha256, which must match the CEF module of
the Flatpak manifest, so a CEF bump has to refresh the list too.
"""

from pathlib import Path
import json
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent
HELPERS = ROOT / "cmake/linux/helpers.cmake"
MANIFEST = ROOT / "build-aux/com.obsproject.Studio.json"
FILES = ROOT / "ci/data/cef-linux-x86_64-files.txt"


def bundle_files():
    lines = FILES.read_text().splitlines()
    sha = next(l.split()[-1] for l in lines if l.startswith("# sha256 "))
    files = {l.strip() for l in lines if l.strip() and not l.startswith("#")}
    return sha, files


def obs_browser_block():
    text = HELPERS.read_text()
    start = text.index("elseif(${target} STREQUAL obs-browser)")
    end = text.index("\n  elseif(", start)
    return text[start:end]


def referenced_files(block):
    """Release/<x> for ${cef_location}/<x>, Resources/<x> for
    ${cef_root_location}/Resources/<x>; directories (locales) excluded."""
    found = set()
    for name in re.findall(r'"\$\{cef_location\}/([^"/]+)"', block):
        found.add(f"Release/{name}")
    for name in re.findall(r'"\$\{cef_root_location\}/Resources/([^"/]+)"', block):
        if name != "locales":
            found.add(f"Resources/{name}")
    return found


class CefLinuxFilesTest(unittest.TestCase):
    def test_file_list_matches_the_pinned_bundle(self):
        sha, _ = bundle_files()
        modules = json.loads(MANIFEST.read_text())["modules"]
        cef = next(m for m in modules if isinstance(m, dict) and m.get("name") == "cef")
        shas = {s.get("sha256") for s in cef["sources"] if isinstance(s, dict)}
        self.assertIn(sha, shas, f"{FILES.name} is for another CEF bundle; regenerate it")

    def test_helpers_reference_files(self):
        # Guard the parser: the block must still list the known files.
        refs = referenced_files(obs_browser_block())
        self.assertIn("Release/chrome-sandbox", refs)
        self.assertIn("Resources/icudtl.dat", refs)

    def test_every_copied_or_installed_file_is_in_the_bundle(self):
        _, files = bundle_files()
        missing = sorted(referenced_files(obs_browser_block()) - files)
        self.assertEqual(missing, [], "helpers.cmake copies/installs CEF files the bundle lacks")


if __name__ == "__main__":
    unittest.main()
