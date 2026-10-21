#!/usr/bin/env -S uv run --script
"""Verify duration-balanced shards partition the actual CTest registry (#1581).

Usage: check-plan.py CTEST_JSON PLAN_DIRECTORY SHARD_COUNT
Read-only: checks every shard before any filtered CTest execution. No changes to
test definitions, RUN_SERIAL isolation, or per-test assertions.
"""

import argparse
import json
from pathlib import Path
import re
import sys


def check(registry: dict, directory: Path, shards: int) -> list[int]:
    names = [test["name"] for test in registry["tests"]]
    if shards < 1 or not names or len(set(names)) != len(names):
        raise ValueError("expected positive shard count and nonempty unique CTest registry")
    assigned = {name: 0 for name in names}
    counts = []
    for index in range(1, shards + 1):
        path = directory / f"{index}-of-{shards}.regex"
        if not path.is_file():
            raise ValueError(f"missing shard: {path.name}")
        pattern = path.read_text().strip()
        if not pattern.startswith("^(") or not pattern.endswith(")$"):
            raise ValueError(f"invalid regex: {path.name} is not an anchored test list")
        # The planner emits a literal list, so validate using the same portable
        # names rather than allowing Python/CTest regex dialect differences.
        selected = pattern[2:-2].split("|")
        if selected == [""]:
            raise ValueError(f"empty shard: {path.name}")
        for name in selected:
            if not re.fullmatch(r"[A-Za-z0-9_-]+", name):
                raise ValueError(f"invalid regex: {path.name}: {name!r}")
            if name not in assigned:
                raise ValueError(f"unregistered test: {name}")
            assigned[name] += 1
        counts.append(len(selected))
    bad = [name for name, count in assigned.items() if count != 1]
    if bad:
        raise ValueError(f"tests must occur exactly once: {', '.join(bad)}")
    return counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("registry", type=Path)
    parser.add_argument("directory", type=Path)
    parser.add_argument("shards", type=int)
    args = parser.parse_args()
    try:
        counts = check(json.loads(args.registry.read_text()), args.directory, args.shards)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"FAIL: Tier 2 plan: {error}", file=sys.stderr)
        return 1
    print(f"Tier 2 plan: {sum(counts)} tests across {args.shards} disjoint shards; counts={counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
