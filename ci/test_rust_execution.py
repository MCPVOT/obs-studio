"""The faster Rust runner must retain scope, doctests and failure propagation (#1694)."""
import importlib.util
from pathlib import Path
import subprocess
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('execution_gate', Path(__file__).with_name('local_gate.py'))
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


class RustExecutionTest(unittest.TestCase):
    def calls(self, packages, tool=Path('/tools/cargo-nextest'), failure=None):
        with mock.patch.object(gate, 'ensure_nextest', return_value=tool, create=True), \
             mock.patch.object(gate, 'soldr_cargo', side_effect=failure) as cargo, \
             mock.patch.object(gate.os, 'cpu_count', return_value=4):
            gate.rust_tests(packages)
            return cargo.call_args_list

    def test_workspace_uses_bounded_runner_then_all_doctests(self):
        calls = self.calls(['--workspace'])
        self.assertEqual(calls[0].args[0], ['nextest', 'run', '--workspace', '--locked', '--no-fail-fast', '--test-threads', '4',
            '--profile', 'default', '--config-file', str(Path(gate.__file__).with_name('nextest.toml')),
            '--user-config-file', 'none', '--ignore-default-filter', '--retries', '0', '--no-tests', 'fail'])
        self.assertTrue(calls[0].args[1]['PATH'].startswith('/tools:'))
        self.assertEqual(calls[1].args[0], ['test', '--workspace', '--locked', '--doc'])

    def test_changed_crate_scope_applies_to_runner_and_doctests(self):
        packages = ['-p', 'obs-data', '-p', 'obs-util']
        calls = self.calls(packages)
        for call in calls:
            for package in ('obs-data', 'obs-util'):
                self.assertIn(package, call.args[0])
            self.assertNotIn('--workspace', call.args[0])

    def test_only_scoped_packages_retain_cargo_zero_test_behavior(self):
        for scope, expected in [(['--workspace'], 'fail'), (['-p', 'libobs-rust'], 'pass')]:
            args = self.calls(scope)[0].args[0]
            self.assertEqual(args[args.index('--no-tests') + 1], expected)

    def test_unsupported_host_retains_the_complete_cargo_test_command(self):
        calls = self.calls(['--workspace'], tool=None)
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0].args[0], ['test', '--workspace', '--locked'])

    def test_failed_unit_suite_still_runs_doctests_and_fails_the_gate(self):
        error = subprocess.CalledProcessError(100, ['nextest'])
        with mock.patch.object(gate, 'ensure_nextest', return_value=Path('/tools/cargo-nextest'), create=True), \
             mock.patch.object(gate, 'soldr_cargo', side_effect=[error, None]) as cargo, \
             self.assertRaises(subprocess.CalledProcessError):
            gate.rust_tests(['--workspace'])
        self.assertEqual(cargo.call_args_list[-1].args[0], ['test', '--workspace', '--locked', '--doc'])

    def test_executor_sources_are_attested_inputs_and_trust_surfaces(self):
        import tomllib
        data = tomllib.loads((Path(__file__).resolve().parent.parent / 'local-gate.toml').read_text())
        for path in ('ci/local_gate.py', 'ci/nextest_tool.py', 'ci/nextest.toml'):
            self.assertIn(path, data['gate']['lanes']['rust-test']['keep'])
        self.assertIn('ci/nextest_tool.py', data['gate']['trust']['surfaces'])

    def test_failed_doctest_fails_the_gate(self):
        error = subprocess.CalledProcessError(101, ['test', '--doc'])
        with self.assertRaises(subprocess.CalledProcessError):
            self.calls(['--workspace'], failure=[None, error])


if __name__ == '__main__':
    unittest.main()
