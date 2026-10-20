#!/usr/bin/env -S uv run --script
"""Model setup-soldr's post-job cook snapshots against actual Rust consumers.

Archives are saved AFTER workspace clippy and selected obs-util tests, like
setup-soldr's post action. Compare unchanged inputs and an obs-util rebuild.
All experiment targets remain below --directory; existing target/ is kept.
Network transfer and formatter/Dylint time are not included in this model.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
POLICIES = {"release": ["--release"], "dev": ["--profile", "dev"], "none": None}


def benchmark(
    directory: Path,
    soldr: str,
    trials: int,
    jobs: int,
    existing: bool,
    scenarios: list[str],
) -> dict:
    directory = directory.resolve()
    if directory.exists() and not existing:
        raise ValueError(
            f"Use a new directory or explicitly reuse completed seed trials: {directory}"
        )
    directory.mkdir(parents=True, exist_ok=existing)
    destination = directory / "post-job-result.json"
    resumed = json.loads(destination.read_text()) if destination.exists() else None
    version = subprocess.run(
        [soldr, "--version"], capture_output=True, text=True, check=True
    ).stdout.strip()
    expected = re.search(r"set\(OBS_SOLDR_VERSION ([0-9.]+)\)",
                         (ROOT / "cmake/common/soldr.cmake").read_text())[1]
    if version != f"soldr {expected}":
        raise ValueError(f"Expected the CI pin soldr {expected}, got {version}")

    def source_state() -> str:
        diff = subprocess.run(
            [
                "git",
                "diff",
                "--no-ext-diff",
                "--binary",
                "--",
                "Cargo.toml",
                "Cargo.lock",
                "rust",
            ],
            cwd=ROOT,
            capture_output=True,
            check=True,
        ).stdout
        return hashlib.sha256(diff).hexdigest()

    original = source_state()
    source = ROOT / "rust/obs-util/src/lib.rs"
    source_bytes = source.read_bytes()
    result = {
        "soldr": version,
        "jobs": jobs,
        "archive_stage": "after-clippy-and-tests",
        "network_transfer_included": False,
        "formatter_dylint_included": False,
        "rust_tree": subprocess.run(
            ["git", "rev-parse", "HEAD:rust"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip(),
        "cargo_lock_sha256": hashlib.sha256(
            (ROOT / "Cargo.lock").read_bytes()
        ).hexdigest(),
        "policies": {},
    }

    if resumed is not None:
        for key in ("soldr", "jobs", "rust_tree", "cargo_lock_sha256"):
            if resumed[key] != result[key]:
                raise ValueError(f"Resume input changed: {key}")
        result = resumed

    def persist() -> None:
        destination.write_text(json.dumps(result, indent=2) + "\n")

    def command(args: list[str], env: dict, log: Path) -> float:
        start = time.monotonic()
        with log.open("w") as stream:
            subprocess.run(
                args,
                cwd=ROOT,
                env=env,
                stdout=stream,
                stderr=subprocess.STDOUT,
                check=True,
            )
        return round(time.monotonic() - start, 3)

    def consume(env: dict, logs: Path, force: bool) -> dict:
        if source_state() != original:
            raise RuntimeError(
                "Cook must restore original workspace contents before consuming artifacts"
            )
        logs.mkdir(parents=True)
        stamp = source.stat()
        if force:
            os.utime(source, ns=(stamp.st_atime_ns, time.time_ns()))
        gate = dict(env, SOLDR_RUSTC_WRAPPER="none")
        try:
            clippy = command(
                [
                    soldr,
                    "cargo",
                    "clippy",
                    "--workspace",
                    "--all-targets",
                    "--locked",
                    "--",
                    "-D",
                    "warnings",
                ],
                gate,
                logs / "clippy.log",
            )
            tests = command(
                [
                    soldr,
                    "cargo",
                    "test",
                    "-p",
                    "obs-util",
                    "--locked",
                    "--",
                    "--test-threads=4",
                ],
                gate,
                logs / "tests.log",
            )
            if force and not any(
                label in (logs / "clippy.log").read_text()
                for label in ("Checking obs-util", "Compiling obs-util")
            ):
                raise RuntimeError("The core-rebuild scenario did not recheck obs-util")
            return {"clippy_seconds": clippy, "test_seconds": tests}
        finally:
            if source.read_bytes() == source_bytes:
                os.utime(source, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))

    old = json.loads((directory / "result.json").read_text()) if existing else None
    for policy, flags in POLICIES.items():
        if policy in result["policies"]:
            continue
        case = directory / policy
        case.mkdir(exist_ok=existing)
        target = case / "target"
        if target.exists():
            raise ValueError(f"Inspect the unfinished target before starting: {target}")
        env = dict(os.environ, CARGO_TARGET_DIR=str(target), CARGO_BUILD_JOBS=str(jobs))
        env.pop("SOLDR_RUSTC_WRAPPER", None)
        record = {"trials": []}
        result["policies"][policy] = record
        if flags is not None:
            if existing:
                if (
                    old["soldr"] != version
                    or old["jobs"] != jobs
                    or old["policies"][policy]["trials"][0].get("test_returncode") != 0
                ):
                    raise ValueError(
                        "Existing seed must use the same pin/jobs and have passing real tests"
                    )
                (case / "trial-1-output").rename(target)
            else:
                record["cold_cook_seconds"] = command(
                    [soldr, "cook", *flags], env, case / "seed-cook.log"
                )
                record["seed_consumer"] = consume(env, case / "seed-logs", False)
            record["save_seconds"] = command(
                [
                    soldr,
                    "save",
                    "--cache-dir",
                    str(target),
                    "--workspace",
                    str(ROOT),
                    "--out",
                    str(case / "post-job.tar.zst"),
                    "--zstd-level",
                    "9",
                ],
                env,
                case / "post-save.log",
            )
            record["archive_bytes"] = (case / "post-job.tar.zst").stat().st_size
            target.rename(case / ("trial-1-output" if existing else "seed-output"))
        persist()

    for scenario in scenarios:
        for trial in range(1, trials + 1):
            order = list(POLICIES)
            order = order[trial % 3 :] + order[: trial % 3]
            for policy in order:
                if any(
                    t["scenario"] == scenario and t["trial"] == trial
                    for t in result["policies"][policy]["trials"]
                ):
                    continue
                case = directory / policy
                target = case / "target"
                env = dict(
                    os.environ, CARGO_TARGET_DIR=str(target), CARGO_BUILD_JOBS=str(jobs)
                )
                env.pop("SOLDR_RUSTC_WRAPPER", None)
                label = f"post-{scenario}-{trial}"
                logs = case / label
                logs.mkdir()
                restore = cook = 0.0
                flags = POLICIES[policy]
                if flags is not None:
                    restore = command(
                        [
                            soldr,
                            "load",
                            "--archive",
                            str(case / "post-job.tar.zst"),
                            "--cache-dir",
                            str(target),
                            "--workspace",
                            str(ROOT),
                        ],
                        env,
                        logs / "restore.log",
                    )
                    cook = command([soldr, "cook", *flags], env, logs / "warm-cook.log")
                times = consume(env, logs / "consume", scenario == "core-rebuild")
                result["policies"][policy]["trials"].append(
                    {
                        "scenario": scenario,
                        "trial": trial,
                        **times,
                        "restore_seconds": restore,
                        "warm_cook_seconds": cook,
                        "load_average": os.getloadavg(),
                        "total_seconds": round(restore + cook + sum(times.values()), 3),
                    }
                )
                target.rename(case / f"{label}-output")
                persist()
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--soldr", required=True, help="Absolute pinned executable")
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument(
        "--seed-existing-trials",
        action="store_true",
        help="Reuse completed seed consumers at their original target path",
    )
    parser.add_argument(
        "--scenarios",
        nargs="+",
        choices=("unchanged", "core-rebuild"),
        default=["unchanged", "core-rebuild"],
    )
    args = parser.parse_args()
    if args.trials < 1 or args.jobs < 1:
        parser.error("trials and jobs must be positive")
    print(
        json.dumps(
            benchmark(
                args.directory,
                args.soldr,
                args.trials,
                args.jobs,
                args.seed_existing_trials,
                args.scenarios,
            ),
            indent=2,
        )
    )
