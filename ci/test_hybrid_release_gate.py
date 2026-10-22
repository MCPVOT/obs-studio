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


class ReleaseChannelGateTest(unittest.TestCase):
    """Nightlies publish while approval is pending; blessing stays gated."""

    def test_nightly_prerelease_publishes_while_approval_is_pending(self):
        self.assertIn("pending", module.check("nightly", {"approved": False}))

    def test_blessed_promotion_still_requires_approval(self):
        with self.assertRaisesRegex(ValueError, "approval is pending"):
            module.check("blessed", {"approved": False})

    def test_unknown_channel_fails_closed(self):
        with self.assertRaises(ValueError):
            module.check("stable-ish", {"approved": False})

    def test_workflow_publishes_nightly_and_gates_certify(self):
        source = (Path(__file__).resolve().parent.parent / ".github/workflows/release-rust.yaml").read_text()
        release, certify = source.split("\n  release:", 1)[1], source.split("\n  certify:", 1)[1]
        self.assertIn("hybrid_release_gate.py --channel nightly", release.split("\n  certify:", 1)[0])
        gate = certify.index("hybrid_release_gate.py --channel blessed")
        self.assertLess(gate, certify.index("--prerelease=false --latest"))
