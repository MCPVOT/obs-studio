"""Cook cache namespaces must reflect the measured consumer policy (#1286)."""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def jobs(path):
    text = path.read_text().split("\njobs:\n", 1)[1]
    parts = re.split(r"^  ([a-z0-9-]+):\s*$", text, flags=re.M)
    return dict(zip(parts[1::2], parts[2::2]))


class CookProfilesTest(unittest.TestCase):
    def test_build_consumers_declare_the_profile_policy(self):
        expected = {
            "rust-tests-linux": "${{ needs.check-event.outputs.core == 'true' && '--release' || '--profile dev' }}",
            "rust-cross-lint": "--profile dev",
            "rust-cross-lint-release": "--profile dev",
            "rust-tests": "--profile dev",
            "rust-linux-tier2-build": "--release",
            "rust-tier2": "--release",
        }
        workflow = jobs(ROOT / ".github/workflows/build-project.yaml")
        for name, flags in expected.items():
            with self.subTest(job=name):
                self.assertIn("prebuild-deps-flags: " + flags, workflow[name])
                self.assertNotIn("prebuild-deps: none", workflow[name])

    def test_release_static_library_uses_release_cook(self):
        found = 0
        for name, job in jobs(ROOT / ".github/workflows/release-rust.yaml").items():
            if "zackees/setup-soldr@" in job:
                found += 1
                self.assertIn("prebuild-deps-flags: --release", job, name)
        self.assertEqual(found, 1)


if __name__ == "__main__":
    unittest.main()
