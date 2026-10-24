#!/usr/bin/env -S uv run --script
"""Print a ctest regex matching exactly the RUN_SERIAL tests of a registry.

Usage: serial-tests.py REGISTRY.json [I/N]   (ctest --show-only=json-v1 output)

With I/N, only every Nth of the sorted tests, starting at the Ith: CI runs the
serial tests on N runners (#2072).

Tier 2 runs these tests on their own runner and the rest in parallel on
another (#2071). Fails when there are none: an empty serial part would pass
without running anything.
"""

import json
import re
import sys


def main() -> None:
    tests = json.load(open(sys.argv[1], encoding="utf-8"))["tests"]
    names = sorted(test["name"] for test in tests
                   if any(p["name"] == "RUN_SERIAL" and p["value"] for p in test.get("properties", [])))
    if len(sys.argv) > 2:
        index, count = (int(n) for n in sys.argv[2].split("/"))
        if not 1 <= index <= count:
            sys.exit("FAIL: shard must be I/N with 1 <= I <= N, got " + sys.argv[2])
        names = names[index - 1::count]
    if not names:
        sys.exit("FAIL: no RUN_SERIAL tests in " + sys.argv[1])
    print("^(" + "|".join(re.escape(name) for name in names) + ")$")


if __name__ == "__main__":
    main()
