#!/usr/bin/env -S uv run --script
"""Shared Qt facade boundaries and legacy dialog/modifier policy (#884)."""
from pathlib import Path
import os
import json
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import platform_ifdef_ratchet as ratchet
from test_settings_platform_facade import function

ROOT = Path(__file__).resolve().parent.parent
PROPERTIES = ROOT / 'shared/properties-view/properties-view.cpp.hpp'
WRAPPERS = ROOT / 'shared/qt/wrappers/qt-wrappers.cpp.hpp'
PLATFORM = ROOT / 'shared/qt/platform'
SIGNATURES = ('void tweak_checkbox(', 'void raise_after_dialog(',
              'QColorDialog::ColorDialogOptions color_dialog_options(',
              'QFontDialog::FontDialogOptions font_dialog_options(',
              'uint32_t translate_modifiers(')


def bodies(host):
    """Exercise original branches before the move and actual facade bodies after."""
    source = PLATFORM / f'qt-host-{host}.cpp.hpp'
    if source.exists():
        text = source.read_text()
        return '\n'.join(function(text, s) for s in SIGNATURES)
    text = PROPERTIES.read_text()
    checkbox = re.search(r'#ifdef __APPLE__\n(.*?)#endif', text, re.S).group(0)
    raise_window = re.search(r'#ifdef __APPLE__\n\s*// TODO: Revisit when QTBUG-42661.*?#endif', text, re.S).group(0)
    color = re.search(r'#ifdef __linux__\n.*?#endif', text, re.S).group(0)
    font = re.search(r'#ifndef _WIN32\n.*?#endif', text, re.S).group(0)
    modifiers = function(WRAPPERS.read_text(), 'uint32_t TranslateQtKeyboardEventModifiers(')
    modifiers = modifiers.replace('TranslateQtKeyboardEventModifiers', 'translate_modifiers')
    return ('void tweak_checkbox(QCheckBox *checkbox) {\n' + checkbox + '\n}\n'
            'void raise_after_dialog(QWidget *widget) {\n' + raise_window + '\n}\n'
            'QColorDialog::ColorDialogOptions color_dialog_options() {\n'
            'QColorDialog::ColorDialogOptions options;\n' + color + '\nreturn options;\n}\n'
            'QFontDialog::FontDialogOptions font_dialog_options() {\n'
            'QFontDialog::FontDialogOptions options;\n' + font + '\nreturn options;\n}\n' + modifiers)


STUBS = r'''
#include <cstdint>
namespace Qt {
enum { WA_LayoutUsesWidgetRect=1, ShiftModifier=1, AltModifier=2, ControlModifier=4, MetaModifier=8 };
struct KeyboardModifiers { int value; bool testFlag(int flag) const { return value & flag; } };
}
struct QWidget { int raises=0; QWidget *window() { return this; } void raise() { ++raises; } };
struct QCheckBox { bool rect=false; void setAttribute(int) { rect=true; } };
struct QColorDialog { enum { DontUseNativeDialog=1, ShowAlphaChannel=2 }; using ColorDialogOptions=int; };
struct QFontDialog { enum { DontUseNativeDialog=1 }; using FontDialogOptions=int; };
'''
# Qt QFlags default-initializes to zero, unlike int.
STUBS = STUBS.replace('using ColorDialogOptions=int;', 'struct ColorDialogOptions { int value=0; ColorDialogOptions()=default; ColorDialogOptions(int f):value(f){} void operator|=(int f) { value|=f; } operator int() const { return value; } };')
STUBS = STUBS.replace('using FontDialogOptions=int;', 'struct FontDialogOptions { int value=0; FontDialogOptions()=default; FontDialogOptions(int f):value(f){} operator int() const { return value; } };')


class SharedQtPlatformTest(unittest.TestCase):
    def test_shared_boundary(self):
        for p in (PROPERTIES, WRAPPERS, ROOT / 'shared/properties-view/properties-view.moc.hpp'):
            with self.subTest(path=p):
                self.assertEqual(ratchet.count_platform_conditionals(p.read_text()), [])

    def test_build_wiring(self):
        header = PLATFORM / 'qt-platform.hpp'
        self.assertTrue(header.exists())
        sources = json.loads((ROOT / 'shared/qt/wrappers/source-sets.json').read_text())
        for host in ('windows', 'macos', 'posix'):
            p = PLATFORM / f'qt-host-{host}.cpp.hpp'
            self.assertTrue(p.exists())
            for signature in SIGNATURES:
                self.assertIn(signature, p.read_text())
            group = ROOT / f'shared/qt/wrappers/build/qt-wrappers.host-{host}+.hpp.cpp'
            self.assertIn(str(p.relative_to(ROOT)), group.read_text())
            self.assertIn('build/' + group.name, sources[host])
        cmake = (ROOT / 'shared/properties-view/CMakeLists.txt').read_text()
        self.assertIn('$<$<CXX_COMPILER_ID:MSVC>:/wd4505>', cmake)

    @unittest.skipUnless(shutil.which('cmake'), 'CMake required')
    def test_cmake_selects_the_shared_host_inventory(self):
        wrappers = ROOT / 'shared/qt/wrappers'
        sources = json.loads((wrappers / 'source-sets.json').read_text())
        for host in ('windows', 'macos', 'posix'):
            with self.subTest(host=host), tempfile.TemporaryDirectory(prefix='obs-wrapper-sources-') as tmp:
                root = Path(tmp)
                (root / 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.28)\nproject(WrapperSources NONE)\n'
                    'function(find_package)\nendfunction()\n'
                    'add_library(core INTERFACE)\nadd_library(Qt::Core ALIAS core)\n'
                    'add_library(widgets INTERFACE)\nadd_library(Qt::Widgets ALIAS widgets)\n'
                    'add_library(obs INTERFACE)\nadd_library(OBS::libobs ALIAS obs)\n'
                    f'set(OS_WINDOWS {"TRUE" if host == "windows" else "FALSE"})\n'
                    f'set(OS_MACOS {"TRUE" if host == "macos" else "FALSE"})\n'
                    f'add_subdirectory("{wrappers.as_posix()}" wrappers)\n'
                    'get_target_property(inputs qt-wrappers INTERFACE_SOURCES)\n'
                    'file(WRITE "${CMAKE_BINARY_DIR}/sources.txt" "${inputs}")\n'
                )
                result = subprocess.run(['cmake', '-S', str(root), '-B', str(root / 'build')],
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                actual = (root / 'build/sources.txt').read_text().split(';')
                expected = [(wrappers / item).as_posix() for item in sources['common'] + sources[host]]
                self.assertEqual(actual, expected)

    def test_legacy_policy_all_hosts(self):
        compiler = shutil.which('c++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        for host, flags, mac, linux in (
            ('windows', ['-U__linux__', '-D_WIN32'], False, False),
            ('macos', ['-U__linux__', '-D__APPLE__'], True, False),
            ('posix', ['-D__linux__'], False, True),
            ('posix', ['-U__linux__'], False, False),
        ):
            with self.subTest(host=host, flags=flags):
                source = STUBS + '\n#include "libobs/obs-interaction.h"\n' + bodies(host)
                source += f'''
int main() {{
 QCheckBox checkbox; tweak_checkbox(&checkbox);
 if (checkbox.rect != {str(mac).lower()}) return 1;
 QWidget window; raise_after_dialog(&window);
 if (window.raises != {int(mac)}) return 2;
 if (int(color_dialog_options()) != {int(linux)}) return 3;
 if (int(font_dialog_options()) != {int(host != 'windows')}) return 4;
 for (int bits=0; bits<32; ++bits) {{
  int expected=((bits&1)?INTERACT_SHIFT_KEY:0)|((bits&2)?INTERACT_ALT_KEY:0)|
    ((bits&4)?{'INTERACT_COMMAND_KEY' if mac else 'INTERACT_CONTROL_KEY'}:0)|
    ((bits&8)?{'INTERACT_CONTROL_KEY' if mac else 'INTERACT_COMMAND_KEY'}:0);
  if (translate_modifiers(Qt::KeyboardModifiers{{bits}}) != unsigned(expected)) return 5;
 }}
}}
'''
                with tempfile.TemporaryDirectory(prefix='obs-shared-qt-') as tmp:
                    p = Path(tmp) / 'policy.cpp'; p.write_text(source)
                    binary = Path(tmp) / 'policy'
                    result = subprocess.run([compiler, '-std=c++17', '-I', str(ROOT), *flags,
                                             str(p), '-o', str(binary)], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    subprocess.run([str(binary)], check=True)

    def test_real_qt_offscreen(self):
        qt_prefix = os.environ.get('OBS_QT_TEST_PREFIX')
        if not qt_prefix and (not shutil.which('pkg-config') or subprocess.run(
                ['pkg-config', '--exists', 'Qt6Widgets'], capture_output=True).returncode):
            self.skipTest('Qt6Widgets development files unavailable; native Qt CI still required')
        flags = [] if qt_prefix else shlex.split(subprocess.check_output(
            ['pkg-config', '--cflags', '--libs', 'Qt6Widgets'], text=True))
        source = '#include <QApplication>\n#include <QCheckBox>\n#include <QColorDialog>\n#include <QFontDialog>\n#include "libobs/obs-interaction.h"\n'
        source += bodies('posix')
        source += r'''
int main(int argc, char **argv) {
 QApplication app(argc, argv);
 QCheckBox checkbox; tweak_checkbox(&checkbox);
 if (checkbox.testAttribute(Qt::WA_LayoutUsesWidgetRect)) return 1;
 QWidget window; raise_after_dialog(&window);
 if (color_dialog_options() != QColorDialog::DontUseNativeDialog) return 2;
 if (font_dialog_options() != QFontDialog::DontUseNativeDialog) return 3;
 for (int bits=0; bits<16; ++bits) {
  Qt::KeyboardModifiers mods;
  if (bits&1) mods |= Qt::ShiftModifier;
  if (bits&2) mods |= Qt::AltModifier;
  if (bits&4) mods |= Qt::ControlModifier;
  if (bits&8) mods |= Qt::MetaModifier;
  int expected=((bits&1)?INTERACT_SHIFT_KEY:0)|((bits&2)?INTERACT_ALT_KEY:0)|
    ((bits&4)?INTERACT_CONTROL_KEY:0)|((bits&8)?INTERACT_COMMAND_KEY:0);
  if (translate_modifiers(mods) != unsigned(expected)) return 4;
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='obs-shared-qt-offscreen-') as tmp:
            p = Path(tmp) / 'offscreen.cpp'; p.write_text(source)
            binary = Path(tmp) / 'offscreen'
            if qt_prefix:
                Path(tmp, 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.28)\nproject(qt_policy LANGUAGES CXX)\n'
                    'find_package(Qt6 REQUIRED COMPONENTS Widgets)\n'
                    'add_executable(offscreen offscreen.cpp)\n'
                    'target_compile_features(offscreen PRIVATE cxx_std_17)\n'
                    f'target_include_directories(offscreen PRIVATE "{ROOT}")\n'
                    'target_link_libraries(offscreen PRIVATE Qt6::Widgets)\n')
                subprocess.run(['cmake', '-S', tmp, '-B', str(Path(tmp) / 'build'),
                                '-DCMAKE_PREFIX_PATH=' + qt_prefix], check=True)
                subprocess.run(['cmake', '--build', str(Path(tmp) / 'build')], check=True)
                binary = Path(tmp) / 'build/offscreen'
                subprocess.run([str(binary)], env={**os.environ, 'QT_QPA_PLATFORM':'offscreen'}, check=True)
                return
            result = subprocess.run(['c++', '-std=c++17', '-fPIC', '-I', str(ROOT), str(p),
                                     '-o', str(binary), *flags], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(binary)], env={**os.environ, 'QT_QPA_PLATFORM':'offscreen'}, check=True)


if __name__ == '__main__':
    unittest.main()
