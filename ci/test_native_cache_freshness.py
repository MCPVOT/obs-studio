#!/usr/bin/env -S uv run --script
"""Freshness qualification cannot pass an old binary or overwrite unrelated edits."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("freshness", Path(__file__).with_name("native_cache_freshness.py"))
freshness = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(freshness)
CONTEXT = {"GITHUB_REPOSITORY": "obs-rust/obs-studio", "GITHUB_REF": "refs/heads/master",
           "GITHUB_EVENT_NAME": "workflow_dispatch", "GITHUB_ACTIONS": "true",
           "GITHUB_SHA": "a" * 40, "GITHUB_RUN_ID": "123", "GITHUB_RUN_ATTEMPT": "1"}
SOURCE = b'// fixture\r\n\tblog(LOG_INFO, "Qt Version: %s (runtime), %s (compiled)", qVersion(), QT_VERSION_STR);\r\n'


class NativeCacheFreshnessTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / "frontend/OBSApp.cpp.hpp"
        self.source.parent.mkdir()
        self.source.write_bytes(SOURCE)
        self.state = self.root / "state.json"

    def binary(self, marker=b"", platform="windows"):
        if platform == "windows":
            path = self.root / "build_x64/install/bin/64bit/obs64.exe"
            prefix = b"MZ"
        else:
            # cmake/macos/helpers.cmake overrides the generic Unix GUI name
            # with OBS_MACOS_BUNDLE_NAME for this executable.
            path = self.root / "build_macos/OBS-Studio-Rust.app/Contents/MacOS/OBS-Studio-Rust"
            prefix = b"\xcf\xfa\xed\xfe"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(prefix + b"\0" * ((1 << 20) - len(prefix) - 3) + marker + b"\0")
        return path

    def test_overlay_installed_binary_and_exact_restore(self):
        prepared = freshness.prepare(self.root, self.state, CONTEXT)
        marker = prepared["marker"].encode()
        self.assertIn(marker, self.source.read_bytes())
        self.assertEqual(prepared["original_sha256"], hashlib.sha256(SOURCE).hexdigest())
        for platform in ("windows", "macos"):
            with self.subTest(platform=platform):
                binary = self.binary(marker, platform)
                report = freshness.verify(self.root, self.state, platform, "x64" if platform == "windows" else "arm64")
                self.assertEqual(report["status"], "fresh")
                self.assertEqual(report["binary_sha256"], hashlib.sha256(binary.read_bytes()).hexdigest())
        freshness.restore(self.root, self.state)
        self.assertEqual(self.source.read_bytes(), SOURCE)
        freshness.restore(self.root, self.state)

    def test_missing_marker_and_wrong_binary_format_fail(self):
        prepared = freshness.prepare(self.root, self.state, CONTEXT)
        binary = self.binary()
        with self.assertRaises(ValueError):
            freshness.verify(self.root, self.state, "windows", "x64")
        binary.write_bytes(prepared["marker"].encode())
        with self.assertRaises(ValueError):
            freshness.verify(self.root, self.state, "windows", "x64")
        freshness.restore(self.root, self.state)

    def test_macos_unbranded_bundle_uses_its_bundle_executable_name(self):
        prepared = freshness.prepare(self.root, self.state, CONTEXT)
        binary = self.root / 'build_macos/OBS.app/Contents/MacOS/OBS'
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b'\xcf\xfa\xed\xfe' + prepared['marker'].encode())
        report = freshness.verify(self.root, self.state, 'macos', 'x86_64')
        self.assertEqual(report['binary'], 'build_macos/OBS.app/Contents/MacOS/OBS')
        freshness.restore(self.root, self.state)

    def test_macos_linux_style_executable_cannot_qualify_the_app(self):
        prepared = freshness.prepare(self.root, self.state, CONTEXT)
        binary = self.root / 'build_macos/OBS-Studio-Rust.app/Contents/MacOS/obs-studio-rust-gui'
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b'\xcf\xfa\xed\xfe' + prepared['marker'].encode())
        with self.assertRaises(ValueError):
            freshness.verify(self.root, self.state, 'macos', 'arm64')
        freshness.restore(self.root, self.state)

    def test_cli_report_identifies_a_known_freshness_failure(self):
        freshness.prepare(self.root, self.state, CONTEXT)
        self.binary()
        report = self.root / 'report.json'
        argv = ['native_cache_freshness.py', 'verify', '--root', str(self.root), '--state', str(self.state),
                '--report', str(report), '--platform', 'windows', '--architecture', 'x64']
        with patch.object(sys, 'argv', argv):
            self.assertEqual(freshness.main(), 1)
        self.assertEqual(json.loads(report.read_text())['reason'],
                         'installed main binary lacks changed-source freshness marker')
        freshness.restore(self.root, self.state)

    def test_unrelated_edit_is_preserved(self):
        freshness.prepare(self.root, self.state, CONTEXT)
        self.source.write_bytes(b"an unrelated edit")
        with self.assertRaises(ValueError):
            freshness.restore(self.root, self.state)
        self.assertEqual(self.source.read_bytes(), b"an unrelated edit")

    def test_context_is_trusted_and_marker_is_bounded(self):
        for field, value in (("GITHUB_REPOSITORY", "another/repo"), ("GITHUB_REF", "refs/heads/other"),
                             ("GITHUB_EVENT_NAME", "pull_request"), ("GITHUB_ACTIONS", "false"),
                             ("GITHUB_SHA", '"; injected'), ("GITHUB_RUN_ID", "1\n2"),
                             ("GITHUB_RUN_ATTEMPT", "x")):
            with self.subTest(field=field), self.assertRaises(ValueError):
                freshness.prepare(self.root, self.state, {**CONTEXT, field: value})
            self.assertEqual(self.source.read_bytes(), SOURCE)
            self.assertFalse(self.state.exists())

    def test_state_is_exclusive_and_original_bytes_are_authenticated(self):
        freshness.prepare(self.root, self.state, CONTEXT)
        modified = self.source.read_bytes()
        with self.assertRaises(FileExistsError):
            freshness.prepare(self.root, self.state, CONTEXT)
        self.assertEqual(self.source.read_bytes(), modified)
        state = json.loads(self.state.read_text())
        state["original_sha256"] = "0" * 64
        self.state.write_text(json.dumps(state))
        with self.assertRaises(ValueError):
            freshness.restore(self.root, self.state)

    def test_source_drift_is_rejected(self):
        self.source.write_bytes(b"unexpected source layout")
        with self.assertRaises(ValueError):
            freshness.prepare(self.root, self.state, CONTEXT)
        self.assertFalse(self.state.exists())

    def test_source_and_binary_symlinks_are_rejected(self):
        original = self.root / 'original'
        self.source.rename(original)
        self.source.symlink_to(original)
        with self.assertRaises(ValueError):
            freshness.prepare(self.root, self.state, CONTEXT)
        self.assertTrue(self.source.is_symlink())
        # A separate fixture avoids deleting or replacing the rejected link.
        other = self.root / 'other'
        (other / 'frontend').mkdir(parents=True)
        (other / 'frontend/OBSApp.cpp.hpp').write_bytes(SOURCE)
        state = other / 'state.json'
        prepared = freshness.prepare(other, state, CONTEXT)
        target = other / 'binary'
        target.write_bytes(b'MZ' + prepared['marker'].encode())
        link = other / 'build_x64/install/bin/64bit/obs64.exe'
        link.parent.mkdir(parents=True)
        link.symlink_to(target)
        with self.assertRaises(ValueError):
            freshness.verify(other, state, 'windows', 'x64')
        freshness.restore(other, state)


class NativeReuseWorkflowTest(unittest.TestCase):
    def test_manual_source_guard_rejects_unsafe_or_confounded_requests(self):
        text = (Path(__file__).resolve().parent.parent / '.github/workflows/build-project.yaml').read_text()
        guard = text.split('          if [[ -n "$NATIVE_REUSE_SOURCE" ]]; then\n', 1)[1].split('          fi', 1)[0]
        script = 'if [[ -n "$NATIVE_REUSE_SOURCE" ]]; then\n' + guard + '\nfi'
        env = {**os.environ, **CONTEXT, 'NATIVE_REUSE_SOURCE': 'b' * 40,
               'NATIVE_TRANSPORT_PROBE': 'false', 'NATIVE_BUDGET_PROBE': 'false'}
        for overrides, expected in [({}, 0), ({'NATIVE_REUSE_SOURCE': ''}, 0),
                                    ({'NATIVE_REUSE_SOURCE': 'bad\nidentity'}, 1),
                                    ({'GITHUB_REF': 'refs/heads/other'}, 1),
                                    ({'GITHUB_REPOSITORY': 'another/repo'}, 1),
                                    ({'GITHUB_EVENT_NAME': 'pull_request'}, 1),
                                    ({'NATIVE_TRANSPORT_PROBE': 'true'}, 1),
                                    ({'NATIVE_BUDGET_PROBE': 'true'}, 1)]:
            with self.subTest(overrides=overrides):
                result = subprocess.run(['bash', '-e', '-c', script], env=env | overrides,
                                        capture_output=True, timeout=5, check=False)
                self.assertEqual(result.returncode, expected)

    def test_input_is_default_empty_and_forwarded_only_on_dispatch(self):
        root = Path(__file__).resolve().parent.parent
        for name in ('ci.yml', 'build-project.yaml'):
            text = (root / '.github/workflows' / name).read_text()
            self.assertRegex(text, r"native-cache-reuse-source:\n(?:        .*\n)*?        default: ''")
        caller = (root / '.github/workflows/ci.yml').read_text()
        self.assertIn("native-cache-reuse-source: ${{ github.event_name == 'workflow_dispatch' && inputs.native-cache-reuse-source || '' }}", caller)

    def test_real_build_uses_release_restore_then_verifies_and_restores_source(self):
        text = (Path(__file__).resolve().parent.parent / '.github/workflows/build-project.yaml').read_text()
        for name in ('macos-build', 'windows-build'):
            body = re.search(rf'^  {name}:\n(.*?)(?=^  [a-z0-9-]+:|\Z)', text, re.S | re.M).group(1)
            with self.subTest(job=name):
                prepare = body.index('- name: Prepare Native Release Reuse')
                build = body.index('- name: Build OBS Studio')
                verify = body.index('- name: Verify Native Installed Binary Freshness')
                restore = body.index('- name: Restore Native Freshness Source')
                self.assertLess(prepare, build)
                self.assertLess(build, verify)
                self.assertLess(verify, restore)
                self.assertIn('consume --require-hit --expected-source "$NATIVE_REUSE_SOURCE"', body)
                self.assertIn('native_cache_freshness.py prepare', body[prepare:build])
                self.assertIn('native_cache_freshness.py verify', body[verify:restore])
                self.assertIn("if: always() && inputs.native-cache-reuse-source != ''", body[restore:])
                self.assertIn('native_cache_freshness.py restore', body[restore:])
                self.assertIn('native-reuse-*.json', body[restore:])
                # Neither Actions compiler restoration nor publication may
                # confound this experiment's source or publish the overlay.
                for block in body.split('      - ')[1:]:
                    if 'actions/cache/restore@' in block or 'actions/cache/save@' in block:
                        self.assertIn("inputs.native-cache-reuse-source == ''", block)
                self.assertIn('steps.native-reuse.outputs.cache_path', body)


if __name__ == "__main__":
    unittest.main()
