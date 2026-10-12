#!/usr/bin/env python3
"""The contributor leaderboard refreshes every 4 hours.

    python3 -m unittest ci/test_leaderboard_schedule.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parent.parent / ".github" / "workflows" / "leaderboard.yml"


class LeaderboardScheduleTest(unittest.TestCase):
    def setUp(self):
        self.text = WORKFLOW.read_text()

    def test_runs_every_four_hours(self):
        crons = re.findall(r'cron:\s*"([^"]+)"', self.text)
        self.assertEqual(len(crons), 1, crons)
        minute, hour, *rest = crons[0].split()
        self.assertEqual(hour, "*/4", crons[0])
        self.assertEqual(rest, ["*", "*", "*"], crons[0])

    def test_no_time_of_day_gate(self):
        # A scheduled run must always regenerate; nothing may skip it by hour.
        self.assertNotIn("America/Los_Angeles", self.text)
        self.assertNotIn("steps.gate", self.text)

    def test_still_dispatchable(self):
        self.assertIn("workflow_dispatch:", self.text)


if __name__ == "__main__":
    unittest.main()
