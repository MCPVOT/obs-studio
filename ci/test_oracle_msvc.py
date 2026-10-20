#!/usr/bin/env -S uv run --script
"""Oracle MSVC configuration accepts C11 flexible arrays under /W4 /WX."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent


class OracleMsvcTest(unittest.TestCase):
    def test_flexible_arrays_keep_strict_warnings(self):
        build = (ROOT / "rust/obs-c-oracle/build.rs").read_text()
        block = re.search(r"if oracle.get_compiler\(\).is_like_msvc\(\) \{(.*?)\n    \}", build, re.S)
        self.assertIsNotNone(block)
        flags = re.findall(r'oracle\.flag\("([^"]+)"\)', block.group(1))
        self.assertIn("/wd4200", flags, "C11 flexible arrays must compile under MSVC /W4 /WX (#1427)")
        self.assertNotIn("/w", flags)
        self.assertNotIn("/WX-", flags)
        self.assertIn(".extra_warnings(true)", build)
        self.assertIn(".warnings_into_errors(true)", build)


if __name__ == "__main__":
    unittest.main()
