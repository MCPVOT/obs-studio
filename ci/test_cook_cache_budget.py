"""The measured release and dev Linux cook snapshots must coexist."""

import re
import tomllib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class CookCacheBudgetTest(unittest.TestCase):
    def test_linux_allowance_covers_both_live_profile_archives(self):
        config = tomllib.loads((ROOT / "ci.toml").read_text())
        family = config["cache"]["family"]["deps"]
        size = family["shapes"]["linux"]["max"]
        match = re.fullmatch(r"([0-9.]+)(MB|GB)", size)
        self.assertIsNotNone(match)
        allowance = float(match[1]) * {"MB": 1_000_000, "GB": 1_000_000_000}[match[2]]
        # API snapshot after master bf187 seeded the explicit dev profile.
        required = 621_978_240 + 636_783_114
        self.assertGreaterEqual(allowance, required)
        self.assertEqual(family["evict"], "lru")
        self.assertEqual(config["cache"]["budget"], "10GB")


if __name__ == "__main__":
    unittest.main()
