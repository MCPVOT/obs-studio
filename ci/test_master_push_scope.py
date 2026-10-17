"""Master warming must include every commit in a batched push (#1286)."""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class MasterPushScopeTest(unittest.TestCase):
    def test_push_scope_uses_the_event_base_instead_of_only_the_last_commit(self):
        text = (ROOT / ".github/workflows/build-project.yaml").read_text()
        job = re.search(r"^  check-event:\n(.*?)(?=^  [a-z0-9-]+:\n)", text, re.M | re.S).group(1)
        self.assertIn("github.event.before", job)
        self.assertNotIn('base="HEAD^1"', job)
        self.assertIn("github.event.merge_group.base_sha", job)
        self.assertIn('base="origin/${GITHUB_BASE_REF}"', job)
        self.assertIn('ci/ci_scope.py "${base}"', job)


if __name__ == "__main__":
    unittest.main()
