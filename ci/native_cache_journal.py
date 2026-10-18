#!/usr/bin/env -S uv run --script
"""Start a job-owned native cache journal and summarize it without raw arguments."""

import argparse
import json
import os
import subprocess
import uuid
from collections import Counter
from pathlib import Path

OUTCOMES = {"hit", "miss", "error", "non-cacheable", "non_cacheable"}
REASONS = {"context_not_found", "uncacheable_input", "no_artifact_for_key",
           "input_fingerprint_mismatch", "unknown"}
DIFF_FIELDS = {"args", "env", "compiler", "source", "headers", "includes",
               "force_includes", "externs", "metadata", "changed_inputs"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"}
VALUE_OPTIONS = {"-o", "-I", "-isystem", "-iquote", "-idirafter", "-include",
                 "-include-pch", "-imacros", "-x", "-MF", "-MT", "-MQ", "-D", "-U", "--sysroot"}
MAX_BYTES = 32 * 1024 * 1024


def sources(args, workspace):
    result = []
    skip = False
    for arg in args if isinstance(args, list) else []:
        if skip:
            skip = False
            continue
        if not isinstance(arg, str):
            continue
        if arg in VALUE_OPTIONS:
            skip = True
            continue
        if not arg or arg.startswith("-") or "=" in arg or len(arg) > 4096:
            continue
        path = Path(arg)
        if path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        try:
            relative = (workspace / path).resolve().relative_to(workspace)
        except (ValueError, OSError):
            continue
        result.append(relative.as_posix())
        if len(result) == 10:
            break
    return sorted(set(result))


def summarize(text, workspace, max_records=10000):
    workspace = workspace.resolve()
    records = []
    malformed = 0
    truncated = False
    for line in text.splitlines():
        if not line.strip():
            continue
        if len(records) >= max_records:
            truncated = True
            break
        try:
            raw = json.loads(line)
        except json.JSONDecodeError:
            malformed += 1
            continue
        if not isinstance(raw, dict):
            malformed += 1
            continue
        outcome = raw.get("outcome")
        outcome = outcome if isinstance(outcome, str) and outcome in OUTCOMES else "unclassified"
        diff = raw.get("miss_diff")
        row = {"outcome": outcome, "source_candidates": sources(raw.get("args"), workspace),
               "miss_diff_fields": sorted(DIFF_FIELDS.intersection(diff)) if isinstance(diff, dict) else []}
        if outcome == "miss":
            reason = raw.get("miss_reason")
            row["miss_reason"] = reason if isinstance(reason, str) and reason in REASONS else "unclassified"
        latency = raw.get("latency_ns")
        if type(latency) is int and latency >= 0:
            row["latency_ns"] = latency
        records.append(row)
    return {"schema": 1, "status": "partial" if malformed or truncated else "ok" if records else "no_compile_records",
            "counts": dict(Counter(row["outcome"] for row in records)), "records": records,
            "malformed_lines": malformed, "record_limit_reached": truncated,
            "limitations": ["Observed journal records only; missing or truncated evidence is not a complete build count.",
                            "Source candidates are workspace-relative positional paths, not reconstructed compiler commands.",
                            "Only known miss categories and diff field names survive; arguments, environment and diff values are omitted."]}


def start(tool, workspace, cache_dir, journal, environ_file):
    workspace = workspace.resolve()
    cache_dir = cache_dir.resolve()
    if not cache_dir.relative_to(workspace).parts:
        raise ValueError("Use a cache subdirectory owned by this job")
    endpoint = cache_dir / "native-journal.sock"
    result = subprocess.run([str(tool), "session-start", "--cache-dir", str(cache_dir),
                             "--endpoint", str(endpoint),
                             "--cwd", str(workspace), "--stats", "--journal", str(journal), "--profile"],
                            check=True, capture_output=True, text=True)
    session = str(uuid.UUID(json.loads(result.stdout)["session_id"]))
    with environ_file.open("a") as output:
        output.write(f"ZCCACHE_CACHE_DIR={cache_dir}\nZCCACHE_ENDPOINT={endpoint}\n"
                     f"ZCCACHE_SESSION_ID={session}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    begin = commands.add_parser("start")
    begin.add_argument("--tool", type=Path, required=True)
    begin.add_argument("--cache-dir", type=Path, required=True)
    for command in (begin, commands.add_parser("summarize")):
        command.add_argument("--workspace", type=Path, required=True)
        command.add_argument("--journal", type=Path, required=True)
    finish = commands.choices["summarize"]
    finish.add_argument("--output", type=Path, required=True)
    finish.add_argument("--build-outcome", choices=("success", "failure", "cancelled", "skipped", "unknown"),
                        default="unknown")
    args = parser.parse_args()
    if args.command == "start":
        start(args.tool, args.workspace, args.cache_dir, args.journal, Path(os.environ["GITHUB_ENV"]))
        print("Native compiler journal enabled for this job-owned cache")
        return
    if args.journal.exists():
        with args.journal.open("rb") as source:
            data = source.read(MAX_BYTES + 1)
        result = summarize(data[:MAX_BYTES].decode("utf-8", errors="replace"), args.workspace)
        result["input_byte_limit_reached"] = len(data) > MAX_BYTES
        if len(data) > MAX_BYTES:
            result["status"] = "partial"
    else:
        result = {"schema": 1, "status": "journal_missing", "counts": {}, "records": []}
    result["build_step_outcome"] = args.build_outcome
    if args.build_outcome != "success" and result["status"] == "ok":
        result["status"] = "partial"
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"Native compiler journal: status={result['status']} observed_counts={result['counts']}")


if __name__ == "__main__":
    main()
