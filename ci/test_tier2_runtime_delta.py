"""A prepared Tier 2 runtime survives small Ubuntu updates (#1592, #2072).

Usage:
    uv run python -m unittest ci/test_tier2_runtime_delta.py

The prepared image matched the closure exactly or was not used. One zlib
security update (1ubuntu3.1 -> 1ubuntu3.2, one of 224 packages) sent every
Tier 2 test job to the stock path, 75 s of apt instead of 20-30 s
(2026-10-24). A closure that differs in a few packages now loads the prepared
image and installs exactly those versions on top; the installed inventory is
still verified against the whole closure. A larger difference still falls back
to stock.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / 'rust/tools/linux-validate'


def load(name: str, file: str):
    spec = importlib.util.spec_from_file_location(name, TOOLS / file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


selection = load('runtime_selection', 'runtime-selection.py')
runtime = load('ci_runtime', 'ci-runtime.py')

OLD = '1:1.3.dfsg+really1.3.1-1ubuntu3.1'
NEW = '1:1.3.dfsg+really1.3.1-1ubuntu3.2'


class DeltaTest(unittest.TestCase):
    def test_delta_lists_only_missing_or_changed_packages(self):
        requirements = f'a=1\nzlib1g={NEW}\nc=3\n'
        inventory = f'a:amd64\t1\nzlib1g:amd64\t{OLD}\nb:amd64\t2\n'
        self.assertEqual(selection.delta(requirements, inventory), [f'c:amd64=3', f'zlib1g:amd64={NEW}'])

    def test_matching_closure_has_no_delta(self):
        self.assertEqual(selection.delta('a=1\n', 'a:amd64\t1\n'), [])


class ExecuteTest(unittest.TestCase):
    def descriptor(self, inventory: str) -> dict:
        return {'base': runtime.BASE, 'platform': 'linux/amd64', 'inventory': inventory,
                'image_id': 'sha256:' + 'a' * 64, 'compressed_bytes': 4,
                'archive_sha256': hashlib.sha256(b'good').hexdigest(),
                'url': 'https://github.com/obs-rust/obs-studio-libs/releases/download/runtime-1/image.tar.zst'}

    def run_execute(self, requirements: str, image_inventory: str, installed: str):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'descriptor.json').write_text(json.dumps(self.descriptor(image_inventory)))
            (root / 'packages.txt').write_text(requirements)
            args = argparse.Namespace(packages=root / 'packages.txt', descriptor=root / 'descriptor.json',
                                      state=root / 'state', cost=root / 'cost', source=root, build=root,
                                      uv=root / 'uv', archives=root, mode='ON', part='suite')
            calls = []

            def command(*argv):
                calls.append(argv)
                if argv[:2] == ('docker', 'create'):
                    return 'b' * 64
                if 'dpkg-query' in argv:
                    return installed
                return ''

            with patch.object(runtime, 'download_prepared', return_value='sha256:' + 'a' * 64), \
                    patch.object(runtime, 'command', side_effect=command), \
                    patch.object(runtime.subprocess, 'run'):
                runtime.execute(args)
            report = json.loads((root / 'state/runtime-measurement.json').read_text())
            return calls, report

    def test_small_drift_installs_the_delta_on_the_prepared_image(self):
        calls, report = self.run_execute(f'a=1\nzlib1g={NEW}\n', f'a\t1\nzlib1g\t{OLD}\n',
                                         f'a\t1\nzlib1g\t{NEW}\n')
        self.assertEqual(report['variant'], 'prepared')
        self.assertEqual(report['delta'], [f'zlib1g:amd64={NEW}'])
        installs = [c for c in calls if 'install' in c]
        self.assertEqual(len(installs), 1)
        self.assertIn(f'zlib1g:amd64={NEW}', installs[0])
        self.assertNotIn(('docker', 'pull', runtime.BASE), calls)

    def test_delta_that_does_not_install_fails(self):
        with self.assertRaises(ValueError):
            self.run_execute(f'a=1\nzlib1g={NEW}\n', f'a\t1\nzlib1g\t{OLD}\n', f'a\t1\nzlib1g\t{OLD}\n')

    def test_large_drift_falls_back_to_stock(self):
        count = runtime.MAX_DELTA + 1
        requirements = ''.join(f'p{i}=2\n' for i in range(count))
        image = ''.join(f'p{i}\t1\n' for i in range(count))
        installed = ''.join(f'p{i}\t2\n' for i in range(count))
        calls, report = self.run_execute(requirements, image, installed)
        self.assertEqual(report['variant'], 'stock')
        self.assertIn(('docker', 'pull', runtime.BASE), calls)


if __name__ == '__main__':
    unittest.main()
