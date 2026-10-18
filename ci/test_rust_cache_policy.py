"""Every remote Rust consumer retains the stale-source safeguard (#1286)."""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKFLOWS = ("build-project.yaml", "release-rust.yaml")


class RustCachePolicyTest(unittest.TestCase):
    def test_all_consumers_inherit_the_rust_wrapper_bypass(self):
        for name in WORKFLOWS:
            with self.subTest(workflow=name):
                text = (ROOT / ".github/workflows" / name).read_text()
                # Workflow-level env reaches setup/cook and every native matrix
                # consumer, including the Linux job-container build.
                env = re.search(r"^env:\n((?:[ \t].*\n|\n)+)", text, re.M)
                self.assertIsNotNone(env, "Rust consumers need a shared safeguard")
                self.assertRegex(env[1], r"(?m)^  SOLDR_RUSTC_WRAPPER: none$")
                overrides = re.findall(r"(?m)^\s+SOLDR_RUSTC_WRAPPER:\s*(\S+)", text)
                self.assertTrue(overrides)
                self.assertEqual(set(overrides), {"none"}, "Do not re-enable an unsafe wrapper in a child job")

    def test_rust_bypass_does_not_disable_native_compiler_caches(self):
        for name in WORKFLOWS:
            with self.subTest(workflow=name):
                text = (ROOT / ".github/workflows" / name).read_text()
                self.assertNotRegex(text, r"(?m)^\s+ZCCACHE_DISABLE:")
                self.assertNotRegex(text, r"(?m)^\s+CCACHE_DISABLE:")


if __name__ == "__main__":
    unittest.main()
