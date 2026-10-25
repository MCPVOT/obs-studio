#!/usr/bin/env -S uv run --script
"""The .deb version is the OBS-Studio-Rust release version (#2299).

    uv run python -m unittest ci/test_deb_version.py
"""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HELPER = ROOT / "cmake/linux/deb-version.cmake"
OLD = "33.0.0-beta6-1902-g582a4c3ed"


def _order(c: str) -> int:
    if c == "~":
        return -1
    if c.isalpha():
        return ord(c)
    return ord(c) + 256


def _cmp_part(a: str, b: str) -> int:  # noqa: C901
    """dpkg's verrevcmp for an upstream version or a revision."""
    i = j = 0
    while i < len(a) or j < len(b):
        diff = 0
        while (i < len(a) and not a[i].isdigit()) or (j < len(b) and not b[j].isdigit()):
            ac = _order(a[i]) if i < len(a) and not a[i].isdigit() else 0
            bc = _order(b[j]) if j < len(b) and not b[j].isdigit() else 0
            if ac != bc:
                return ac - bc
            i += 1
            j += 1
        while i < len(a) and a[i] == "0":
            i += 1
        while j < len(b) and b[j] == "0":
            j += 1
        while i < len(a) and a[i].isdigit() and j < len(b) and b[j].isdigit():
            if not diff:
                diff = int(a[i]) - int(b[j])
            i += 1
            j += 1
        if i < len(a) and a[i].isdigit():
            return 1
        if j < len(b) and b[j].isdigit():
            return -1
        if diff:
            return diff
    return 0


def dpkg_cmp(a: str, b: str) -> int:
    def split(v: str) -> tuple[int, str, str]:
        epoch, _, rest = v.rpartition(":") if ":" in v else ("0", "", v)
        up, _, rev = rest.rpartition("-") if "-" in rest else (rest, "", "")
        return int(epoch), up, rev

    ea, ua, ra = split(a)
    eb, ub, rb = split(b)
    if ea != eb:
        return ea - eb
    return _cmp_part(ua, ub) or _cmp_part(ra, rb)


def deb_version(release: str) -> str:
    """Run the helper the way cpackconfig.cmake does; return epoch:version."""
    with tempfile.TemporaryDirectory() as root:
        driver = Path(root) / "probe.cmake"
        driver.write_text(
            f'include("{HELPER.as_posix()}")\n'
            f'set(OBS_RUST_RELEASE_VERSION "{release}")\n'
            "obs_deb_version(version epoch)\n"
            'message(STATUS "DEB=${epoch}:${version}")\n'
        )
        result = subprocess.run(["cmake", "-P", str(driver)], capture_output=True, text=True)
    out = result.stdout + result.stderr
    if result.returncode != 0:
        raise AssertionError(out)
    line = next(l for l in out.splitlines() if "DEB=" in l)
    return line.split("DEB=", 1)[1].strip()


class DebVersion(unittest.TestCase):
    def test_comparator_matches_dpkg_on_known_pairs(self):
        self.assertLess(dpkg_cmp("0.1.0~beta.1", "0.1.0"), 0)
        self.assertLess(dpkg_cmp("33.0.0-12-gabc", OLD), 0)
        self.assertGreater(dpkg_cmp("1:0.1.0", OLD), 0)

    def test_release_version_upgrades_in_order(self):
        self.assertTrue(HELPER.exists(), f"missing {HELPER.relative_to(ROOT)}")
        day1 = deb_version("0.1.0-beta.20261023")
        day2 = deb_version("0.1.0-beta.20261024")
        final = deb_version("0.1.0")
        self.assertEqual(day1, "1:0.1.0~beta.20261023")
        self.assertEqual(final, "1:0.1.0")
        chain = [OLD, day1, day2, final]
        for older, newer in zip(chain, chain[1:]):
            self.assertLess(dpkg_cmp(older, newer), 0, f"{older} -> {newer} is a downgrade")

    def test_cpack_uses_the_helper_for_rust_releases(self):
        text = (ROOT / "cmake/linux/cpackconfig.cmake").read_text()
        self.assertIn("include(deb-version)", text)
        self.assertIn("obs_deb_version(CPACK_DEBIAN_PACKAGE_VERSION CPACK_DEBIAN_PACKAGE_EPOCH)", text)
        self.assertIn("OBS_RUST_RELEASE_VERSION", text)


if __name__ == "__main__":
    unittest.main()
