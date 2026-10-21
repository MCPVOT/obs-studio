"""Check real CTest registrations retain coverage and isolate shared displays."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
TIMEOUTS = {
    "test_async_delay_audio": 180,
    "test_audio_clock_drift": 300,
    "test_deinterlace_2x_field_order": 300,
    "test_inhibit_sleep_dbus": 60,
    "test_media_cover_art_seek": 120,
    "test_pipewire_screencast_resume": 120,
    "test_pipewire_screencast_shm": 90,
    "test_screencast_portal_teardown": 60,
    "test_vlc_audio_pacing": 120,
    "test_xshm_resolution_change": 60,
}


@unittest.skipUnless(all(shutil.which(tool) for tool in ("cmake", "ctest", "cc")), "requires CMake/CTest/cc")
class Tier2ConcurrencyTest(unittest.TestCase):
    def registry(self, directory, display_wrapper):
        source = Path(directory)
        (source / "stub.c").write_text("int fixture;\n")
        cmake = '''cmake_minimum_required(VERSION 3.28)
project(concurrency_registration C)
enable_testing()
set(OS_LINUX TRUE)
set(PKG_CONFIG_FOUND TRUE)
function(find_package)
endfunction()
function(pkg_check_modules)
endfunction()
function(add_executable target)
  _add_executable(${target} "${CMAKE_CURRENT_SOURCE_DIR}/stub.c")
endfunction()
add_library(fixture INTERFACE)
set_target_properties(fixture PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(alias OBS::libobs OBS::caption OBS::media-playback OBS::pipewire OBS::glad
    FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil FFmpeg::swscale FFmpeg::swresample
    gio::gio PkgConfig::INHIBIT_TEST_GLIB PkgConfig::_portal_test_glib
    PkgConfig::_screencast_shm_test_glib PkgConfig::_screencast_resume_test_glib
    Libdrm::Libdrm PipeWire::PipeWire X11::X11 X11::XCB
    XCB::XCB XCB::XFIXES XCB::RANDR XCB::SHM XCB::XINERAMA XCB::COMPOSITE)
  add_library(${alias} ALIAS fixture)
endforeach()
add_library(libobs-opengl SHARED stub.c)
add_library(OBS::libobs-opengl ALIAS libobs-opengl)
add_library(linux-capture MODULE stub.c)
add_library(vlc-video MODULE stub.c)
add_custom_target(cmocka-tests)
set(OBS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/output")
'''
        cmake += f'set(OBS_TEST_XVFB_RUN "{display_wrapper}")\n'
        for name in TIMEOUTS:
            cmake += f'include("{ROOT}/test/cmocka/tests.d/{name}.cmake")\n'
        cmake += 'set_property(TEST test_audio_clock_drift APPEND PROPERTY ENVIRONMENT "EXISTING_SETTING=preserved")\n'
        cmake += f'include("{ROOT}/test/cmocka/parallel-environment.cmake" OPTIONAL)\n'
        (source / "CMakeLists.txt").write_text(cmake)
        build = source / "build"
        configured = subprocess.run(["cmake", "-S", directory, "-B", str(build)], capture_output=True, text=True)
        self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
        return json.loads(subprocess.check_output(["ctest", "--test-dir", str(build), "--show-only=json-v1"]))["tests"]

    def test_isolated_tests_can_overlap_without_losing_timeouts_or_cases(self):
        for available in (True, False):
            with self.subTest(display_wrapper=available), tempfile.TemporaryDirectory() as directory:
                wrapper = str(Path(directory) / "xvfb-run") if available else ""
                if wrapper:
                    Path(wrapper).write_text("#!/bin/sh\nexit 0\n")
                    Path(wrapper).chmod(0o755)
                tests = self.registry(directory, wrapper)
                self.assertEqual({test["name"] for test in tests}, set(TIMEOUTS))
                for test in tests:
                    name = test["name"]
                    properties = {p["name"]: p["value"] for p in test["properties"]}
                    with self.subTest(test=name):
                        exclusive = name == "test_deinterlace_2x_field_order" or (name == "test_xshm_resolution_change" and not wrapper)
                        self.assertEqual(bool(properties.get("RUN_SERIAL")), exclusive,
                                         "Retain timing isolation and unisolated display mutation")
                        self.assertEqual(properties["TIMEOUT"], TIMEOUTS[name])
                        self.assertIn("LP_NUM_THREADS=1", properties.get("ENVIRONMENT", []),
                                      "Concurrent renderers must share the CPU budget")
                        if name == "test_audio_clock_drift":
                            self.assertIn("EXISTING_SETTING=preserved", properties["ENVIRONMENT"])
                        if name == "test_xshm_resolution_change" and wrapper:
                            self.assertEqual(test["command"][:3], [wrapper, "-a", "--server-args=-screen 0 1280x1024x24 -noreset"])


if __name__ == "__main__":
    unittest.main()
