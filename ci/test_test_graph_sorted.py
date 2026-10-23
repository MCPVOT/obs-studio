#!/usr/bin/env -S uv run --script
"""The test graph lists its targets and tests in name order.

    uv run python -m unittest ci/test_test_graph_sorted.py

Every port adds a cmocka test to test/cmocka/build/test-graph.json. When new
entries go at the end of the `targets` and `tests` maps, two open PRs touch the
same hunk and the second is ejected from the merge queue (#1997). In name order
they land in different places. Readers do not depend on the order: obs-build
loads both maps into a BTreeMap and CMake looks tests up by name.

To fix a failure, run this file with --fix.
"""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GRAPH = ROOT / "test" / "cmocka" / "build" / "test-graph.json"
MAPS = ("targets", "tests")


def unsorted_maps(graph: dict) -> list[str]:
    """The names of the maps whose keys are out of order."""
    return [name for name in MAPS if list(graph.get(name, {})) != sorted(graph.get(name, {}))]


def sorted_graph(graph: dict) -> dict:
    """`graph` with each map's entries in name order; other keys unchanged."""
    return {
        key: dict(sorted(value.items())) if key in MAPS else value
        for key, value in graph.items()
    }


def render(graph: dict) -> str:
    return json.dumps(graph, indent=2, ensure_ascii=False) + "\n"


class TestGraphSorted(unittest.TestCase):
    def test_maps_are_in_name_order(self) -> None:
        graph = json.loads(GRAPH.read_text(encoding="utf-8"))
        self.assertEqual(
            unsorted_maps(graph),
            [],
            "run `uv run ci/test_test_graph_sorted.py --fix`",
        )

    def test_unsorted_map_is_reported(self) -> None:
        graph = {"schema": 1, "targets": {"b": {}, "a": {}}, "tests": {"a": {}}}
        self.assertEqual(unsorted_maps(graph), ["targets"])
        self.assertEqual(unsorted_maps(sorted_graph(graph)), [])
        self.assertEqual(list(sorted_graph(graph)), ["schema", "targets", "tests"])


if __name__ == "__main__":
    if sys.argv[1:] == ["--fix"]:
        GRAPH.write_text(
            render(sorted_graph(json.loads(GRAPH.read_text(encoding="utf-8")))),
            encoding="utf-8",
        )
    else:
        unittest.main()
