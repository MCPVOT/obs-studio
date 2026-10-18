#!/usr/bin/env -S uv run --script
"""Manual target-cache correctness experiment for #1286; no CI policy changes.

Keeps a new scratch workspace, full archive, command logs and result.json.
Returns 1 if restored unit tests accept modified source; --verify-guard instead
requires fingerprint invalidation to reject it. No existing cache is removed.
Optional native snapshot-fingerprint checks measure coverage and invalidation;
they do not enable the native helper in CI.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent
MARKER = "DEEP_CACHE_CHANGED"
PACKAGE = "obs-deep-cache-probe"


def probe(directory: Path, soldr: str, *, package: str = PACKAGE,
          native_fingerprint_tool: str | None = None) -> dict:
    directory = directory.resolve()
    if not directory.is_relative_to(ROOT / "build_ci_audit"):
        raise ValueError("Keep experiment state under this repo's build_ci_audit/")
    if directory.exists():
        raise ValueError(f"Use a new directory to preserve evidence: {directory}")
    version = subprocess.run([soldr, "--version"], capture_output=True,
                             text=True, check=True).stdout.strip()
    if version != "soldr 0.9.35":
        raise ValueError(f"Expected the CI pin soldr 0.9.35; got {version}")
    (directory / "src").mkdir(parents=True)
    (directory / "dependency/src").mkdir(parents=True)
    (directory / "Cargo.toml").write_text(
        f'[package]\nname="{package}"\nversion="0.1.0"\nedition="2021"\n'
        '[dependencies]\nprobe-dependency={path="dependency"}\n'
        '[workspace]\nmembers=["dependency"]\n[profile.test]\ndebug=false\n')
    (directory / "dependency/Cargo.toml").write_text(
        '[package]\nname="probe-dependency"\nversion="0.1.0"\nedition="2021"\n')
    (directory / "dependency/src/lib.rs").write_text('pub fn answer() -> u32 { 42 }\n')
    (directory / "rust-toolchain.toml").write_bytes((ROOT / "rust-toolchain.toml").read_bytes())
    (directory / "src/lib.rs").write_text(
        'mod nested;\n#[test] fn ready() { assert_eq!(nested::answer(), 42); }\n')
    source = directory / "src/nested.rs"
    good = (f'// compile_error!("{MARKER}");\n'
            'pub fn answer() -> u32 { probe_dependency::answer() }\n').encode()
    source.write_bytes(good)
    env = dict(os.environ, SOLDR_RUSTC_WRAPPER="none", CARGO_BUILD_JOBS="2")
    observations = []

    def run(label: str, args: list[str]) -> dict:
        start = time.monotonic()
        child = subprocess.run(args, cwd=directory, env=env, capture_output=True,
                               text=True, timeout=120)
        output = child.stdout + child.stderr
        (directory / f"{label}.log").write_text(output)
        row = dict(label=label, returncode=child.returncode,
                   seconds=round(time.monotonic() - start, 3),
                   current_diagnostic=f"error: {MARKER}" in output,
                   compiled_package=f"Compiling {package}" in output,
                   compiled_dependency="Compiling probe-dependency" in output)
        observations.append(row)
        return row

    def require_success(label: str, args: list[str]) -> dict:
        row = run(label, args)
        if row["returncode"]:
            raise RuntimeError(f"Inspect {directory / (label + '.log')}")
        return row

    subprocess.run(["git", "init", "-q"], cwd=directory, check=True)
    subprocess.run(["git", "add", "Cargo.toml", "rust-toolchain.toml", "src", "dependency"],
                   cwd=directory, check=True)
    tests = [soldr, "cargo", "test", "--lib", "-p", package]
    require_success("seed", tests)
    stamp = source.stat()
    archive = directory / "base.tar.zst"
    target = directory / "target"
    require_success("save", [soldr, "save", "--cache-dir", str(target),
                             "--workspace", str(directory), "--out", str(archive), "--json"])

    def hydrate(label: str) -> None:
        target.rename(directory / f"{label}-prior-target")
        require_success(label, [soldr, "load", "--archive", str(archive),
                               "--cache-dir", str(target), "--workspace", str(directory), "--json"])

    hydrate("hydrate-unchanged")
    warm = require_success("warm", tests + ["--locked"])
    changed = b"   " + good[3:]
    source.write_bytes(changed)
    hydrate("hydrate-normal-edit")
    normal = run("normal-edit", tests + ["--locked"])
    source.write_bytes(changed)
    os.utime(source, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
    assert source.stat().st_size == stamp.st_size
    assert source.stat().st_mtime_ns == stamp.st_mtime_ns
    hydrate("hydrate-same-stamp")
    stale = run("same-stamp", tests + ["--locked"])

    # A changed-content guard can invalidate this package without destroying
    # dependency artifacts. Quarantine metadata so no files need deletion.
    quarantine = directory / "invalidated-fingerprints"
    quarantine.mkdir()
    fingerprints = list((target / "debug/.fingerprint").glob(f"{package}-*"))
    if not fingerprints:
        raise RuntimeError("No package fingerprints found; guard was not exercised")
    for fingerprint in fingerprints:
        fingerprint.rename(quarantine / fingerprint.name)
    guarded = run("guarded-same-stamp", tests + ["--locked"])
    native = None
    if native_fingerprint_tool:
        tool = str(Path(native_fingerprint_tool).resolve())
        native_version = subprocess.run([tool, "--version"], capture_output=True,
                                        text=True, check=True, timeout=30).stdout.strip()
        source.write_bytes(good)
        os.utime(source, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        hydrate("hydrate-native-unchanged")
        native_warm = require_success("native-unchanged", tests + ["--locked"])
        manifest_path = directory / "native-fp-manifest.json"
        require_success("native-record", [tool, "snapshot-fp-record", "--target-dir", str(target),
                                          "--workspace-root", str(directory),
                                          "--manifest-path", str(manifest_path)])
        manifest = json.loads(manifest_path.read_text())
        covered_sources = sorted({name for crate in manifest["crates"] for name in crate["sources"]})
        source.write_bytes(changed)
        os.utime(source, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        hydrate("hydrate-native-same-stamp")
        assert source.read_bytes() == changed
        assert source.stat().st_size == stamp.st_size
        assert source.stat().st_mtime_ns == stamp.st_mtime_ns
        validation = run("native-validate", [tool, "snapshot-fp-validate", "--target-dir", str(target),
                                              "--workspace-root", str(directory),
                                              "--manifest-path", str(manifest_path)])
        native_tests = run("native-same-stamp", tests + ["--locked"])
        required_sources = {"src/lib.rs", "src/nested.rs", "dependency/src/lib.rs"}
        summary = re.search(r"snapshot-fp-validate: (\d+) clean / (\d+) dirty",
                            (directory / "native-validate.log").read_text())
        native = dict(tool_version=native_version, recorded_crates=len(manifest["crates"]),
                      covered_sources=covered_sources,
                      fixture_coverage_complete=required_sources.issubset(covered_sources),
                      clean_crates=int(summary[1]) if summary else None,
                      dirty_crates=int(summary[2]) if summary else None,
                      unchanged_reused=not native_warm["compiled_package"],
                      validation_exit=validation["returncode"],
                      changed_test_exit=native_tests["returncode"],
                      changed_source_rejected=native_tests["returncode"] != 0
                      and native_tests["current_diagnostic"] and native_tests["compiled_package"],
                      dependency_preserved=not native_tests["compiled_dependency"])

    result = dict(soldr=version, package=package, wrapper="none", archive_bytes=archive.stat().st_size,
                  fixture="nested compile_error edit preserves size and nanosecond mtime",
                  observations=observations,
                  unchanged_reused=not warm["compiled_package"],
                  ordinary_edit_rejected=normal["returncode"] != 0 and normal["current_diagnostic"],
                  stale_unit_test_success=stale["returncode"] == 0 and not stale["compiled_package"],
                  guarded_edit_rejected=guarded["returncode"] != 0 and guarded["current_diagnostic"]
                  and guarded["compiled_package"],
                  guard_kept_dependency=not guarded["compiled_dependency"],
                  guard_scope="Known changed package only; automatic content-to-package mapping is not implemented")
    if native is not None:
        result["native_fingerprint"] = native
    (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--soldr", required=True)
    parser.add_argument("--package-name", choices=(PACKAGE, PACKAGE.replace("-", "_")), default=PACKAGE)
    parser.add_argument("--native-fingerprint-tool", help="Standalone zccache executable for an optional native-helper experiment")
    verification = parser.add_mutually_exclusive_group()
    verification.add_argument("--verify-guard", action="store_true")
    verification.add_argument("--verify-native-guard", action="store_true")
    args = parser.parse_args()
    if args.verify_native_guard and not args.native_fingerprint_tool:
        parser.error("--verify-native-guard requires --native-fingerprint-tool")
    result = probe(args.directory, args.soldr, package=args.package_name,
                   native_fingerprint_tool=args.native_fingerprint_tool)
    print(json.dumps(result, indent=2))
    if args.verify_guard:
        return 0 if all(result[k] for k in ("unchanged_reused", "ordinary_edit_rejected",
                                           "guarded_edit_rejected", "guard_kept_dependency")) else 1
    if args.verify_native_guard:
        native = result["native_fingerprint"]
        return 0 if native["validation_exit"] == 0 and all(native[k] for k in (
            "fixture_coverage_complete", "unchanged_reused", "changed_source_rejected", "dependency_preserved")) else 1
    return 1 if result["stale_unit_test_success"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
