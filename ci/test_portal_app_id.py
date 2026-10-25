#!/usr/bin/env -S uv run --script
"""The GlobalShortcuts portal registers the desktop id the build installs.

The branded .deb and AppImage install io.github.obs_rust.OBSStudioRust.desktop;
registering com.obsproject.Studio there names a desktop file that is not
installed, so background shortcuts fail (#2346).

    uv run python -m unittest ci/test_portal_app_id.py
"""

import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "libobs" / "obs-nix-portal-shortcuts.c.h"
HOSTS = [ROOT / "libobs" / "cmake" / "os-linux.cmake", ROOT / "libobs" / "cmake" / "os-freebsd.cmake"]


def app_id_block(text: str) -> str:
    """The preprocessor lines that define APP_ID."""
    start = text.index("#define REGISTRY_IFACE")
    end = text.index("\n\n", start)
    return text[start:end] + "\n"


class PortalAppId(unittest.TestCase):
    def expand(self, *defines: str) -> str:
        cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if cc is None:
            self.skipTest("no C compiler")
        with tempfile.TemporaryDirectory(prefix="obs-portal-app-id-") as scratch:
            source = Path(scratch) / "id.c"
            source.write_text(app_id_block(SOURCE.read_text()) + "const char *id = APP_ID;\n")
            out = subprocess.check_output([cc, "-E", "-P", *defines, str(source)], text=True)
        return re.search(r"const char \*id = (.+);", out).group(1)

    def test_branded_build_registers_its_desktop_id(self):
        self.assertEqual(self.expand('-DOBS_PORTAL_APP_ID="io.github.obs_rust.OBSStudioRust"'),
                         '"io.github.obs_rust.OBSStudioRust"')

    def test_unbranded_build_keeps_upstreams_id(self):
        self.assertEqual(self.expand(), '"com.obsproject.Studio"')

    def test_libobs_passes_the_desktop_id_with_the_portal_source(self):
        for host in HOSTS:
            with self.subTest(host=host.name):
                text = host.read_text()
                portal = text.index("build/libobs.nix-portal-shortcuts+.h.c")
                block = text[portal:text.index("endif()", portal)]
                self.assertIn('OBS_PORTAL_APP_ID="${OBS_DESKTOP_ID}"', block)


if __name__ == "__main__":
    unittest.main()
