#!/usr/bin/env -S uv run --script
"""Read-only CI/cache evidence collector for #1286; needs authenticated gh.

Raw run/job/changed-file/cache responses remain under --output. Sampling is
stratified by event and outcome across the requested window. Cohorts use
the actual compare diff where available, not branch names or skipped tests.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import re
import statistics
import subprocess
from collections import defaultdict
from datetime import datetime, timedelta, timezone
from pathlib import Path


def gh(*args: str):
    return json.loads(subprocess.run(["gh", *args], check=True, capture_output=True, text=True).stdout)


def stamp(value: str) -> datetime:
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def seconds(start: str | None, end: str | None) -> float:
    return max(0.0, (stamp(end) - stamp(start)).total_seconds()) if start and end else 0.0


def cohort(files: list[str]) -> str:
    if not files:
        return "unknown"
    if all(name.endswith(".md") for name in files):
        return "docs"
    if all(name.endswith(".md") or name.startswith(("ci/", ".github/", ".claude/"))
           or name in {"ci.toml", "local-gate.toml"} for name in files):
        return "ci-config"
    return "code"


def collect(repo: str, start: str, end: str, output: Path, per_group: int) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    raw = []
    coverage = {}
    for workflow in ("ci.yml", "push.yaml"):
        listing = gh("run", "list", "-R", repo, "--workflow", workflow, "--limit", "2000", "--json",
                     "databaseId,status,conclusion,event,headSha,headBranch,createdAt,updatedAt,url")
        raw.extend(listing)
        oldest = min((r["createdAt"] for r in listing), default=None)
        coverage[workflow] = {"listed": len(listing), "oldest_created_at": oldest,
                              "window_covered": len(listing) < 2000 or stamp(oldest) <= stamp(start)}
    runs = [r for r in raw if stamp(start) <= stamp(r["createdAt"]) < stamp(end)]
    (output / "runs.json").write_text(json.dumps(runs, indent=2) + "\n")
    groups = defaultdict(list)
    for run in runs:
        if run["status"] == "completed":
            groups[(run["event"], run["conclusion"])].append(run)
    selected = []
    for group in groups.values():
        group.sort(key=lambda r: r["createdAt"])
        count = min(per_group, len(group))
        selected.extend(group[round(i * (len(group) - 1) / max(1, count - 1))] for i in range(count))
        # Ensure scarce docs scenarios are inspected too. This only chooses
        # candidates; classification below still requires the actual diff.
        docs = [r for r in group if (r.get("headBranch") or "").startswith(("docs/", "doc/"))]
        count = min(per_group, len(docs))
        selected.extend(docs[round(i * (len(docs) - 1) / max(1, count - 1))] for i in range(count))
    selected = list({r["databaseId"]: r for r in selected}.values())
    pull_requests = gh("pr", "list", "-R", repo, "--state", "all", "--limit", "2000", "--json",
                       "number,headRefName,headRefOid,baseRefOid,createdAt,closedAt,headRepositoryOwner")

    def inspect(run: dict) -> dict:
        run_id = run["databaseId"]
        cached_path = output / f"run-{run_id}.json"
        cached = json.loads(cached_path.read_text()) if cached_path.exists() else None
        if cached and all(cached.get(key) == run[key] for key in ("updatedAt", "headSha", "status", "conclusion")):
            jobs = cached["jobs"]
            detail = {"run_attempt": cached["run_attempt"], "pull_requests": []}
        else:
            detail = gh("api", f"repos/{repo}/actions/runs/{run_id}")
            pages = gh("api", f"repos/{repo}/actions/runs/{run_id}/jobs?per_page=100", "--paginate", "--slurp")
            jobs = [job for page in pages for job in page["jobs"]]
        base = None
        queue = re.search(r"/pr-\d+-([a-f0-9]{40})$", (run.get("headBranch") or ""))
        if queue:
            base = queue.group(1)
        elif detail.get("pull_requests"):
            base = detail["pull_requests"][0]["base"]["sha"]
        elif run["event"] == "pull_request":
            # GitHub often drops associations from old/merged run metadata.
            # Resolve only an unambiguous branch/lifetime match; never guess
            # docs from a missing association or the branch name itself.
            candidates = [pr for pr in pull_requests if pr["headRefName"] == run["headBranch"]
                          and pr["createdAt"] <= run["createdAt"]
                          and (not pr["closedAt"] or run["createdAt"] <= pr["closedAt"])]
            exact = [pr for pr in candidates if pr["headRefOid"] == run["headSha"]]
            candidates = exact or candidates
            if len(candidates) == 1:
                base = candidates[0]["baseRefOid"]
        files = []
        comparison_error = None
        if cached and cached.get("comparison_base") == base and not cached.get("comparison_error"):
            files = cached["changed_files"]
        elif base:
            try:
                compare = gh("api", f"repos/{repo}/compare/{base}...{run['headSha']}")
                files = [f["filename"] for f in compare["files"]]
                if len(files) >= 300:
                    comparison_error = "GitHub compare file limit; cohort unknown"
            except subprocess.CalledProcessError as error:
                comparison_error = str(error)
        gates = [j for j in jobs if j["name"] == "CI Gate ✅" and j.get("completed_at")]
        record = {
            **run, "cohort": "unknown" if comparison_error else cohort(files),
            "changed_files": files, "comparison_base": base, "comparison_error": comparison_error,
            "run_attempt": detail["run_attempt"], "jobs": jobs,
            "gate_seconds": seconds(run["createdAt"], gates[-1]["completed_at"]) if gates else None,
            "runner_minutes": round(sum(seconds(j.get("started_at"), j.get("completed_at"))
                                        for j in jobs if j["conclusion"] != "skipped") / 60, 3),
        }
        (output / f"run-{run_id}.json").write_text(json.dumps(record, indent=2) + "\n")
        return record

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        records = list(pool.map(inspect, selected))
    caches = gh("api", f"repos/{repo}/actions/caches?per_page=100", "--paginate", "--slurp")
    entries = [entry for page in caches for entry in page["actions_caches"]]
    (output / "caches.json").write_text(json.dumps(entries, indent=2) + "\n")
    summary = {"start": start, "end": end, "captured_at": datetime.now(timezone.utc).isoformat(),
               "listed_runs": len(runs), "sampled_runs": len(records),
               "sample_per_event_outcome": per_group, "cohorts": {},
               "sampling": "Evenly spaced by event/outcome, plus verified documentation candidates",
               "listing_coverage": coverage, "outcomes": {}, "lanes": {},
               "cache_bytes": sum(c["size_in_bytes"] for c in entries), "cache_entries": len(entries),
               "limitations": ["Latest run attempt only; overwritten attempts are not reconstructed.",
                               "Unknown changed-file comparisons are not classified as docs.",
                               "Master pushes without PR metadata have an unknown source cohort.",
                               "Per-workflow listing limit is 2000; inspect coverage for larger windows."]}
    buckets = defaultdict(list)
    outcomes = defaultdict(lambda: defaultdict(int))
    lanes = defaultdict(list)
    for run in runs:
        outcomes[run["event"]][run["conclusion"] or run["status"]] += 1
    summary["outcomes"] = dict(outcomes)
    for record in records:
        buckets[f"{record['event']}/{record['cohort']}"].append(record)
        for job in record["jobs"]:
            if job["conclusion"] != "success":
                continue
            setup = sum(seconds(step.get("started_at"), step.get("completed_at"))
                        for step in job.get("steps", []) if "set up soldr" in step["name"].lower()
                        and not step["name"].lower().startswith("post"))
            lanes[f"{record['event']}/{job['name']}"].append({
                "job_seconds": seconds(job.get("started_at"), job.get("completed_at")),
                "soldr_setup_seconds": setup,
            })
    for name, bucket in buckets.items():
        passing = [r["gate_seconds"] for r in bucket if r["conclusion"] == "success" and r["gate_seconds"] is not None]
        summary["cohorts"][name] = {
            "sampled_runs": len(bucket), "successful_gate_samples": len(passing),
            "median_successful_gate_seconds": statistics.median(passing) if passing else None,
            "canceled_runner_minutes": round(sum(r["runner_minutes"] for r in bucket if r["conclusion"] == "cancelled"), 3),
        }
    for name, samples in lanes.items():
        summary["lanes"][name] = {"successful_job_samples": len(samples), **{
            f"median_{key}": statistics.median(sample[key] for sample in samples)
            for key in ("job_seconds", "soldr_setup_seconds")
        }}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default="obs-rust/obs-studio")
    parser.add_argument("--start", default=(datetime.now(timezone.utc) - timedelta(days=1)).strftime("%Y-%m-%dT%H:%M:%SZ"))
    parser.add_argument("--end", default=datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--per-group", type=int, default=12)
    args = parser.parse_args()
    if args.per_group < 1 or stamp(args.start) >= stamp(args.end):
        parser.error("Use a positive per-group count and an increasing UTC time window")
    print(json.dumps(collect(args.repo, args.start, args.end, args.output, args.per_group), indent=2))
