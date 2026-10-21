"""Check the generated CTest scheduling contract for the field-order test."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent


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
            self.assertEqual([test["name"] for test in tests], ["test_deinterlace_2x_field_order"])
            properties = {p["name"]: p["value"] for p in tests[0]["properties"]}
            self.assertTrue(properties.get("RUN_SERIAL"),
                            "CI still drops fields under concurrent CPU load")
            self.assertIn("LP_NUM_THREADS=1", properties.get("ENVIRONMENT", []))
            self.assertEqual(properties["TIMEOUT"], 300.0)


if __name__ == "__main__":
    unittest.main()
