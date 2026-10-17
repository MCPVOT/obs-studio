"""Bound background master builds without coupling janitor/merge-queue CI."""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class MasterBuildConcurrencyTest(unittest.TestCase):
    def test_only_the_master_build_call_is_coalesced(self):
        text = (ROOT / ".github/workflows/push.yaml").read_text()
        self.assertNotRegex(text, r"^concurrency:", "never cancel the parent pre-work workflow")
        parts = re.split(r"^  ([a-z0-9-]+):\s*$", text.split("\njobs:\n", 1)[1], flags=re.M)
        jobs = dict(zip(parts[1::2], parts[2::2]))
        build = jobs["build-project"]
        self.assertIn("concurrency:", build)
        self.assertIn("github.ref == 'refs/heads/master' && 'master' || github.run_id", build)
        self.assertIn("cancel-in-progress: false", build)
        self.assertNotIn("queue: max", build)
        self.assertNotIn("concurrency:", jobs["ci-pre"])
        self.assertNotIn("needs:", jobs["ci-pre"])
        self.assertIn("./.github/workflows/build-project.yaml", build)


if __name__ == "__main__":
    unittest.main()
