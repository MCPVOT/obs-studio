"""Packaging validation belongs to nightly/release runs, outside PR CI."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class QueueReleasePolicyTest(unittest.TestCase):
    def test_pr_workflow_has_no_packaging_jobs_or_dependencies(self):
        text = (ROOT / '.github/workflows/ci.yml').read_text()
        self.assertNotIn('  appimage:', text)
        self.assertNotIn('  appimage-changes:', text)
        self.assertNotIn('workflows/appimage.yaml', text)
        gate = text.split('  ci-gate:', 1)[1]
        self.assertNotIn('appimage', gate)
        self.assertIn('build-project', gate)
        self.assertIn('check-format', gate)

    def test_nightly_release_retains_packaging_validation(self):
        text = (ROOT / '.github/workflows/release-rust.yaml').read_text()
        self.assertIn('  schedule:', text)
        self.assertIn('uses: ./.github/workflows/appimage.yaml', text)
        self.assertIn("needs.appimage.result == 'success'", text)

if __name__ == '__main__':
    unittest.main()
