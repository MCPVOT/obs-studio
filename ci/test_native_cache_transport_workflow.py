#!/usr/bin/env -S uv run --script
"""Trust and scope checks for the opt-in native cache transport route."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent


class TransportWorkflowTest(unittest.TestCase):
    def test_publisher_is_manual_master_only_separate_and_serialized(self):
        workflow = (ROOT / '.github/workflows/ci.yml').read_text()
        job = workflow.split('  native-cache-transport:\n', 1)[1].split('\n  native-cache-consumer:', 1)[0].split('\n  ci-gate:', 1)[0]
        self.assertIn("github.event_name == 'workflow_dispatch' && github.ref == 'refs/heads/master' && inputs.native-cache-transport-probe", job)
        self.assertIn('needs: build-project', job)
        self.assertIn('contents: write', job)
        self.assertIn('cancel-in-progress: false', job)
        self.assertIn('fail-fast: false', job)
        self.assertIn('fail-on-cache-miss: true', job)
        self.assertNotIn('restore-keys:', job)
        self.assertIn('cache-hit !=', job)
        self.assertIn('NATIVE_CACHE_JOB_TOKEN: ${{ github.token }}', job)
        self.assertNotIn('ci/native_cache_transport.py consume', job)
        self.assertIn('--expected', job)
        build_job = workflow.split('  build-project:\n', 1)[1].split('\n  #', 1)[0]
        self.assertIn('contents: read', build_job)
        self.assertNotIn('contents: write', build_job)

    def test_roundtrip_has_a_separate_read_only_token_and_required_source(self):
        workflow = (ROOT / '.github/workflows/ci.yml').read_text()
        self.assertIn('  native-cache-consumer:\n', workflow)
        job = workflow.split('  native-cache-consumer:\n', 1)[1].split('\n  ci-gate:', 1)[0]
        self.assertIn('needs: native-cache-transport', job)
        self.assertIn('always() && !cancelled()', job)
        self.assertIn("github.event_name == 'workflow_dispatch' && github.ref == 'refs/heads/master' && inputs.native-cache-transport-probe", job)
        self.assertIn('contents: read', job)
        self.assertIn('actions: read', job)
        self.assertNotIn('contents: write', job)
        self.assertIn('NATIVE_CACHE_JOB_TOKEN: ${{ github.token }}', job)
        self.assertIn('--require-hit --expected-source', job)
        self.assertIn('native-cache-build-identity/native-cache-identity.json', job)
        self.assertIn('fail-fast: false', job)
        self.assertIn('runs-on: ubuntu-26.04', job)
        gate = workflow.split('  ci-gate:\n', 1)[1]
        needs = next(line.strip() for line in gate.splitlines() if line.strip().startswith('needs: ['))
        self.assertIn('native-cache-consumer', needs.removeprefix('needs: [').removesuffix(']').split(', '))

    def test_identity_evidence_is_default_off_and_matches_native_axes(self):
        workflow = (ROOT / '.github/workflows/ci.yml').read_text()
        build = (ROOT / '.github/workflows/build-project.yaml').read_text()
        for text in (workflow, build):
            setting = text.split('      native-cache-transport-probe:\n', 1)[1].split('\n      ', 1)[0]
            # Remaining fields are read from the complete input block below.
            self.assertTrue(setting)
            self.assertIn('default: false', text.split('      native-cache-transport-probe:\n', 1)[1].split('\n      native-cache-probe:', 1)[0])
        for platform, boundary in (('macos', '\n  ubuntu-build:'), ('windows', '\n  # zackees/ci.yml GATE-008')):
            job = build.split('  ' + platform + '-build:\n', 1)[1].split(boundary, 1)[0]
            self.assertIn('inputs.native-cache-transport-probe', job)
            self.assertIn('ci/native_cache_identity.py', job)
            self.assertIn('native-cache-identity-' + platform, job)
            self.assertIn('retention-days: 1', job)


if __name__ == '__main__':
    unittest.main()
