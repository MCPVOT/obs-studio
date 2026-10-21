"""Hybrid packages keep offline inputs pinned and publish only after approval."""

import json
import os
import re
import shutil
import subprocess
import tempfile
import tomllib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class HybridPackagingTest(unittest.TestCase):
    def test_windows_nvenc_reference_has_separate_linker_outputs(self):
        compiler = shutil.which("cc")
        if not compiler or not shutil.which("ninja"):
            self.skipTest("CMake generation fixture needs cc and Ninja")
        source = (ROOT / "plugins/obs-nvenc/CMakeLists.txt").read_text()
        properties = re.search(r"set_target_properties\(\s*obs-nvenc-c-reference\s+PROPERTIES.*?\n\s*\)", source, re.S)[0]
        for generator in ("Ninja", "Ninja Multi-Config"):
            with self.subTest(generator=generator), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                build = root / "build"
                (root / "empty.c").write_text("void nvenc_fixture(void) {}\n")
                (root / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.28)
project(nvenc_reference C)
add_library(obs-nvenc MODULE empty.c)
add_library(obs-nvenc-c-reference MODULE empty.c)
set_target_properties(obs-nvenc PROPERTIES PREFIX "")
''' + properties)
                result = subprocess.run(["cmake", "-S", directory, "-B", str(build), "-G", generator,
                                         "-DCMAKE_SYSTEM_NAME=Windows", "-DCMAKE_C_COMPILER=" + compiler,
                                         "-DCMAKE_C_COMPILER_FORCED=TRUE", "-DCMAKE_BUILD_TYPE=RelWithDebInfo"],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                configs = ("Debug", "Release", "RelWithDebInfo") if generator.endswith("Multi-Config") else (None,)
                for config in configs:
                    rules = (build / "CMakeFiles" / f"impl-{config}.ninja" if config else build / "build.ninja").read_text()
                    imports = re.findall(r"^\s*TARGET_IMPLIB = (.+)$", rules, re.M)
                    self.assertEqual(len(imports), 2)
                    self.assertNotEqual(imports[0], imports[1], "production and reference import/export files collide")
                    self.assertNotIn("obs-nvenc-c-reference/", imports[0])
                    self.assertIn("obs-nvenc-c-reference/", imports[1])

    def test_flatpak_has_the_pinned_compiler_and_offline_cargo_inputs(self):
        manifest = json.loads((ROOT / "build-aux/com.obsproject.Studio.json").read_text())
        version = tomllib.loads((ROOT / "rust-toolchain.toml").read_text())["toolchain"]["channel"]
        compiler = next(module for module in manifest["modules"] if module["name"] == "obs-rust-toolchain")
        self.assertIn(f"rust-{version}-x86_64-unknown-linux-gnu.tar.xz", compiler["sources"][0]["url"])
        self.assertRegex(compiler["sources"][0]["sha256"], r"^[a-f0-9]{64}$")
        self.assertEqual(compiler["build-options"], {"strip": False, "no-debuginfo": True},
                         "Flatpak must preserve the prebuilt compiler's loadable ELF segments")
        obs = next(module for module in manifest["modules"] if module["name"] == "obs")
        self.assertEqual(obs["build-options"]["env"]["CARGO_NET_OFFLINE"], "true")
        self.assertIn("-DOBS_LIBOBS_BACKEND=HYBRID", obs["config-opts"])
        self.assertTrue(any(source.get("dest") == ".flatpak-rust/corrosion" and source.get("sha256") for source in obs["sources"]))
        workflow = (ROOT / ".github/workflows/build-project.yaml").read_text()
        self.assertLess(workflow.index("run: build-aux/prepare-flatpak-rust.sh"), workflow.index("- name: Build Flatpak Manifest"))

    def test_every_publication_path_checks_hybrid_release_approval(self):
        for name, publish in (("release-rust.yaml", "- name: Create or repair release"),
                              ("appimage.yaml", "- name: Attach to the tag's release")):
            source = (ROOT / ".github/workflows" / name).read_text().split("\n  release:", 1)[1]
            with self.subTest(workflow=name):
                self.assertIn("ci/hybrid_release_gate.py", source)
                self.assertLess(source.index("ci/hybrid_release_gate.py"), source.index(publish))

    def test_macos_application_build_does_not_request_the_removed_twin(self):
        script = (ROOT / ".github/scripts/.build.zsh").read_text()
        self.assertNotIn("libobs-with-rust", script)
        self.assertNotIn("-target libobs-c-reference", script)

    def test_cold_vendor_bootstrap_output_cannot_contaminate_cargo_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "build-aux").mkdir()
            (root / "bin").mkdir()
            script = root / "build-aux/prepare-flatpak-rust.sh"
            shutil.copyfile(ROOT / "build-aux/prepare-flatpak-rust.sh", script)
            stub = root / "bin/soldr"
            stub.write_text('#!/bin/sh\ncat <<\'CONFIG\'\nRust is installed now. Great!\n[source.crates-io]\nreplace-with = "vendored-sources"\n\n[source.vendored-sources]\ndirectory = ".flatpak-rust/vendor"\nCONFIG\n')
            stub.chmod(0o755)
            result = subprocess.run(["bash", str(script)], env={**os.environ, "PATH": str(root / "bin") + os.pathsep + os.environ["PATH"]}, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            config = tomllib.loads((root / ".flatpak-rust/cargo/config.toml").read_text())
            self.assertEqual(config["source"]["vendored-sources"]["directory"], "/run/build/obs/.flatpak-rust/vendor")
