"""Keep observed default-CodeQL overlay caches within a bounded family."""

import tomllib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class CodeqlCacheRetentionTest(unittest.TestCase):
    def test_observed_overlay_keys_have_a_bounded_lru_family(self):
        families = tomllib.loads((ROOT / "ci.toml").read_text())["cache"]["family"]
        keys = (
            "codeql-overlay-base-database-1-2ecd53ea14c312b7-cpp-2.27.2-a9952c82d13758f4a326c9bd99280d4c49dc58d4-39057995138-1",
            "codeql-overlay-base-database-1-d953d79b74456ce0-python-2.27.2-5c77c49868f060d8ff34301ef618ab9971b9fdee-39058652739-1",
        )
        for key in keys:
            matches = [f for f in families.values() if f.get("prefix") and key.startswith(f["prefix"])]
            self.assertEqual(len(matches), 1, key)
            self.assertEqual(matches[0]["evict"], "lru")
            # The 131 MB C++ and ~11 MB Python working bases fit together.
            self.assertEqual(matches[0]["max"], "200MB")


if __name__ == "__main__":
    unittest.main()
