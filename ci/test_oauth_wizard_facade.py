#!/usr/bin/env -S uv run --script
"""OAuth/wizard facade boundary and pure policy characterization (#887)."""

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
SHARED = (
    "oauth/RestreamAuth.cpp.hpp", "oauth/TwitchAuth.cpp.hpp",
    "oauth/YoutubeAuth.cpp.hpp", "wizards/AutoConfig.cpp.hpp",
    "wizards/AutoConfigTestPage.cpp.hpp",
)
OPERATIONS = (
    "PlatformSupportsOAuthBrowser", "ConfigureOAuthLinkMessageBox",
    "PlatformUseModernWizardStyle", "PlatformSupportsAppleHardwareEncoder",
    "PlatformNeedsQsvStreamingFallback",
)


class OAuthWizardFacadeTest(unittest.TestCase):
    def test_shared_files_have_no_host_decisions(self):
        for name in SHARED:
            with self.subTest(file=name):
                text = (ROOT / "frontend" / name).read_text()
                self.assertEqual(ratchet.count_platform_conditionals(text), [])

    def test_each_platform_implements_the_operations(self):
        header = (ROOT / "frontend/utility/platform.hpp").read_text()
        for operation in OPERATIONS:
            self.assertIn(operation + "(", header)
            for name in ("platform-x11.cpp.hpp", "platform-windows.cpp.hpp", "platform-osx.mm.hpp"):
                with self.subTest(operation=operation, file=name):
                    text = (ROOT / "frontend/utility" / name).read_text()
                    self.assertRegex(text, rf"\b{operation}\([^;]*?\)\s*\{{")

    def test_pure_wizard_and_qsv_policies(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable; native builds still required")
        # Compile the actual two self-contained production function bodies,
        # not a second implementation of their policy. All four prior host
        # outcomes remain characterized even when this check runs on Linux.
        for name, modern, fallback, flags in (
            ("platform-windows.cpp.hpp", True, False, []),
            ("platform-osx.mm.hpp", True, False, []),
            ("platform-x11.cpp.hpp", False, True, ["-D__linux__"]),
            ("platform-x11.cpp.hpp", False, False, ["-U__linux__"]),
        ):
            with self.subTest(file=name, flags=flags):
                text = (ROOT / "frontend/utility" / name).read_text()
                bodies = []
                for fn in ("PlatformUseModernWizardStyle", "PlatformNeedsQsvStreamingFallback"):
                    match = re.search(rf"bool {fn}\(\)\s*\{{[^}}]*\}}", text)
                    self.assertIsNotNone(match, fn)
                    bodies.append(match.group())
                code = "\n".join(bodies) + (
                    "\nint main() { return !(PlatformUseModernWizardStyle() == "
                    + str(modern).lower() + " && PlatformNeedsQsvStreamingFallback() == "
                    + str(fallback).lower() + "); }\n"
                )
                with tempfile.TemporaryDirectory(prefix="obs-wizard-policy-") as tmp:
                    source = Path(tmp) / "policy.cpp"
                    binary = Path(tmp) / "policy"
                    source.write_text(code)
                    subprocess.run([compiler, "-std=c++17", "-Werror", *flags,
                                    str(source), "-o", str(binary)], check=True, capture_output=True)
                    subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
