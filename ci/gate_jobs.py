#!/usr/bin/env -S uv run --script
"""Fail CI Gate when any job of this run did not succeed (#1793).

    uv run ci/gate_jobs.py

`needs.*.result` only sees the gate's direct needs. A job inside the
reusable build-project call can end cancelled (for example, "The job was
not acquired by Runner of type hosted even after multiple attempts") while
the call itself reports success, so the gate lists every job of the run
attempt instead. Run in CI Gate with GITHUB_RUN_ID, GITHUB_RUN_ATTEMPT and
GITHUB_REPOSITORY set and gh authenticated (actions: read).
Standard library only.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys

UNSUCCESSFUL = {"failure", "cancelled", "timed_out", "startup_failure", "action_required"}
# Maintenance outside the gate (ci/test_merge_queue.py), and the gate itself.
EXEMPT_PREFIXES = ("CI Pre 🧹", "Cache Janitor 🧹", "CI Gate ✅")


def blocking(jobs: list[dict]) -> list[str]:
    """Names of the finished jobs that should fail the gate."""
    return [
        j["name"] for j in jobs
        if j.get("status") == "completed"
        and j.get("conclusion") in UNSUCCESSFUL
        and not j["name"].startswith(EXEMPT_PREFIXES)
    ]


def run_jobs() -> list[dict]:
    path = (f"repos/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}"
            f"/attempts/{os.environ.get('GITHUB_RUN_ATTEMPT', '1')}/jobs?per_page=100")
    out = subprocess.run(["gh", "api", "--paginate", path, "--jq", ".jobs[]"],
                         check=True, capture_output=True, text=True).stdout
    return [json.loads(line) for line in out.splitlines() if line.strip()]


def main() -> int:
    bad = blocking(run_jobs())
    for name in bad:
        print(f"::error::{name} did not succeed")
    if not bad:
        print("Every finished job of this run succeeded or was skipped.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
