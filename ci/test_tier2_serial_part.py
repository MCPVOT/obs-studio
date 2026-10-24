"""Tier 2 runs its RUN_SERIAL tests on their own runner (#2071).

Usage:
    uv run python -m unittest ci/test_tier2_serial_part.py

test_deinterlace_2x_field_order is RUN_SERIAL (#1343, #1643): concurrent CI
dropped fields. Inside the one ctest run per mode it ran alone for 146 of the
238 s while the runner's other cores sat idle. Each mode now runs two parts
on separate runners: "suite" (every test but the RUN_SERIAL ones, in
parallel) and "serial" (only those, so they keep a runner to themselves).
The tests and their assertions are unchanged.
"""

import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "rust/tools/linux-validate"
WORKFLOW = ROOT / ".github/workflows/build-project.yaml"


def test_job() -> str:
    text = WORKFLOW.read_text(encoding="utf-8")
    return re.search(r"^  rust-linux-tier2:\n(.*?)(?=^  [a-z0-9-]+:\n)", text, re.S | re.M)[1]


def registry(*tests: tuple[str, bool]) -> dict:
    return {"tests": [{"name": name, "properties": [{"name": "RUN_SERIAL", "value": True}] if serial else []}
                      for name, serial in tests]}


class SerialTestsTest(unittest.TestCase):
    def select(self, data: dict) -> str:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "registry.json"
            path.write_text(json.dumps(data))
            return subprocess.run(["uv", "run", "--no-project", str(TOOLS / "serial-tests.py"), str(path)],
                                  check=True, capture_output=True, text=True).stdout.strip()

    def test_regex_matches_exactly_the_run_serial_tests(self):
        regex = self.select(registry(("test_a", False), ("test_deinterlace_2x_field_order", True),
                                     ("test_xshm_resolution_change", True), ("test_deinterlace_2x", False)))
        self.assertRegex("test_deinterlace_2x_field_order", regex)
        self.assertRegex("test_xshm_resolution_change", regex)
        self.assertNotRegex("test_a", regex)
        self.assertNotRegex("test_deinterlace_2x", regex)

    def test_no_serial_tests_is_an_error(self):
        # An empty selection would make the serial part pass with no tests.
        with self.assertRaises(subprocess.CalledProcessError):
            self.select(registry(("test_a", False)))


class ValidateTest(unittest.TestCase):
    def test_parts_select_with_exclude_and_include(self):
        text = (TOOLS / "validate.sh").read_text(encoding="utf-8")
        self.assertIn("OBS_CTEST_PART", text)
        self.assertIn("serial-tests.py", text)
        self.assertRegex(text, r"suite\)[^;]*-E ")
        self.assertRegex(text, r"serial\)[^;]*-R ")


class RuntimeTest(unittest.TestCase):
    def test_ci_runtime_takes_the_part(self):
        spec = importlib.util.spec_from_file_location("ci_runtime", TOOLS / "ci-runtime.py")
        runtime = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runtime)
        text = (TOOLS / "ci-runtime.py").read_text(encoding="utf-8")
        self.assertIn("'--part', choices=('suite', 'serial')", text)
        self.assertIn("OBS_CTEST_PART={args.part}", text)


class WorkflowTest(unittest.TestCase):
    def test_each_mode_runs_both_parts_on_separate_runners(self):
        job = test_job()
        self.assertRegex(job, r'mode: \["OFF", "ON"\]')
        self.assertRegex(job, r'part: \["suite", "serial"\]')
        self.assertIn("--part '${{ matrix.part }}'", job)
        # Two parts of one mode write timing artifacts; the names must differ.
        self.assertIn("name: tier2-cost-${{ matrix.mode }}-${{ matrix.part }}", job)
        self.assertIn("name: tier2-runtime-${{ matrix.mode }}-${{ matrix.part }}", job)


if __name__ == "__main__":
    unittest.main()
