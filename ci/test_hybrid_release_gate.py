"""Publication cannot silently bypass the hybrid implementation release gates."""

import copy
import unittest
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location("hybrid_release_gate", Path(__file__).with_name("hybrid_release_gate.py"))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
validate = module.validate


class HybridReleaseGateTest(unittest.TestCase):
    def test_pending_evidence_refuses_publication(self):
        with self.assertRaisesRegex(ValueError, "approval is pending"):
            validate({"approved": False})

    def test_approval_alone_cannot_waive_required_evidence(self):
        with self.assertRaises(ValueError):
            validate({"approved": True})

    def test_reviewed_evidence_requires_consecutive_stability_days(self):
        evidence = {
            "approved": True,
            "candidate": "a" * 40,
            "compatibility": ["https://example.com/platform-matrix"],
            "application": ["https://example.com/runtime-logs"],
            "performance": ["https://example.com/benchmarks"],
            "stability_days": [f"2026-10-{day:02d}" for day in range(1, 15)],
            "review": "https://example.com/maintainer-review",
        }
        validate(evidence)
        broken = copy.deepcopy(evidence)
        broken["stability_days"][-1] = "2026-10-15"
        with self.assertRaisesRegex(ValueError, "consecutive"):
            validate(broken)
