#!/usr/bin/env -S uv run --script
"""The leaderboard page lists the top five contributors and hides the rest
behind a "Show all" button, so the README image stays short as the list
grows. The page opens with the full list when its URL ends in #all.

    uv run python -m unittest ci/test_leaderboard_top_five.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

PAGE = Path(__file__).resolve().parent.parent / "tools/leaderboard/site/index.html"


class TopFiveTest(unittest.TestCase):
    def setUp(self):
        self.html = PAGE.read_text()

    def test_shows_five_rows_first(self):
        match = re.search(r"\bconst TOP=(\d+)", self.html)
        self.assertIsNotNone(match, "index.html must set how many rows show first (const TOP=...)")
        self.assertEqual(match.group(1), "5")

    def test_rows_after_the_top_are_hidden_until_shown(self):
        self.assertRegex(self.html, r"\.row\.more\{display:none\}", "rows past the top five must start hidden")
        self.assertRegex(self.html, r"\.board\.all \.row\.more\{display:grid\}", "showing all must reveal them")

    def test_show_all_button(self):
        self.assertIn('id="more"', self.html, "a button must reveal the full list")
        self.assertIn("Show all", self.html)

    def test_hash_all_opens_the_full_list(self):
        self.assertIn('location.hash==="#all"', self.html)


if __name__ == "__main__":
    unittest.main()
