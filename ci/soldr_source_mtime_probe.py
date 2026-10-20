#!/usr/bin/env -S uv run --script
"""Manual #1359 probe: regenerate obs-util full/delta test archives and replay.

Use a new directory under build_ci_audit and an explicit released soldr binary.
Preserves original tracked-file timestamps and keeps all archives and logs.
The compiler wrapper is disabled; this does not test compiler-cache correctness.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent
SOURCE = "rust/obs-util/src/platform/windows/host.rs"


def probe(directory: Path, soldr: str, jobs: int) -> dict:
    directory = directory.resolve()
    if not directory.is_relative_to(ROOT / "build_ci_audit"):
        raise ValueError("Keep probe state under build_ci_audit")
    directory.mkdir(parents=True, exist_ok=False)
    target = directory / "target"
    env = dict(os.environ, SOLDR_RUSTC_WRAPPER="none", CARGO_TARGET_DIR=str(target),
               CARGO_BUILD_JOBS=str(jobs), CARGO_TERM_COLOR="never",
               CARGO_LOG="cargo::core::compiler::fingerprint=info")
    result = {"soldr": subprocess.check_output([soldr, "--version"], text=True).strip(),
              "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
              "compiler_wrapper": "none", "source": SOURCE, "observations": []}

    def persist():
        (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n")

    def run(label, args):
        start = time.monotonic()
        with (directory / f"{label}.log").open("w") as log:
            child = subprocess.run(args, cwd=ROOT, env=env, stdout=log,
                                   stderr=subprocess.STDOUT, timeout=1200, check=False)
        output = (directory / f"{label}.log").read_text(errors="replace")
        row = {"label": label, "command": args, "returncode": child.returncode,
               "seconds": round(time.monotonic() - start, 3),
               "fresh_obs_util": "Fresh obs-util " in output,
               "precalculated_components_changed": "PrecalculatedComponentsChanged" in output}
        result["observations"].append(row)
        persist()
        if child.returncode:
            raise RuntimeError(f"Inspect {directory / (label + '.log')}")
        return row

    # Hydration replays every tracked source's timestamp. Restore each only if
    # its bytes still match, so an intervening content edit is never hidden.
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).split(b"\0")
    stamps = {}
    for rel in filter(None, tracked):
        path = ROOT / os.fsdecode(rel)
        if path.is_file() and not path.is_symlink():
            stamp = path.stat()
            stamps[path] = (hashlib.sha256(path.read_bytes()).digest(), stamp.st_atime_ns, stamp.st_mtime_ns)
    source = ROOT / SOURCE
    try:
        requested = 1_792_262_706_251_336_821
        os.utime(source, ns=(source.stat().st_atime_ns, requested))
        original = source.stat().st_mtime_ns
        result["source_mtime_ns"] = original
        result["source_sha256"] = hashlib.sha256(source.read_bytes()).hexdigest()
        if original % 1_000_000 == 0:
            raise RuntimeError("Filesystem cannot represent submillisecond timestamps")
        tests = [soldr, "cargo", "test", "-p", "obs-util", "--locked", "--no-run", "-vv"]
        run("seed", tests)
        run("seed-warm", tests)
        base = directory / "base.tar.zst"
        delta = directory / "delta.tar.zst"
        manifest = directory / "base.pb"
        run("save-base", [soldr, "save", "--cache-dir", str(target), "--workspace", str(ROOT),
                          "--out", str(base), "--zstd-level", "1", "--json"])

        def load(label, archive, extra=()):
            run(label, [soldr, "load", "--archive", str(archive), "--cache-dir", str(target),
                        "--workspace", str(ROOT), "--json", *extra])

        target.rename(directory / "seed-target")
        os.utime(source, ns=(source.stat().st_atime_ns, time.time_ns()))
        load("restore-full", base, ["--manifest-out", str(manifest)])
        full_stamp = source.stat().st_mtime_ns
        full = run("full-consumer", tests)
        # A real changed payload proves the delta, rather than only its base,
        # was applied. Source bytes and compiler inputs remain unchanged.
        (target / "mtime-probe-marker").write_text("delta applied\n")
        # Save the delta with the same precise source time as the producer.
        os.utime(source, ns=(source.stat().st_atime_ns, original))
        run("save-delta", [soldr, "save", "--cache-dir", str(target), "--workspace", str(ROOT),
                           "--delta-from-manifest", str(manifest), "--out", str(delta),
                           "--zstd-level", "1", "--json"])
        target.rename(directory / "full-consumer-target")
        os.utime(source, ns=(source.stat().st_atime_ns, time.time_ns()))
        load("restore-base", base)
        load("restore-delta", delta)
        delta_stamp = source.stat().st_mtime_ns
        combined = run("delta-consumer", tests)
        result.update(full_source_mtime_ns=full_stamp, delta_source_mtime_ns=delta_stamp,
                      full_exact_mtime=full_stamp == original, delta_exact_mtime=delta_stamp == original,
                      full_fresh=full["fresh_obs_util"], delta_fresh=combined["fresh_obs_util"],
                      no_precalculated_changes=not any(x["precalculated_components_changed"]
                                                      for x in (full, combined)),
                      delta_payload_applied=(target / "mtime-probe-marker").read_text() == "delta applied\n",
                      source_unchanged=hashlib.sha256(source.read_bytes()).hexdigest() == result["source_sha256"],
                      archive_bytes={"base": base.stat().st_size, "delta": delta.stat().st_size})
    finally:
        for path, (digest, atime, mtime) in stamps.items():
            if path.is_file() and hashlib.sha256(path.read_bytes()).digest() == digest:
                os.utime(path, ns=(atime, mtime))
        persist()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--soldr", required=True)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    result = probe(args.directory, str(Path(args.soldr).resolve()), args.jobs)
    print(json.dumps(result, indent=2))
    return 0 if all(result[k] for k in ("full_exact_mtime", "delta_exact_mtime", "full_fresh",
                                       "delta_fresh", "no_precalculated_changes", "delta_payload_applied",
                                       "source_unchanged")) else 1


if __name__ == "__main__":
    raise SystemExit(main())
