"""Queue compiler objects must not share nightly-only LRU protection."""
from pathlib import Path
import json
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

    def test_reallocation_does_not_expand_combined_allowance(self):
        owners = {self.owner(key) for key in ("zccache-tier2-linux-both-seed", "zccache-ubuntu-seed",
                                             "zccache-appimage-seed", "zccache-sanitize-seed")}
        footprint = 0
        for owner in owners:
            family = self.families[owner]
            footprint += (sum(size(shape["max"]) for shape in family["shapes"].values())
                          if family.get("shapes") else size(family["max"]))
        # Ubuntu gains capacity from measured over-sized native/dependency
        # allowances, rather than adding to the repository's overcommit.
        donors = ("windows-tier2-ccache", "macos-tier2-xcas", "libs-qt", "libs-ffmpeg")
        footprint += sum(size(self.families[name]["max"]) for name in donors)
        self.assertLessEqual(footprint, (1700 + 800 + 500 + 1024 + 500) * MIB)

    def test_both_compatible_ubuntu_modes_fit_without_sharing_nightly_lru(self):
        historical = json.loads((ROOT / "docs/ci-evidence/2026-10-18-ubuntu-retention-plan.json").read_text())
        owners = {self.owner(entry["key"]) for entry in historical["ubuntu_entries"]}
        self.assertEqual(len(owners), 1)
        owner = owners.pop()
        for key in ("zccache-appimage-seed", "zccache-sanitize-seed", "zccache-tier2-linux-both-seed"):
            self.assertNotEqual(owner, self.owner(key))
        family = self.families[owner]
        self.assertEqual(family["evict"], "lru")
        required = sum(entry["size_in_bytes"] for entry in historical["ubuntu_entries"])
        self.assertGreaterEqual(size(family["max"]), required)

    def test_reduced_allowances_retain_measured_seeds_with_growth_room(self):
        snapshot = json.loads((ROOT / "docs/ci-evidence/2026-10-21-cache-retention-report.json").read_text())
        for name in ("windows-tier2-ccache", "macos-tier2-xcas", "libs-qt", "libs-ffmpeg"):
            with self.subTest(family=name):
                measured = snapshot["families"][name]["listed_bytes"]
                self.assertGreater(measured, 0)
                self.assertGreaterEqual(size(self.families[name]["max"]), 2 * measured)


if __name__ == "__main__":
    unittest.main()
