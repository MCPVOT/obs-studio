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

    def test_whatsnew_url_is_chosen_by_the_host_file(self):
        # #883: the shared source has no host branch; macOS and Windows pick
        # their URL macro in their obs_frontend_platform_whatsnew().
        source = (FRONTEND / "utility/WhatsNewInfoThread.cpp.hpp").read_text()
        self.assertIsNone(re.search(r"#\s*(?:if|ifdef|elif)\b.*\b(?:__APPLE__|_WIN32)\b", source))
        self.assertIn("#ifndef WHATSNEW_URL\n#define WHATSNEW_URL LINUX_WHATSNEW_URL", source)
        for host, url in (("macos", "MAC_WHATSNEW_URL"), ("windows", "WIN_WHATSNEW_URL")):
            with self.subTest(host=host):
                text = (FRONTEND / f"cmake/os-{host}.cmake").read_text()
                body = text[text.index("macro(obs_frontend_platform_whatsnew)"):]
                body = body[:body.index("endmacro()")]
                self.assertIn(f"WHATSNEW_URL={url}", body)

    def test_output_network_options_go_through_the_facade(self):
        # #883: the Windows-only socket loop and low-latency options are set by
        # SetOutputNetworkOptions() in the host files, not under _WIN32.
        header = (FRONTEND / "utility/platform.hpp").read_text()
        self.assertIn("void SetOutputNetworkOptions(config_t *config, obs_data_t *settings);", header)
        for name in ("SimpleOutput.cpp.hpp", "AdvancedOutput.cpp.hpp"):
            with self.subTest(file=name):
                source = (FRONTEND / "utility" / name).read_text()
                self.assertNotIn("NewSocketLoopEnable", source)
                self.assertIn("SetOutputNetworkOptions(main->Config(), settings);", source)
        for host in ("windows.cpp.hpp", "osx.mm.hpp", "x11.cpp.hpp"):
            with self.subTest(host=host):
                source = (FRONTEND / f"utility/platform-{host}").read_text()
                self.assertIn("void SetOutputNetworkOptions(config_t *", source)
        windows = (FRONTEND / "utility/platform-windows.cpp.hpp").read_text()
        self.assertIn('"new_socket_loop_enabled"', windows)
        self.assertIn('"low_latency_mode_enabled"', windows)

    def test_legacy_encoder_ids_are_translated_by_the_host(self):
        # #883: only macOS renames its old VideoToolbox encoder ids, in
        # TranslateLegacyEncoderId(); AdvancedOutput has no __APPLE__ branch.
        header = (FRONTEND / "utility/platform.hpp").read_text()
        self.assertIn("void TranslateLegacyEncoderId(const char *&encoder);", header)
        source = (FRONTEND / "utility/AdvancedOutput.cpp.hpp").read_text()
        self.assertNotIn("translate_macvth264_encoder", source)
        self.assertIn("TranslateLegacyEncoderId(streamEncoder);", source)
        self.assertIn("TranslateLegacyEncoderId(recordEncoder);", source)
        for host in ("windows.cpp.hpp", "osx.mm.hpp", "x11.cpp.hpp"):
            with self.subTest(host=host):
                self.assertIn("void TranslateLegacyEncoderId(const char *&",
                              (FRONTEND / f"utility/platform-{host}").read_text())
        mac = (FRONTEND / "utility/platform-osx.mm.hpp").read_text()
        self.assertIn('"com.apple.videotoolbox.videoencoder.h264.gva"', mac)

    def test_macos_dialog_and_style_quirks_go_through_the_facade(self):
        # #883: the QTBUG-42661 window raise and the native combo box popup
        # are host operations, not __APPLE__ branches in shared files.
        header = (FRONTEND / "utility/platform.hpp").read_text()
        self.assertIn("void RaiseAfterFileDialog(QWidget *window);", header)
        self.assertIn("bool UseNativeComboBoxPopup();", header)
        for name in ("MissingFilesPathItemDelegate.cpp.hpp", "RemuxEntryPathItemDelegate.cpp.hpp"):
            with self.subTest(file=name):
                source = (FRONTEND / "utility" / name).read_text()
                self.assertNotIn("__APPLE__", source)
                self.assertIn("RaiseAfterFileDialog(container->window());", source)
        style = (FRONTEND / "utility/OBSProxyStyle.cpp.hpp").read_text()
        self.assertNotIn("__APPLE__", style)
        self.assertIn("UseNativeComboBoxPopup()", style)
        for host in ("windows.cpp.hpp", "osx.mm.hpp", "x11.cpp.hpp"):
            with self.subTest(host=host):
                source = (FRONTEND / f"utility/platform-{host}").read_text()
                self.assertIn("void RaiseAfterFileDialog(QWidget *", source)
                self.assertIn("bool UseNativeComboBoxPopup()", source)

    def test_dropped_files_go_through_the_facade(self):
        # #873: the JXR image type, the text source and Windows .url and
        # shortcut handling are host operations, not _WIN32 branches.
        header = (FRONTEND / "utility/platform.hpp").read_text()
        self.assertIn("bool IsPlatformImageExtension(const char *suffix);", header)
        self.assertIn("const char *GetPlatformTextSourceId();", header)
        self.assertIn("void SetPlatformTextFileSettings(obs_data_t *settings, const char *file, QString &name);", header)
        self.assertIn("bool ResolvePlatformDroppedFile(QString &file, QFileInfo &info, QString &urlTarget);", header)
        source = (FRONTEND / "widgets/OBSBasic_Dropfiles.cpp.hpp").read_text()
        self.assertNotIn("_WIN32", source)
        self.assertNotIn("text_gdiplus", source)
        for host in ("windows.cpp.hpp", "osx.mm.hpp", "x11.cpp.hpp"):
            with self.subTest(host=host):
                source = (FRONTEND / f"utility/platform-{host}").read_text()
                self.assertIn("bool IsPlatformImageExtension(const char *", source)
                self.assertIn("const char *GetPlatformTextSourceId()", source)
                self.assertIn("void SetPlatformTextFileSettings(obs_data_t *", source)
                self.assertIn("bool ResolvePlatformDroppedFile(QString &", source)

    def test_small_main_window_quirks_go_through_the_facade(self):
        # #873: the color dialog, the meta key name, projector screen names
        # and the desktop audio device are host choices.
        header = (FRONTEND / "utility/platform.hpp").read_text()
        self.assertIn("const char *GetPlatformMetaKeyText();", header)
        self.assertIn("QString GetPlatformScreenName(QScreen *screen);", header)
        self.assertIn("bool PlatformUsesDesktopAudioDevice();", header)
        for name in ("OBSBasic_Preview", "OBSBasic_Hotkeys", "OBSBasic_Projectors", "OBSBasic_SceneItems"):
            with self.subTest(file=name):
                source = (FRONTEND / f"widgets/{name}.cpp.hpp").read_text()
                for macro in ("_WIN32", "__APPLE__", "__linux__"):
                    self.assertNotIn(macro, source)
        preview = (FRONTEND / "widgets/OBSBasic_Preview.cpp.hpp").read_text()
        self.assertIn("PlatformNeedsNonNativeColorDialog()", preview)
        for host in ("windows.cpp.hpp", "osx.mm.hpp", "x11.cpp.hpp"):
            with self.subTest(host=host):
                source = (FRONTEND / f"utility/platform-{host}").read_text()
                self.assertIn("const char *GetPlatformMetaKeyText()", source)
                self.assertIn("QString GetPlatformScreenName(QScreen *", source)
                self.assertIn("bool PlatformUsesDesktopAudioDevice()", source)

    def test_tray_icon_goes_through_the_facade(self):
        # #873: the tray icon images, the tray click and the dock icon are
        # host choices; the tray code names no host.
        header = (FRONTEND / "utility/platform.hpp").read_text()
        self.assertIn("QIcon GetPlatformTrayIcon(TrayIconState state);", header)
        self.assertIn("bool PlatformTrayActivationTogglesWindow();", header)
        self.assertIn("void SetPlatformDockIconVisible(bool visible);", header)
        for name in ("OBSBasic_SysTray", "OBSBasic_Recording"):
            with self.subTest(file=name):
                source = (FRONTEND / f"widgets/{name}.cpp.hpp").read_text()
                self.assertNotIn("__APPLE__", source)
                self.assertNotIn("EnableOSXDockIcon", source)
        for host in ("windows.cpp.hpp", "osx.mm.hpp", "x11.cpp.hpp"):
            with self.subTest(host=host):
                source = (FRONTEND / f"utility/platform-{host}").read_text()
                self.assertIn("QIcon GetPlatformTrayIcon(TrayIconState", source)
                self.assertIn("bool PlatformTrayActivationTogglesWindow()", source)
                self.assertIn("void SetPlatformDockIconVisible(bool", source)

    def test_nvenc_check_goes_through_the_facade(self):
        # #873: whether a host has NVENC and its test program's name are
        # host facts; the missing-encoder check names no host.
        header = (FRONTEND / "utility/platform.hpp").read_text()
        self.assertIn("const char *GetPlatformNvencTestProgram();", header)
        source = (FRONTEND / "widgets/OBSBasic_Profiles.cpp.hpp").read_text()
        for macro in ("_WIN32", "__APPLE__"):
            self.assertNotIn(macro, source)
        self.assertIn("GetPlatformNvencTestProgram()", source)
        for host, program in (("windows.cpp.hpp", '"obs-nvenc-test.exe"'), ("osx.mm.hpp", "nullptr"),
                              ("x11.cpp.hpp", '"obs-nvenc-test"')):
            with self.subTest(host=host):
                source = (FRONTEND / f"utility/platform-{host}").read_text()
                body = source.split("const char *GetPlatformNvencTestProgram()", 1)[1].split("}", 1)[0]
                self.assertIn(program, body)

    def test_main_controls_go_through_the_facade(self):
        # #873: the macOS permissions dialog, the dock icon and the dialog
        # close before an always-on-top toggle are host operations. Only the
        # Windows updater branch stays, for the updater slice.
        header = (FRONTEND / "utility/platform.hpp").read_text()
        self.assertIn("void ShowPlatformPermissionsDialog(QWidget *parent);", header)
        self.assertIn("bool PlatformRecreatesWindowsOnTopToggle();", header)
        source = (FRONTEND / "widgets/OBSBasic_MainControls.cpp.hpp").read_text()
        self.assertNotIn("__APPLE__", source)
        self.assertNotIn("#ifndef _WIN32", source)
        self.assertNotIn("EnableOSXDockIcon", source)
        self.assertIn("ShowPlatformPermissionsDialog(this);", source)
        pch = (FRONTEND / "build/frontend-widgets.pch.hpp").read_text()
        self.assertNotIn("OBSPermissions", pch)
        for host in ("windows.cpp.hpp", "osx.mm.hpp", "x11.cpp.hpp"):
            with self.subTest(host=host):
                source = (FRONTEND / f"utility/platform-{host}").read_text()
                self.assertIn("void ShowPlatformPermissionsDialog(QWidget *", source)
                self.assertIn("bool PlatformRecreatesWindowsOnTopToggle()", source)

    def test_hdr_screenshots_go_through_the_facade(self):
        # #883: the HDR color space choice and the JXR writer are host
        # operations; ScreenshotObj and its precompiled header name no host.
        header = (FRONTEND / "utility/platform.hpp").read_text()
        self.assertIn("enum gs_color_space ScreenshotColorSpace(obs_source_t *source);", header)
        self.assertIn("bool SaveHdrScreenshot(const char *path, uint8_t *pixels, uint32_t cx, uint32_t cy);", header)
        for path in ("utility/ScreenshotObj.cpp.hpp", "build/frontend-utility-screenshotobj.pch.hpp"):
            with self.subTest(file=path):
                source = (FRONTEND / path).read_text()
                self.assertNotIn("_WIN32", source)
                self.assertNotIn("wincodec", source)
        shot = (FRONTEND / "utility/ScreenshotObj.cpp.hpp").read_text()
        self.assertIn("ScreenshotColorSpace(source)", shot)
        self.assertIn("SaveHdrScreenshot(path.c_str(), half_bytes.data(), outputWidth, outputHeight)", shot)
        for host in ("windows.cpp.hpp", "osx.mm.hpp", "x11.cpp.hpp"):
            with self.subTest(host=host):
                source = (FRONTEND / f"utility/platform-{host}").read_text()
                self.assertIn("enum gs_color_space ScreenshotColorSpace(obs_source_t *", source)
                self.assertIn("bool SaveHdrScreenshot(const char *", source)
        windows = (FRONTEND / "utility/platform-windows.cpp.hpp").read_text()
        self.assertIn("GUID_ContainerFormatWmp", windows)
        self.assertIn("<wincodec.h>", (FRONTEND / "build/frontend-windows-platform.pch.hpp").read_text())

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
                    script = script.replace(
                        'project(FrontendFacade NONE)\n',
                        'project(FrontendFacade NONE)\n'
                        f'include("{(FRONTEND / "cmake/sources.cmake").as_posix()}")\n',
                    )
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
