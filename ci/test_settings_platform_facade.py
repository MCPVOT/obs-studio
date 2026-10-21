#!/usr/bin/env -S uv run --script
"""Settings platform boundaries, encoder eligibility and CapsLock behavior (#869)."""

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import platform_ifdef_ratchet as ratchet  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OPERATIONS = ("PlatformNeedsNonNativeColorDialog", "PlatformSupportsSimpleQsvEncoders",
              "PlatformSupportsSimpleAppleEncoders", "PlatformHotkeyEditHandlesKeyRelease")


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


class SettingsFacadeTest(unittest.TestCase):
    def test_shared_files_have_no_host_decisions(self):
        for name in ("OBSBasicSettings_A11y.cpp.hpp", "OBSBasicSettings_Stream.cpp.hpp",
                     "OBSHotkeyEdit.cpp.hpp", "OBSHotkeyEdit.hpp"):
            with self.subTest(file=name):
                self.assertEqual(ratchet.count_platform_conditionals(
                    (ROOT / "frontend/settings" / name).read_text()), [])

    def test_each_platform_defines_the_operations(self):
        header = (ROOT / "frontend/utility/platform.hpp").read_text()
        for operation in OPERATIONS:
            self.assertIn(operation + "(", header)
            for host in ("x11.cpp", "windows.cpp", "osx.mm"):
                with self.subTest(host=host, operation=operation):
                    text = (ROOT / f"frontend/utility/platform-{host}.hpp").read_text()
                    self.assertRegex(text, rf"bool {operation}\(\)\s*\{{")

    def test_encoder_color_and_hotkey_behavior(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable; native validation remains required")
        edit = (ROOT / "frontend/settings/OBSHotkeyEdit.cpp.hpp").read_text()
        release = function(edit, "void OBSHotkeyEdit::keyReleaseEvent(")
        for host, qsv, apple, color, handles, flags in (
            ("windows.cpp", True, False, False, False, []),
            ("osx.mm", False, True, False, True, []),
            ("x11.cpp", False, False, True, False, ["-D__linux__"]),
            ("x11.cpp", False, False, False, False, ["-U__linux__"]),
        ):
            with self.subTest(host=host, flags=flags):
                text = (ROOT / f"frontend/utility/platform-{host}.hpp").read_text()
                bodies = [function(text, f"bool {name}(") for name in OPERATIONS]
                bodies.append(function(text, "bool GetPlatformHotkeyOverride("))
                preamble = r'''
using obs_key_t = int;
namespace Qt { constexpr int Key_CapsLock = 1; }
constexpr int OBS_KEY_NONE = 0;
int obs_key_from_virtual_key(int value) { return value + 1000; }
bool appleAvailable = true;
bool PlatformSupportsAppleHardwareEncoder(const char *) { return appleAvailable; }
struct obs_key_combination_t { int key; int modifiers; };
struct QKeyEvent {
    int code; bool repeat; int mods;
    int key() const { return code; }
    bool isAutoRepeat() const { return repeat; }
    int modifiers() const { return mods; }
};
int TranslateQtKeyboardEventModifiers(int value) { return value; }
struct QLineEdit { int baseReleases = 0; void keyReleaseEvent(QKeyEvent *) { ++baseReleases; } };
struct OBSHotkeyEdit : QLineEdit {
    int changes = 0; obs_key_combination_t last{0, 0};
    void HandleNewKey(obs_key_combination_t value) { ++changes; last = value; }
    void keyReleaseEvent(QKeyEvent *);
};
'''
                checks = f'''
int main() {{
    if (PlatformSupportsSimpleQsvEncoders() != {str(qsv).lower()}) return 1;
    if (PlatformSupportsSimpleAppleEncoders() != {str(apple).lower()}) return 2;
    appleAvailable = false;
    if (PlatformSupportsSimpleAppleEncoders()) return 3;
    if (PlatformNeedsNonNativeColorDialog() != {str(color).lower()}) return 4;
    if (PlatformHotkeyEditHandlesKeyRelease() != {str(handles).lower()}) return 5;
    obs_key_t key = 123; bool pressed = false;
    if (GetPlatformHotkeyOverride(2, key, pressed) || key != 123 || pressed) return 6;
    if (GetPlatformHotkeyOverride(Qt::Key_CapsLock, key, pressed) != {str(handles).lower()}) return 7;
    if ({str(handles).lower()} && (key != 1057 || !pressed)) return 8;
    OBSHotkeyEdit edit;
    QKeyEvent cap{{Qt::Key_CapsLock, false, 9}}, other{{2, false, 3}}, repeat{{Qt::Key_CapsLock, true, 4}};
    edit.keyReleaseEvent(&cap);
    edit.keyReleaseEvent(&other);
    edit.keyReleaseEvent(&repeat);
    if (edit.changes != {int(handles)}) return 9;
    if (edit.baseReleases != {0 if handles else 3}) return 10;
    if ({str(handles).lower()} && (edit.last.key != 1057 || edit.last.modifiers != 9)) return 11;
}}
'''
                with tempfile.TemporaryDirectory(prefix="obs-settings-policy-") as tmp:
                    source = Path(tmp) / "policy.cpp"
                    binary = Path(tmp) / "policy"
                    source.write_text(preamble + "\n".join(bodies) + release + checks)
                    subprocess.run([compiler, "-std=c++17", "-Werror", *flags, str(source),
                                    "-o", str(binary)], check=True, capture_output=True)
                    subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
