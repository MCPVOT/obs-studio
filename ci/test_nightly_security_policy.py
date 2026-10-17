"""Security analysis must not consume PR or merge queue runners."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class NightlySecurityPolicy(unittest.TestCase):
    def test_codeql_lives_in_existing_nightly_workflow(self):
        text = (ROOT / '.github/workflows/scheduled.yaml').read_text()
        self.assertIn('  codeql:', text)
        section = text.split('  codeql:', 1)[1].split('\n  upload-language-files:', 1)[0]
        self.assertIn("github.event_name == 'schedule' || github.event_name == 'workflow_dispatch'", section)
        self.assertIn('language: [actions, c-cpp, python, rust]', section)
        self.assertIn('build-mode: none', section)
        self.assertIn('github/codeql-action/init@', section)
        self.assertIn('github/codeql-action/analyze@', section)

    def test_analysis_workflow_rejects_queue_calls(self):
        text = (ROOT / '.github/workflows/analyze-project.yaml').read_text()
        self.assertIn("github.event_name == 'schedule' || github.event_name == 'workflow_dispatch'", text)

if __name__ == '__main__':
    unittest.main()
