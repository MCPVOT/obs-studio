#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["ci-lint @ git+https://github.com/zackees/ci.yml@1a970f01574447d681571ff3d0b8de56cb983dcc"]
# ///
"""Read-only #1269 inventory and offline pinned-janitor reproduction.

Run from the repository root with --output DIRECTORY. Use --replay to
recompute from that directory without network calls. No delete callback is
provided, and the simulated janitor fetch can only return saved cache pages.
"""

import argparse
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from ci_lint.cache.audit import classify
from ci_lint.cache.github_cache import CacheEntry
from ci_lint.cache.ops import janitor
from ci_lint.rules.cache_static import family_footprint, parse_size
from ci_lint.schema import load_ci_toml


def gh(endpoint, *args):
    return json.loads(subprocess.run(
        ["gh", "api", endpoint, *args], check=True, capture_output=True, text=True,
    ).stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--replay", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    capture_path = args.output / "inventory.json"
    if args.replay:
        capture = json.loads(capture_path.read_text())
    else:
        capture = {
            "started_at": datetime.now(timezone.utc).isoformat(),
            "revision": subprocess.run(
                ["git", "rev-parse", "HEAD"], check=True, capture_output=True, text=True,
            ).stdout.strip(),
            "usage": gh("repos/obs-rust/obs-studio/actions/cache/usage"),
            "pages": gh("repos/obs-rust/obs-studio/actions/caches?per_page=100", "--paginate", "--slurp"),
            "completed_at": datetime.now(timezone.utc).isoformat(),
        }
        capture_path.write_text(json.dumps(capture, indent=2) + "\n")
        # The exact configuration travels with the snapshot for offline replay.
        (args.output / "ci.toml").write_bytes(Path("ci.toml").read_bytes())

    config, findings = load_ci_toml(args.output)
    if config is None:
        raise ValueError(f"invalid saved configuration: {findings}")
    raw = [entry for page in capture["pages"] for entry in page["actions_caches"]]
    if len({entry["id"] for entry in raw}) != len(raw):
        raise ValueError("cache listing changed during pagination: duplicate IDs; recapture")
    if any(page["total_count"] != len(raw) for page in capture["pages"]):
        raise ValueError("cache listing count changed during pagination; recapture")
    entries = [CacheEntry(**{field: entry[field] for field in CacheEntry.__dataclass_fields__}) for entry in raw]
    classified = classify(config, entries)
    families = {}
    for name, family in config.cache.family.items():
        members = [item.entry for item in classified if item.family_id == name]
        families[name] = {
            "declared_bytes": family_footprint(config, family),
            "listed_bytes": sum(item.size_in_bytes for item in members),
            "entries": len(members),
        }

    def saved_fetch(url, _token):
        # Fail if the executor ever attempts a non-cache API, rather than
        # quietly sending an authenticated request during offline replay.
        from urllib.parse import parse_qs, urlparse
        parsed = urlparse(url)
        if parsed.path != "/repos/obs-rust/obs-studio/actions/caches":
            raise ValueError(f"unexpected offline request: {parsed.path}")
        page = int(parse_qs(parsed.query)["page"][0])
        ordered = sorted(raw, key=lambda entry: entry["size_in_bytes"], reverse=True)
        return {"total_count": len(raw), "actions_caches": ordered[(page - 1) * 100:page * 100]}

    now = datetime.fromisoformat(capture["completed_at"]).timestamp()
    plans = {}
    for label, clock in [("capture_time", now), ("after_grace_hypothetical", now + 601)]:
        result, table = janitor(
            config, fetch=saved_fetch, graphql=None, delete=None, token="offline",
            repo="obs-rust/obs-studio", default_branch="master", dry_run=True,
            now=clock, max_deletes=len(raw),
        )
        plans[label] = {**result.to_json_dict(), "family_table": table}
    listed = sum(entry.size_in_bytes for entry in entries)
    budget = parse_size(config.cache.budget)
    report = {
        "pin": "1a970f01574447d681571ff3d0b8de56cb983dcc",
        "revision": capture["revision"],
        "started_at": capture["started_at"], "completed_at": capture["completed_at"],
        "budget_bytes": budget,
        "declared_bytes": sum(row["declared_bytes"] for row in families.values()),
        "listed_entries": len(entries), "listed_bytes": listed,
        "usage_bytes": capture["usage"]["active_caches_size_in_bytes"],
        "usage_count": capture["usage"]["active_caches_count"],
        "listed_headroom_bytes": budget - listed,
        "families": families, "offline_janitor": plans,
        "limits": [
            "Usage and paginated inventory are separate non-atomic observations.",
            "No PR-state queries: unknown PR state is retained conservatively; warnings are included.",
            "Future-clock plan is hypothetical, not an observed deletion or retention guarantee.",
            "No consumer hit-rate, speedup, or representative-day durability follows from this snapshot.",
        ],
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in (
        "declared_bytes", "budget_bytes", "listed_bytes", "usage_bytes", "listed_headroom_bytes",
    )}))


if __name__ == "__main__":
    main()
