#!/usr/bin/env -S uv run --script
"""Opt-in, same-runner Windows full rebuild measurements for #1269."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def tree_bytes(root):
    return sum(path.stat().st_size for path in root.rglob("*") if path.is_file())


def run_probe(seed, output, target, configuration, *, execute=subprocess.run):
    """Keep each starting cache identical; only its configured size differs."""
    seed, output = Path(seed).resolve(), Path(output).resolve()
    if not seed.is_dir() or output.exists():
        raise ValueError("probe needs an existing seed and a new output directory")
    if seed in output.parents or output in seed.parents:
        raise ValueError("seed and probe must be separate directory trees")
    output.mkdir(parents=True)
    report = {
        "schema": 1, "status": "running", "observations": [],
        "source_sha": os.environ.get("GITHUB_SHA"),
        "runner_image": os.environ.get("ImageVersion"), "target": target,
        "configuration": configuration, "seed_bytes": tree_bytes(seed),
        "started_at": datetime.now(timezone.utc).isoformat(),
        "limits": [
            "Two clean full-build trials per size, in ABBA order, on one runner; not a representative day.",
            "Filesystem bytes are not GitHub archive bytes or repository-wide retention proof.",
            "Copy/cleanup time is separate from rebuild time; original publication cache is unchanged.",
            "Timing includes Rust/build-system work; ccache statistics describe C/C++ only.",
        ],
    }
    try:
        # Four independent copies remain on the ephemeral runner. Do not risk
        # exhausting disk midway through a full build or deleting the real seed.
        if shutil.disk_usage(output).free < 4 * report["seed_bytes"] + 1024 ** 3:
            raise RuntimeError("not enough free disk for four isolated cache copies plus 1 GiB")
        command = ["cmake", "--build", "--preset", f"windows-{target}",
                   "--config", configuration, "--clean-first", "--parallel",
                   "--", "/consoleLoggerParameters:Summary", "/noLogo"]
        for index, limit in enumerate(("2G", "350M", "350M", "2G")):
            cache = output / f"cache-{index}"
            prepare_start = time.monotonic()
            shutil.copytree(seed, cache)
            env = {**os.environ, "CCACHE_DIR": str(cache), "CCACHE_MAXSIZE": limit}

            def ccache(*args):
                return execute(["ccache", *args], env=env, check=True,
                               capture_output=True, text=True).stdout

            version = ccache("--version")
            if Path(ccache("--get-config", "cache_dir").strip()).resolve() != cache:
                raise RuntimeError("ccache is not using the isolated cache directory")
            ccache(f"--max-size={limit}")
            ccache("--cleanup")
            before_bytes = tree_bytes(cache)
            ccache("--zero-stats")
            preparation_seconds = time.monotonic() - prepare_start
            started = time.monotonic()
            execute(command, env=env, check=True)
            seconds = time.monotonic() - started
            counters = json.loads(ccache("--print-stats", "--format=json"))
            calls = sum(counters.get(name, 0) for name in
                        ("direct_cache_hit", "preprocessed_cache_hit", "cache_miss"))
            if calls <= 0:
                raise RuntimeError("clean rebuild produced no cacheable C/C++ calls")
            report["observations"].append({
                "trial": index + 1, "limit": limit, "build_seconds": seconds,
                "preparation_seconds": preparation_seconds, "command": command,
                "before_bytes": before_bytes, "after_bytes": tree_bytes(cache),
                "ccache_version": version,
                "ccache_counters": counters,
                "ccache_statistics": ccache("--show-stats", "--verbose"),
            })
        report["status"] = "success"
    except Exception as error:
        report["status"] = "failure"
        report["error"] = str(error)
        raise
    finally:
        report["completed_at"] = datetime.now(timezone.utc).isoformat()
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("x64", "arm64"), required=True)
    parser.add_argument("--config", default="RelWithDebInfo")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run_probe(Path(os.environ["CCACHE_DIR"]), args.output, args.target, args.config)


if __name__ == "__main__":
    main()
