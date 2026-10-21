#!/usr/bin/env -S uv run --script
"""Refuse publication until the hybrid default's release evidence is reviewed."""

import datetime
import json
import re
import sys
from pathlib import Path

EVIDENCE = Path(__file__).resolve().parent.parent / "docs/rust-port/hybrid-release-evidence.json"


def validate(evidence):
    if evidence.get("approved") is not True:
        raise ValueError("Hybrid release approval is pending; see docs/rust-port/release-criteria.md and #1587")
    if not re.fullmatch(r"[0-9a-f]{40}", evidence.get("candidate", "")):
        raise ValueError("Record the validated release-candidate commit")
    for category in ("compatibility", "application", "performance"):
        links = evidence.get(category, [])
        if not links or any(not isinstance(link, str) or not link.startswith("https://") for link in links):
            raise ValueError(f"Record reviewed {category} evidence links for every release platform")
    days = [datetime.date.fromisoformat(day) for day in evidence.get("stability_days", [])]
    if len(days) < 14 or any(b - a != datetime.timedelta(days=1) for a, b in zip(days, days[1:])):
        raise ValueError("Record at least 14 consecutive successful daily stability builds")
    if not evidence.get("review", "").startswith("https://"):
        raise ValueError("Link the maintainer review of platform coverage, performance bounds and release blockers")


if __name__ == "__main__":
    try:
        validate(json.loads(EVIDENCE.read_text()))
    except (ValueError, TypeError, OSError) as error:
        print(f"::error::{error}", file=sys.stderr)
        raise SystemExit(1) from error
