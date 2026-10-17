"""Queue compiler objects must not share nightly-only LRU protection."""
from pathlib import Path
import tomllib
import unittest


ROOT = Path(__file__).resolve().parent.parent
MIB = 1024 ** 2


def size(value):
    units = {"MB": MIB, "GB": 1024 ** 3}
    return int(float(value[:-2]) * units[value[-2:]])


class Tier2RetentionTest(unittest.TestCase):
    def setUp(self):
        self.families = tomllib.loads((ROOT / "ci.toml").read_text())["cache"]["family"]

    def owner(self, key):
        # Literal prefix classification is first-match at the pinned ci-lint.
        return next(name for name, family in self.families.items()
                    if family.get("prefix") and key.startswith(family["prefix"]))

    def test_queue_objects_have_independent_lru_protection(self):
        owner = self.owner("zccache-tier2-linux-both-seed")
        for key in ("zccache-ubuntu-seed", "zccache-appimage-seed", "zccache-sanitize-seed"):
            with self.subTest(key=key):
                self.assertNotEqual(owner, self.owner(key))
        family = self.families[owner]
        self.assertEqual(family["evict"], "lru")
        self.assertGreaterEqual(size(family["max"]), 475427223)

    def test_split_does_not_expand_combined_allowance(self):
        owners = {self.owner(key) for key in ("zccache-tier2-linux-both-seed", "zccache-ubuntu-seed",
                                             "zccache-appimage-seed", "zccache-sanitize-seed")}
        footprint = 0
        for owner in owners:
            family = self.families[owner]
            footprint += (sum(size(shape["max"]) for shape in family["shapes"].values())
                          if family.get("shapes") else size(family["max"]))
        self.assertLessEqual(footprint, 1700 * MIB)


if __name__ == "__main__":
    unittest.main()
