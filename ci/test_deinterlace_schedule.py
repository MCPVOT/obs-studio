"""Check the generated CTest scheduling contract for the field-order test."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import re
import unittest


ROOT = Path(__file__).resolve().parent.parent
RATES = ("60i_into_60", "50i_into_50", "50i_into_60", "50i_into_30", "60i_into_50")


@unittest.skipUnless(shutil.which("cmake") and shutil.which("ctest") and shutil.which("cc"),
                     "requires CMake, CTest and a C compiler")
class DeinterlaceScheduling(unittest.TestCase):
    def test_field_order_retains_timing_isolation_and_renderer_budget(self):
        # Configure the real registration with stand-in link targets. No OBS
        # compilation or change to the C test's assertions is needed.
        with tempfile.TemporaryDirectory() as scratch:
            source = Path(scratch)
            (source / "placeholder.c").write_text("int placeholder;\n")
            (source / "test_deinterlace_2x_field_order.c").write_text("int main(void) { return 0; }\n")
            registration = ROOT / "test/cmocka/tests.d/test_deinterlace_2x_field_order.cmake"
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.28)\n'
                'project(schedule_probe C)\nenable_testing()\n'
                'set(OS_LINUX TRUE)\n'
                'add_library(core STATIC placeholder.c)\n'
                'add_library(OBS::libobs ALIAS core)\n'
                'add_library(libobs-opengl SHARED placeholder.c)\n'
                'add_library(OBS::libobs-opengl ALIAS libobs-opengl)\n'
                'add_custom_target(cmocka-tests)\n'
                'set(OBS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/output")\n'
                f'include("{registration.as_posix()}")\n'
                f'include("{ROOT}/test/cmocka/parallel-environment.cmake" OPTIONAL)\n'
            )
            build = source / "build"
            configured = subprocess.run(["cmake", "-S", str(source), "-B", str(build)],
                                        capture_output=True, text=True)
            self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
            result = subprocess.run(["ctest", "--test-dir", str(build), "--show-only=json-v1"],
                                    capture_output=True, text=True, check=True)
            tests = json.loads(result.stdout)["tests"]
            # One entry per rate, so CI can spread them over runners (#2072).
            self.assertEqual(sorted(test["name"] for test in tests),
                             sorted(f"test_deinterlace_2x_field_order_{rate}" for rate in RATES))
            for test in tests:
                rate = test["name"].removeprefix("test_deinterlace_2x_field_order_")
                properties = {p["name"]: p["value"] for p in test["properties"]}
                with self.subTest(test=test["name"]):
                    self.assertTrue(properties.get("RUN_SERIAL"),
                                    "CI still drops fields under concurrent CPU load")
                    self.assertIn("LP_NUM_THREADS=1", properties.get("ENVIRONMENT", []))
                    self.assertIn(f"CMOCKA_TEST_FILTER=*_{rate}", properties.get("ENVIRONMENT", []))
                    self.assertEqual(properties["TIMEOUT"], 120.0)


class RateCoverage(unittest.TestCase):
    def test_every_case_group_runs_under_one_registered_rate(self):
        # A case whose rate no entry filters for would silently never run.
        source = (ROOT / "test/cmocka/test_deinterlace_2x_field_order.c").read_text()
        main = source[source.index("int main(void)"):]
        used = re.findall(r"^\s*[A-Z_]+_CASES\((\w+),", main, re.M)
        self.assertGreater(len(used), 20)
        self.assertEqual(set(used), set(RATES))
        # Every case name ends in its rate (#rate_name or ##rate_name).
        for macro in re.findall(r"#define [A-Z_]+CASES?\(rate_name|#define FIELD_CASE\(name", source):
            self.assertTrue(macro)
        names = re.findall(r'\{"(\w+)_" #rate_name|FIELD_CASE\((\w+)_##rate_name', source)
        self.assertTrue(names)


if __name__ == "__main__":
    unittest.main()
