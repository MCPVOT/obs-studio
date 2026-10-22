"""Verify the NVENC source reader preserves the original platform/loader inputs."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NvencSources(unittest.TestCase):
    @unittest.skipUnless(shutil.which("cmake"), "CMake is required for source parity")
    def test_cmake_reads_exact_original_sources(self):
        expected = {
            "common": ["build/obs-nvenc.caps-cache+.h.c", "build/obs-nvenc.core+.h.c", "build/obs-nvenc.override+.h.c"],
            "linux": ["build/obs-nvenc.caps-cache-linux+.h.c", "build/obs-nvenc.linux+.h.c"],
            "windows": ["build/obs-nvenc.caps-cache-windows+.h.c", "build/obs-nvenc.windows+.h.c"],
            "reference": ["build/obs-nvenc.cuda-load+.h.c"],
            "rust": ["build/obs-nvenc.cuda-load-rust+.h.c"],
            "helper": ["obs-nvenc-test/build/obs-nvenc-test.core+.hpp.cpp"],
            "headers": ["cuda-helpers.h", "nvenc-caps-cache.h", "nvenc-helpers.h", "nvenc-internal.h", "nvenc-override.h", "obs-nvenc.h"],
        }
        manifest = ROOT / "plugins/obs-nvenc/build/source-sets.json"
        self.assertEqual(json.loads(manifest.read_text()), expected)
        with tempfile.TemporaryDirectory() as temporary:
            script = Path(temporary) / "read.cmake"
            script.write_text(
                f'include("{ROOT.as_posix()}/plugins/obs-nvenc/cmake/sources.cmake")\n'
                + '\n'.join(
                    f'obs_nvenc_sources(inputs {key})\nfile(WRITE "{Path(temporary).as_posix()}/{key}" "${{inputs}}")'
                    for key in expected
                )
            )
            subprocess.run(["cmake", "-P", str(script)], check=True, capture_output=True, text=True)
            for key, sources in expected.items():
                actual = [Path(source).resolve() for source in (Path(temporary) / key).read_text().split(";")]
                self.assertEqual(actual, [ROOT / "plugins/obs-nvenc" / source for source in sources])
                self.assertTrue(all(source.is_file() for source in actual))


if __name__ == "__main__":
    unittest.main()
