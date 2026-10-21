#!/usr/bin/env -S uv run --script
"""Frontend CMake platform boundaries and configure-stage behavior (#889)."""

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from platform_paths import platforms  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
FRONTEND = ROOT / "frontend"
HOST_CONDITION = re.compile(r"\b(?:if|elseif)\(([^)]*\bOS_(?:WINDOWS|MACOS|LINUX|FREEBSD|OPENBSD|FLATPAK)\b[^)]*)\)", re.S)
OPERATIONS = ("obs_frontend_platform_sources", "obs_frontend_platform_qt",
              "obs_frontend_platform_plugin_manager", "obs_frontend_platform_plugin_paths",
              "obs_frontend_platform_whatsnew")


class FrontendCMakeFacadeTest(unittest.TestCase):
    def test_shared_features_have_no_host_branches(self):
        for name in ("feature-plugin-manager.cmake", "feature-whatsnew.cmake", "ui-qt.cmake"):
            with self.subTest(file=name):
                text = (FRONTEND / "cmake" / name).read_text()
                self.assertEqual(HOST_CONDITION.findall(text), [])

    def test_dispatch_and_platform_operations(self):
        text = (FRONTEND / "CMakeLists.txt").read_text()
        self.assertEqual(text.count("if(OS_WINDOWS)"), 1)
        self.assertIn("obs_frontend_platform_sources()", text)
        self.assertLess(text.index("include(cmake/os-windows.cmake)"),
                        text.index("include(cmake/feature-plugin-manager.cmake)"))
        self.assertGreater(text.index("obs_frontend_platform_sources()"),
                           text.index("configure_file(cmake/templates/ui-config.h.in ui-config.h)"))
        for host, expected in (("windows", "windows"), ("macos", "macos"),
                               ("linux", "linux"), ("freebsd", "linux")):
            with self.subTest(host=host):
                self.assertEqual(platforms(f"frontend/cmake/os-{host}.cmake"), frozenset({expected}))
                self.assertEqual(text.count(f"include(cmake/os-{host}.cmake)"), 1)
                source = (FRONTEND / f"cmake/os-{host}.cmake").read_text()
                for operation in OPERATIONS:
                    self.assertIn(f"macro({operation})", source)

    def test_configure_feature_selection(self):
        cmake = shutil.which("cmake")
        if not cmake:
            self.skipTest("CMake unavailable; native validation remains required")
        cases = [(host, "6.11.1", True, True, False) for host in
                 ("windows", "macos", "linux", "freebsd", "openbsd", "flatpak")]
        cases += [("macos", version, True, True, False) for version in ("6.10.2", "6.12.0")]
        cases += [(host, "6.11.1", enabled, browser, relocatable)
                  for host in ("linux", "flatpak", "freebsd")
                  for enabled, browser, relocatable in ((False, True, False), (True, False, False),
                                                        (True, True, True))]
        for host, version, enabled, browser, relocatable in cases:
            with self.subTest(host=host, version=version, enabled=enabled, browser=browser, relocatable=relocatable):
                with tempfile.TemporaryDirectory(prefix="obs-cmake-facade-") as tmp:
                    directory = Path(tmp)
                    (directory / "cmake").mkdir()
                    (directory / "cmake/feature-macos-update.cmake").write_text(
                        'file(APPEND "${TRACE}" "MACOS_UPDATE\\n")\n')
                    selected = "linux" if host == "flatpak" else "freebsd" if host == "openbsd" else host
                    platform_file = FRONTEND / f"cmake/os-{selected}.cmake"
                    # Loading the selected host file must only define setup
                    # operations; the trace must remain empty until a stage.
                    setup = f'include("{platform_file.as_posix()}")\n'
                    script = '''cmake_minimum_required(VERSION 3.28)
project(FrontendFacade NONE)
set(TRACE "${CMAKE_BINARY_DIR}/trace.txt")
file(WRITE "${TRACE}" "")
add_library(wrappers INTERFACE)
add_library(OBS::qt-wrappers ALIAS wrappers)
'''
                    if browser:
                        script += 'add_library(browser INTERFACE)\nadd_library(OBS::browser-panels ALIAS browser)\n'
                    for flag in ("WINDOWS", "MACOS", "LINUX", "FREEBSD", "OPENBSD", "FLATPAK"):
                        value = flag.lower() == host or (host == "flatpak" and flag == "LINUX")
                        script += f'set(OS_{flag} {"ON" if value else "OFF"})\n'
                    script += f'''set(Qt6_VERSION {version})
set(ENABLE_WHATSNEW {"ON" if enabled else "OFF"})
set(ENABLE_RELOCATABLE {"ON" if relocatable else "OFF"})
set(CMAKE_INSTALL_PREFIX /prefix)
set(CMAKE_INSTALL_LIBDIR lib)
set(CMAKE_INSTALL_DATAROOTDIR share)
set(OBS_PLUGIN_DESTINATION lib/obs-plugins)
set(OBS_DATA_DESTINATION share/obs)
'''
                    for command in ("find_package", "target_sources", "target_link_libraries", "configure_file",
                                    "target_enable_feature", "target_compile_definitions", "set_target_properties",
                                    "set_property", "add_subdirectory"):
                        script += f'function({command})\n  file(APPEND "${{TRACE}}" "{command}|${{ARGV}}\\n")\nendfunction()\n'
                    script += setup + 'file(READ "${TRACE}" BEFORE_STAGE)\n'
                    for stage, filename in (("PLUGIN", "feature-plugin-manager.cmake"),
                                            ("QT", "ui-qt.cmake"), ("WHATSNEW", "feature-whatsnew.cmake")):
                        script += f'file(APPEND "${{TRACE}}" "STAGE:{stage}\\n")\ninclude("{(FRONTEND / "cmake" / filename).as_posix()}")\n'
                    script += '''file(APPEND "${TRACE}" "PATHS|${OBS_PLATFORM_INSTALL_PATH}|${OBS_PLATFORM_PLUGIN_MODULE_PATH}|${OBS_PLATFORM_PLUGIN_DATA_PATH}\\n")
if(NOT BEFORE_STAGE STREQUAL "")
  message(FATAL_ERROR "Platform include executed setup before its stage: ${BEFORE_STAGE}")
endif()
'''
                    (directory / "CMakeLists.txt").write_text(script)
                    result = subprocess.run([cmake, "-S", str(directory), "-B", str(directory / "build")],
                                            cwd=directory, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    trace = (directory / "build/trace.txt").read_text()
                    stages = {stage: trace.split(f"STAGE:{stage}\n")[1].split("STAGE:")[0]
                              for stage in ("PLUGIN", "QT", "WHATSNEW")}
                    plugin = {"windows": "pluginmoduleloader_windows", "macos": "plugin-manager-mm",
                              "linux": "plugin-manager-linux", "flatpak": "plugin-manager-flatpak",
                              "freebsd": "plugin-manager-bsd", "openbsd": "plugin-manager-bsd"}[host]
                    self.assertIn(plugin, stages["PLUGIN"])
                    self.assertEqual("LoaderPaths_Flatpak.hpp" in stages["PLUGIN"], host == "flatpak")
                    self.assertEqual("LoaderPaths_BSD.hpp" in stages["PLUGIN"], host in ("freebsd", "openbsd"))
                    self.assertEqual("GuiPrivate" in stages["QT"], host == "macos" and version.startswith("6.11."))
                    self.assertEqual("find_package|Qt6;REQUIRED;Gui;DBus" in stages["QT"],
                                     host in ("linux", "flatpak", "freebsd", "openbsd"))
                    self.assertEqual("MACOS_UPDATE" in stages["WHATSNEW"], host == "macos" and enabled and browser)
                    self.assertEqual("MbedTLS" in stages["WHATSNEW"], host in ("linux", "flatpak") and enabled and browser)
                    self.assertEqual("target_enable_feature" in stages["WHATSNEW"], enabled and browser)
                    path = "../|../lib/obs-plugins/plugins|../share/obs/obs-modules/plugins" if host == "linux" and relocatable else "/prefix/|/prefix/lib/obs-plugins/plugins|/prefix/share/obs/obs-modules/plugins"
                    self.assertIn("PATHS|" + path, trace)


if __name__ == "__main__":
    unittest.main()
