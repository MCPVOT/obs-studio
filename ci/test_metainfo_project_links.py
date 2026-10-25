#!/usr/bin/env -S uv run --script
"""The Rust build's AppStream metainfo and .deb name obs-rust as developer,
homepage, bug tracker and source (#2302), so software centres send reports
about this build to this project. The upstream brand keeps upstream's links.

    uv run --no-project ci/test_metainfo_project_links.py
"""

from __future__ import annotations

import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "frontend/cmake/linux/com.obsproject.Studio.metainfo.xml.in"
RUST = "https://github.com/obs-rust/obs-studio"


def configure(brand: bool) -> ET.Element:
    with tempfile.TemporaryDirectory(prefix="metainfo-") as d:
        driver = Path(d) / "driver.cmake"
        driver.write_text(
            f"set(OBS_BRAND_RUST {'ON' if brand else 'OFF'})\n"
            f'include("{(ROOT / "cmake/common/app-identity.cmake").as_posix()}")\n'
            f'configure_file("{TEMPLATE.as_posix()}" "{d}/out.xml" @ONLY)\n',
            encoding="utf-8",
        )
        subprocess.run(["cmake", "-P", str(driver)], cwd=d, check=True, capture_output=True, text=True)
        return ET.parse(Path(d) / "out.xml").getroot()


def urls(root: ET.Element) -> dict[str, str]:
    return {u.get("type"): u.text for u in root.findall("url")}


class MetainfoProjectLinksTest(unittest.TestCase):
    def test_rust_brand_links_to_obs_rust(self):
        root = configure(True)
        got = urls(root)
        self.assertEqual(got["homepage"], RUST)
        self.assertEqual(got["bugtracker"], f"{RUST}/issues")
        self.assertEqual(got["vcs-browser"], RUST)
        developer = root.find("developer")
        self.assertEqual(developer.get("id"), "io.github.obs_rust")
        self.assertEqual(developer.findtext("name"), "obs-rust")

    def test_upstream_brand_keeps_upstream_links(self):
        root = configure(False)
        got = urls(root)
        self.assertEqual(got["homepage"], "https://obsproject.com")
        self.assertEqual(got["bugtracker"], "https://github.com/obsproject/obs-studio/issues")
        self.assertEqual(root.find("developer").get("id"), "com.obsproject")

    def test_deb_names_obs_rust(self):
        cpack = (ROOT / "cmake/linux/cpackconfig.cmake").read_text()
        self.assertIn("CPACK_DEBIAN_PACKAGE_HOMEPAGE", cpack)
        self.assertIn("OBS_PROJECT_HOMEPAGE", cpack)
        self.assertIn("OBS_PROJECT_DEVELOPER_NAME", cpack)

    def test_packaged_check_reads_the_links(self):
        self.assertIn("bugtracker", (ROOT / "ci/app_identity_check.py").read_text())


if __name__ == "__main__":
    unittest.main()
