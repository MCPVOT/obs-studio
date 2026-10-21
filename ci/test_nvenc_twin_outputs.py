#!/usr/bin/env -S uv run --script
"""Evaluate real NVENC twin properties with Windows CMake generators (#1626)."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent.parent


def twin_properties():
    source=(ROOT/'plugins/obs-nvenc/CMakeLists.txt').read_text()
    return re.search(r'set_target_properties\(\s*obs-nvenc-with-rust\s+PROPERTIES.*?\n\s*\)',source,re.S)[0]


class NvencTwinOutputsTest(unittest.TestCase):
    def test_artifact_destinations(self):
        compiler=shutil.which('cc')
        if not compiler or not shutil.which('ninja'):
            self.skipTest('CMake generation fixture needs cc and Ninja')
        for system,generator in [('Windows','Ninja'),('Windows','Ninja Multi-Config'),('Linux','Ninja'),('Darwin','Ninja')]:
            with self.subTest(system=system,generator=generator),tempfile.TemporaryDirectory(prefix='obs-nvenc-outputs-') as tmp:
                build=Path(tmp)/'build'
                Path(tmp,'empty.c').write_text('void obs_nvenc_output_fixture(void) {}\n')
                Path(tmp,'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.28)
project(nvenc_artifacts C)
add_library(obs-nvenc MODULE empty.c)
add_library(obs-nvenc-with-rust MODULE empty.c)
set_target_properties(obs-nvenc PROPERTIES PREFIX "")
'''+twin_properties()+'''
foreach(target IN ITEMS obs-nvenc obs-nvenc-with-rust)
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/${target}-$<CONFIG>.txt" CONTENT "$<TARGET_FILE:${target}>\n")
endforeach()
''')
                result=subprocess.run(['cmake','-S',tmp,'-B',str(build),'-G',generator,
                                       '-DCMAKE_SYSTEM_NAME='+system,'-DCMAKE_C_COMPILER='+compiler,
                                       '-DCMAKE_C_COMPILER_FORCED=TRUE','-DCMAKE_BUILD_TYPE=RelWithDebInfo'],capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)
                configs=('Debug','Release','RelWithDebInfo') if generator.endswith('Multi-Config') else ('RelWithDebInfo',)
                for config in configs:
                    production=Path((build/f'obs-nvenc-{config}.txt').read_text().strip())
                    twin=Path((build/f'obs-nvenc-with-rust-{config}.txt').read_text().strip())
                    suffix=Path(config) if generator.endswith('Multi-Config') else Path()
                    self.assertEqual(production.parent,build/suffix,'production plugin path must not change')
                    self.assertEqual(twin.parent,build/'obs-nvenc-with-rust'/suffix,'existing twin DLL/module path stays isolated')
                    self.assertEqual(production.name,twin.name,'parity module filename must stay the same')
                    if system=='Windows':
                        ninja=(build/f'CMakeFiles/impl-{config}.ninja' if generator.endswith('Multi-Config') else build/'build.ninja').read_text()
                        imports=re.findall(r'^\s*TARGET_IMPLIB = (.+)$',ninja,re.M)
                        self.assertEqual(len(imports),2,ninja)
                        self.assertNotEqual(imports[0],imports[1], 'both linkers write the same import/export artifact prefix')
                        self.assertNotIn('obs-nvenc-with-rust',imports[0],'production import library must stay in place')
                        self.assertIn('obs-nvenc-with-rust/',imports[1])


if __name__=='__main__':
    unittest.main()
