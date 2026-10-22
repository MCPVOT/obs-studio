#!/usr/bin/env -S uv run --script
"""Keep raw shader resources identical when Git uses Windows line endings."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
EFFECT = Path("libobs/data/format_conversion.effect")


class EffectCheckoutTest(unittest.TestCase):
    def test_conversion_techniques_survive_a_windows_style_checkout(self):
        with tempfile.TemporaryDirectory(prefix="obs-effect-checkout-") as directory:
            destination = Path(directory)
            subprocess.run(
                ["git", "-c", "core.autocrlf=true", "-c", "core.eol=crlf",
                 "checkout-index", "--prefix=" + destination.as_posix() + "/", EFFECT.as_posix()],
                cwd=ROOT, check=True, capture_output=True,
            )
            shader = (destination / EFFECT).read_bytes()
            self.assertNotIn(b"\r", shader)
            self.assertIn(b"\ntechnique Planar_Y\n", shader)


if __name__ == "__main__":
    unittest.main()
