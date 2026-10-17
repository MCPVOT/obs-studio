#!/usr/bin/env -S uv run --script
"""Manual safety probe for soldr's Rust cache; exits 1 on stale output.

    uv run ci/cache_source_probe.py --directory build_ci_audit/source-probe \
        --soldr /path/to/pinned/soldr --expected-version 0.9.35

Keeps the scratch crate, command logs and result.json for inspection. This
is an upstream-blocker experiment, not a required CI job. See #1286 and
https://github.com/zackees/zccache/issues/1943.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MARKER = "CACHE_SOURCE_PROBE"


def probe(directory: Path, executable: str, expected_version: str | None) -> dict:
    version = subprocess.run([executable, "--version"], capture_output=True, text=True, check=True).stdout.strip()
    if expected_version and version != f"soldr {expected_version}":
        raise ValueError(f"Expected soldr {expected_version}, got {version}")
    directory = directory.resolve()
    if directory.exists():
        raise ValueError(f"Use a new directory to preserve previous evidence: {directory}")
    (directory / "src").mkdir(parents=True)
    (directory / "Cargo.toml").write_text(
        '[package]\nname="obs-cache-source-probe"\nversion="0.1.0"\nedition="2021"\n[workspace]\n'
    )
    (directory / "Cargo.lock").write_text(
        'version = 3\n\n[[package]]\nname = "obs-cache-source-probe"\nversion = "0.1.0"\n'
    )
    (directory / "rust-toolchain.toml").write_bytes((ROOT / "rust-toolchain.toml").read_bytes())
    root = directory / "src/lib.rs"
    root.write_text("pub mod nested;\n")
    source = directory / "src/nested.rs"
    source.write_text(f'// compile_error!("{MARKER}");\npub fn ready() {{}}\n')
    env = dict(os.environ)
    env.pop("SOLDR_RUSTC_WRAPPER", None)
    command = [executable, "cargo", "check", "--locked"]

    def run(name: str, *, bypass: bool = False) -> dict:
        child_env = dict(env)
        if bypass:
            child_env["SOLDR_RUSTC_WRAPPER"] = "none"
        start = time.monotonic()
        result = subprocess.run(command, cwd=directory, env=child_env, capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        (directory / f"{name}.log").write_text(output)
        return {
            "returncode": result.returncode,
            "seconds": round(time.monotonic() - start, 3),
            "probe_diagnostic": f"error: {MARKER}" in output,
            "cache_hit_reported": "[HIT" in output,
            "log": f"{name}.log",
        }

    warm = run("warm")
    if warm["returncode"]:
        raise RuntimeError(f"Warm build failed; inspect {directory / 'warm.log'}")
    stamp = source.stat()
    good = source.read_bytes()
    source.write_bytes(b"   " + good[3:])
    os.utime(source, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
    assert source.stat().st_size == stamp.st_size
    assert source.stat().st_mtime_ns == stamp.st_mtime_ns

    def force_cargo() -> None:
        # Cargo must invoke rustc even though the nested source's stamp is
        # unchanged. The root contents remain identical, so a correct cache
        # must detect the nested source's content change.
        now = time.time_ns() + 2_000_000_000
        os.utime(root, ns=(now, now))

    force_cargo()
    cached = run("cached-changed-source")
    force_cargo()
    control = run("uncached-control", bypass=True)
    result = {
        "soldr": version,
        "command": command,
        "preserved_source_size_and_mtime": True,
        "warm": warm,
        "cached": cached,
        "control": control,
        "safe": cached["returncode"] != 0 and cached["probe_diagnostic"]
        and control["returncode"] != 0 and control["probe_diagnostic"],
    }
    (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--soldr", default="soldr", help="Explicit binary avoids a different soldr on uv's PATH")
    parser.add_argument("--expected-version", help="Refuse to measure a different soldr release")
    args = parser.parse_args()
    outcome = probe(args.directory, args.soldr, args.expected_version)
    print(json.dumps(outcome, indent=2))
    raise SystemExit(0 if outcome["safe"] else 1)
