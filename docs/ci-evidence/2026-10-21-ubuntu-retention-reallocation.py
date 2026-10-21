#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["ci-lint @ git+https://github.com/zackees/ci.yml@1a970f01574447d681571ff3d0b8de56cb983dcc"]
# ///
"""Offline before/after janitor probe for #1269; no network or live deletes.

Run from the repository root. The fixture combines the saved October 21
inventory with the two measured October 18 Ubuntu archives. It is a modeled
coexistence scenario, not a claim that these entries coexist in GitHub today.
"""

import argparse
import json
from datetime import datetime
from pathlib import Path
from urllib.parse import parse_qs, urlparse

from ci_lint.cache.ops import janitor
from ci_lint.rules.cache_static import family_footprint
from ci_lint.schema import load_ci_toml


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    evidence = Path("docs/ci-evidence")
    bundle = json.loads((evidence / "2026-10-21-cache-retention-inputs.json").read_text())
    capture = bundle["inventory.json"]
    historical = json.loads((evidence / "2026-10-18-ubuntu-retention-plan.json").read_text())
    entries = [entry for page in capture["pages"] for entry in page["actions_caches"]
               if not entry["key"].startswith("zccache-ubuntu-")]
    for entry in historical["ubuntu_entries"]:
        entries.append({**entry, "ref": "refs/heads/master", "version": "historical-version-not-recorded"})
    # Synthetic newer, oversized AppImage/sanitizer writes exercise family
    # isolation. They are not measurements or promises about those payloads.
    for index, kind in enumerate(("appimage", "sanitize"), start=1):
        entries.append({
            "id": -index, "key": f"zccache-{kind}-hypothetical-seed",
            "ref": "refs/heads/master", "version": "synthetic",
            "size_in_bytes": 2 * 1024 ** 3,
            "created_at": capture["completed_at"], "last_accessed_at": capture["completed_at"],
        })

    def fetch(url, _token):
        parsed = urlparse(url)
        if parsed.path != "/repos/obs-rust/obs-studio/actions/caches":
            raise ValueError(f"unexpected offline request: {parsed.path}")
        page = int(parse_qs(parsed.query)["page"][0])
        return {"total_count": len(entries), "actions_caches": entries[(page - 1) * 100:page * 100]}

    output = {"pin": historical["pin"], "scenario": __doc__, "plans": {}}
    ubuntu_ids = {entry["id"] for entry in historical["ubuntu_entries"]}
    for label, text in [("before", bundle["ci.toml"]), ("after", Path("ci.toml").read_text())]:
        root = args.output / label
        root.mkdir(parents=True, exist_ok=True)
        (root / "ci.toml").write_text(text)
        config, findings = load_ci_toml(root)
        if config is None:
            raise ValueError(str(findings))
        result, _ = janitor(
            config, fetch=fetch, graphql=None, delete=None, token="offline",
            repo="obs-rust/obs-studio", default_branch="master", dry_run=True,
            now=datetime.fromisoformat(capture["completed_at"]).timestamp() + 601,
            max_deletes=len(entries),
        )
        output["plans"][label] = {
            "declared_bytes": sum(family_footprint(config, family) for family in config.cache.family.values()),
            "planned_ubuntu_deletions": [entry.__dict__ for entry in result.planned if entry.id in ubuntu_ids],
            "all_planned_ids": [entry.id for entry in result.planned],
            "warning": result.warning, "live_deletes": list(result.deleted),
        }
    assert output["plans"]["before"]["planned_ubuntu_deletions"], "fixture must reproduce the loss"
    assert not output["plans"]["after"]["planned_ubuntu_deletions"], "both modes must survive"
    assert set(output["plans"]["after"]["all_planned_ids"]) <= set(output["plans"]["before"]["all_planned_ids"]), "no other fixture seed may become deletable"
    assert output["plans"]["after"]["declared_bytes"] < output["plans"]["before"]["declared_bytes"]
    (args.output / "report.json").write_text(json.dumps(output, indent=2) + "\n")
    print(json.dumps({label: plan["declared_bytes"] for label, plan in output["plans"].items()}))


if __name__ == "__main__":
    main()
