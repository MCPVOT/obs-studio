"""Prepared runtimes must satisfy the current artifact's exact closure."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent


class SelectionTest(unittest.TestCase):
    def load(self):
        spec = importlib.util.spec_from_file_location(
            'runtime_selection', ROOT / 'rust/tools/linux-validate/runtime-selection.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_unqualified_native_package_matches_dpkg_architecture(self):
        self.assertTrue(self.load().satisfies('libvlc-dev=3.0.23-1\n',
                                             'libvlc-dev:amd64\t3.0.23-1\n'))

    def test_missing_or_new_version_requires_stock_fallback(self):
        module = self.load()
        self.assertFalse(module.satisfies('a=2\n', 'a\t1\n'))
        self.assertFalse(module.satisfies('b=1\n', 'a\t1\n'))

    def test_foreign_architecture_does_not_match_native(self):
        self.assertFalse(self.load().satisfies('a:arm64=1\n', 'a:amd64\t1\n'))

    def test_invalid_and_duplicate_requirements_fail_closed(self):
        module = self.load()
        for text in ('', 'a\n', '--yes=1\n', 'a=1\na=2\n', 'a=1\na:amd64=1\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                module.satisfies(text, 'a\t1\n')

    def test_ambiguous_inventory_fails_closed(self):
        with self.assertRaises(ValueError):
            self.load().satisfies('a=1\n', 'a\t1\na:amd64\t2\n')


if __name__ == '__main__':
    unittest.main()
