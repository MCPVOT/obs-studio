#!/usr/bin/env -S uv run --script
"""Manual target-cache correctness experiment for #1286; no CI policy changes.

Keeps a new scratch workspace, full archive, command logs and result.json.
Returns 1 if restored unit tests accept modified source; --verify-guard instead
requires fingerprint invalidation to reject it. No existing cache is removed.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent
MARKER = "DEEP_CACHE_CHANGED"
PACKAGE = "obs-deep-cache-probe"


def probe(directory: Path, soldr: str) -> dict:
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
        f'[package]\nname="{PACKAGE}"\nversion="0.1.0"\nedition="2021"\n'
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
                   compiled_package=f"Compiling {PACKAGE}" in output,
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
    tests = [soldr, "cargo", "test", "--lib", "-p", PACKAGE]
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
    fingerprints = list((target / "debug/.fingerprint").glob(f"{PACKAGE}-*"))
    if not fingerprints:
        raise RuntimeError("No package fingerprints found; guard was not exercised")
    for fingerprint in fingerprints:
        fingerprint.rename(quarantine / fingerprint.name)
    guarded = run("guarded-same-stamp", tests + ["--locked"])
    result = dict(soldr=version, wrapper="none", archive_bytes=archive.stat().st_size,
                  fixture="nested compile_error edit preserves size and nanosecond mtime",
                  observations=observations,
                  unchanged_reused=not warm["compiled_package"],
                  ordinary_edit_rejected=normal["returncode"] != 0 and normal["current_diagnostic"],
                  stale_unit_test_success=stale["returncode"] == 0 and not stale["compiled_package"],
                  guarded_edit_rejected=guarded["returncode"] != 0 and guarded["current_diagnostic"]
                  and guarded["compiled_package"],
                  guard_kept_dependency=not guarded["compiled_dependency"],
                  guard_scope="Known changed package only; automatic content-to-package mapping is not implemented")
    (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--soldr", required=True)
    parser.add_argument("--verify-guard", action="store_true")
    args = parser.parse_args()
    result = probe(args.directory, args.soldr)
    print(json.dumps(result, indent=2))
    if args.verify_guard:
        return 0 if all(result[k] for k in ("unchanged_reused", "ordinary_edit_rejected",
                                           "guarded_edit_rejected", "guard_kept_dependency")) else 1
    return 1 if result["stale_unit_test_success"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
