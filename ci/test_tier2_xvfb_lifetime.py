"""The shared Tier 2 display must survive gaps between test clients (#1355)."""
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'rust/tools/linux-validate/validate.sh'


class Tier2DisplayLifetimeTest(unittest.TestCase):
    def test_real_suite_invocation_keeps_server_and_preserves_failure(self):
        source = SCRIPT.read_text()
        declaration = re.search(r'^\s*xvfb_args=\([^\n]+\)', source, re.M)
        self.assertIsNotNone(declaration, 'Declare the shared server lifetime policy')
        invocation = source.split('    ctest_status=0\n', 1)[1].split('    # Keep the timings', 1)[0]
        with tempfile.TemporaryDirectory() as directory:
            argv = Path(directory) / 'argv'
            prefix = '''
set -euo pipefail
mode_env=()
mode_shard_args=()
ctest_status=0
xvfb-run() { printf '%s\\n' "$@" > "$argv_file"; return 17; }
'''
            run = subprocess.run(
                ['bash', '-c', declaration.group(0) + '\n' + prefix + invocation
                 + '\nprintf "%s" "$ctest_status"',
                 '_'],
                env={**os.environ, 'build': directory + '/tree', 'mode': 'OFF',
                     'argv_file': str(argv)}, capture_output=True, text=True, check=True)
            self.assertEqual(run.stdout, '17', 'The test failure must reach the gate')
            args = argv.read_text().splitlines()
            self.assertIn('--server-args=-screen 0 1280x1024x24 -noreset', args)
            self.assertIn('--output-on-failure', args)
        calls = re.findall(r'xvfb-run ([^\n]+)ctest', source)
        self.assertEqual(len(calls), 2)
        self.assertTrue(all('"${xvfb_args[@]}"' in call for call in calls),
                        'Normal and repeated tests need the same display lifetime')


if __name__ == '__main__':
    unittest.main()
