#!/usr/bin/env -S uv run --script
"""The claim bot labels and assigns claimants and expires stale claims (#909).

    uv run python -m unittest ci/test_claim_bot.py

Outside contributors cannot add the `claimed` label, so their claims
(#777, #809, #811, #815, #817) went unlabeled and the grind bot, which skips
only labeled issues, could duplicate their work. The 48 h expiry was
honor-system. These tests pin the bot's decisions; GitHub I/O is not run.
"""

from __future__ import annotations

import importlib.util
import re
import sys
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "tools" / "claim-bot" / "claim_bot.py"
WORKFLOW = ROOT / ".github" / "workflows" / "claim-bot.yaml"


def load():
    spec = importlib.util.spec_from_file_location("claim_bot", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod  # dataclasses look the module up by name
    spec.loader.exec_module(mod)
    return mod


cb = load()
T0 = datetime(2026, 10, 15, 12, 0, tzinfo=timezone.utc)


def h(n: float) -> datetime:
    return T0 + timedelta(hours=n)


def human(author, body, at, assoc="CONTRIBUTOR"):
    return cb.Comment(author, assoc, body, at)


def bot(body, at):
    return cb.Comment(cb.BOT, "NONE", f"{cb.BOT_PREFIX} {body}", at)


def issue(comments=(), labels=(), prs=()):
    return cb.Issue(1, set(labels), [], list(comments), list(prs))


def kinds(actions):
    return [a[0] for a in actions]


def claimed_by_alice(labels=(cb.CLAIMED,), prs=()):
    return issue([human("alice", "Claiming: x", h(0)), bot("claimed by @alice. ...", h(0))], labels, prs)


class ClaimTest(unittest.TestCase):
    def test_outside_contributor_claim_is_labeled_and_assigned(self):
        acts = cb.decide_comment(issue(), human("vpacher", "Claiming: audio-io\n1. plan", h(0)), h(0))
        self.assertIn(("label_add", cb.CLAIMED), acts)
        self.assertIn(("assign", "vpacher"), acts)
        self.assertRegex(acts[-1][1], r"claimed by @vpacher")

    def test_claim_is_case_insensitive_and_first_line_only(self):
        self.assertTrue(cb.decide_comment(issue(), human("a", "claiming: x", h(0)), h(0)))
        self.assertEqual(cb.decide_comment(issue(), human("a", "plan\nClaiming: x", h(0)), h(0)), [])

    def test_fresh_claim_by_someone_else_is_refused(self):
        acts = cb.decide_comment(claimed_by_alice(), human("bob", "Claiming: x", h(1)), h(1))
        self.assertEqual(kinds(acts), ["comment"])
        self.assertIn("already claimed by @alice", acts[0][1])

    def test_one_account_may_hold_any_number_of_claims(self):
        """Several agents on one maintainer's machine share one GitHub
        account; a per-account cap refused their claims. Expiry handles
        hoarding instead."""
        acts = cb.decide_comment(issue(), human("zackees", "Claiming: x", h(0), "MEMBER"), h(0))
        self.assertIn(("assign", "zackees"), acts)
        self.assertNotIn("limit", acts[-1][1])

    def test_reclaim_by_holder_is_a_no_op(self):
        self.assertEqual(cb.decide_comment(claimed_by_alice(), human("alice", "Claiming: x", h(1)), h(1)), [])

    def test_claim_on_a_stale_claim_moves_it(self):
        acts = cb.decide_comment(claimed_by_alice(), human("bob", "Claiming: x", h(49)), h(49))
        self.assertIn(("unassign", "alice"), acts)
        self.assertIn(("assign", "bob"), acts)

    def test_locked_issue_is_ignored(self):
        self.assertEqual(cb.decide_comment(issue(labels=[cb.LOCKED]), human("a", "Claiming: x", h(0)), h(0)), [])

    def test_bot_comments_are_ignored(self):
        self.assertEqual(cb.decide_comment(issue(), cb.Comment("x[bot]", "NONE", "Claiming: x", h(0)), h(0)), [])


class ReleaseTest(unittest.TestCase):
    def test_holder_releases(self):
        acts = cb.decide_comment(claimed_by_alice(), human("alice", "Releasing x, branch foo", h(2)), h(2))
        self.assertEqual(kinds(acts), ["unassign", "label_remove", "comment"])

    def test_others_cannot_release(self):
        self.assertEqual(cb.decide_comment(claimed_by_alice(), human("bob", "Releasing x", h(2)), h(2)), [])

    def test_released_issue_has_no_claim(self):
        i = claimed_by_alice()
        i.comments.append(bot("claim released by @alice.", h(3)))
        self.assertIsNone(cb.current_claim(i))


class TakeoverTest(unittest.TestCase):
    def test_takeover_of_fresh_claim_is_refused(self):
        acts = cb.decide_comment(claimed_by_alice(),
                                 human("bob", "Taking over stale claim from @alice", h(10)), h(10))
        self.assertEqual(kinds(acts), ["comment"])
        self.assertIn("still active", acts[0][1])

    def test_takeover_of_stale_claim_moves_it(self):
        acts = cb.decide_comment(claimed_by_alice(),
                                 human("bob", "Taking over stale claim from @alice", h(48)), h(48))
        self.assertIn(("assign", "bob"), acts)
        self.assertRegex(acts[-1][1], r"claim moved to @bob")

    def test_takeover_naming_the_wrong_holder_is_refused(self):
        acts = cb.decide_comment(claimed_by_alice(),
                                 human("bob", "Taking over stale claim from @carol", h(48)), h(48))
        self.assertEqual(kinds(acts), ["comment"])

    def test_critical_claim_with_idle_draft_pr_is_stealable_after_8h(self):
        pr = cb.LinkedPR(5, "alice", [h(1)])
        i = claimed_by_alice(labels=(cb.CLAIMED, cb.CRITICAL), prs=[pr])
        take = human("bob", "Taking over stale claim from @alice", h(8.5))
        self.assertIn(("assign", "bob"), cb.decide_comment(i, take, h(9)))
        early = cb.decide_comment(i, take, h(8.5))
        self.assertEqual(kinds(early), ["comment"])

    def test_pushes_to_the_pr_keep_a_critical_claim(self):
        pr = cb.LinkedPR(5, "alice", [h(1), h(7)])
        i = claimed_by_alice(labels=(cb.CLAIMED, cb.CRITICAL), prs=[pr])
        take = human("bob", "Taking over stale claim from @alice", h(10))
        self.assertEqual(kinds(cb.decide_comment(i, take, h(10))), ["comment"])

    def test_someone_elses_pr_does_not_count_as_progress(self):
        pr = cb.LinkedPR(5, "mallory", [h(40)])
        i = claimed_by_alice(prs=[pr])
        self.assertTrue(cb.is_stale(i, cb.current_claim(i), h(48)))


class ReassignTest(unittest.TestCase):
    def test_maintainer_reassigns_a_fresh_claim(self):
        acts = cb.decide_comment(claimed_by_alice(), human("zackees", "/reassign @bob", h(1), "MEMBER"), h(1))
        self.assertIn(("unassign", "alice"), acts)
        self.assertIn(("assign", "bob"), acts)

    def test_contributor_cannot_reassign(self):
        acts = cb.decide_comment(claimed_by_alice(), human("bob", "/reassign @bob", h(1)), h(1))
        self.assertEqual(kinds(acts), ["comment"])


class SweepTest(unittest.TestCase):
    def test_fresh_claim_is_left_alone(self):
        self.assertEqual(cb.decide_sweep(claimed_by_alice(), h(10)), [])

    def test_warns_two_hours_before_expiry_once(self):
        i = claimed_by_alice()
        acts = cb.decide_sweep(i, h(46.5))
        self.assertEqual(kinds(acts), ["comment"])
        self.assertIn("expires in", acts[0][1])
        i.comments.append(cb.Comment(cb.BOT, "NONE", acts[0][1], h(46.5)))
        self.assertEqual(cb.decide_sweep(i, h(47)), [])

    def test_expires_after_48h(self):
        acts = cb.decide_sweep(claimed_by_alice(), h(48))
        self.assertEqual(kinds(acts), ["unassign", "label_remove", "comment"])

    def test_claimant_comment_resets_the_clock(self):
        i = claimed_by_alice()
        i.comments.append(human("alice", "progress: RED test pushed", h(40)))
        self.assertEqual(cb.decide_sweep(i, h(60)), [])

    def test_critical_expires_after_8h(self):
        self.assertIn("label_remove", kinds(cb.decide_sweep(claimed_by_alice((cb.CLAIMED, cb.CRITICAL)), h(8))))

    def test_locked_is_never_expired(self):
        self.assertEqual(cb.decide_sweep(claimed_by_alice((cb.CLAIMED, cb.LOCKED)), h(100)), [])

    def test_pre_bot_claim_is_read_from_the_comment(self):
        i = issue([human("nathan-v", "Claiming: x", h(0))], [cb.CLAIMED])
        self.assertEqual(cb.current_claim(i).user, "nathan-v")
        self.assertIn("label_remove", kinds(cb.decide_sweep(i, h(48))))

    def test_unlabeled_issue_without_bot_record_has_no_claim(self):
        self.assertIsNone(cb.current_claim(issue([human("a", "Claiming: x", h(0))])))


class WorkflowTest(unittest.TestCase):
    def setUp(self):
        self.text = WORKFLOW.read_text(encoding="utf-8")

    def test_triggers_on_comments_and_hourly(self):
        self.assertIn("issue_comment:", self.text)
        self.assertRegex(self.text, r'cron: "\d+ \* \* \* \*"')

    def test_token_can_write_issues_only(self):
        self.assertRegex(self.text, r"issues: write")
        self.assertNotIn("contents: write", self.text)
        self.assertNotIn("pull_request_target", self.text)

    def test_comment_body_never_reaches_a_shell(self):
        self.assertNotRegex(self.text, r"\$\{\{\s*github\.event\.comment")


if __name__ == "__main__":
    unittest.main()
