#!/usr/bin/env -S uv run --script
"""Claim bot for obs-rust/obs-studio (docs/rust-port/coordination.md §2-3).

Usage:
  claim_bot.py comment [--dry-run]   handle the issue_comment event in $GITHUB_EVENT_PATH
  claim_bot.py sweep [--dry-run]     warn about and expire stale claims

Needs the `gh` CLI, authenticated (GH_TOKEN in CI). Standard library only.

Commands, read from the first line of an issue comment:
  Claiming: <item>                  claim the issue
  Releasing <item>                  the claimant gives it up
  Taking over stale claim from @x   take a stale claim
  /reassign @y                      maintainers (OWNER/MEMBER/COLLABORATOR) move any claim

A claim goes stale after STALE_HOURS (CRITICAL_STALE_HOURS on issues labeled
`critical`) with no comment from the claimant on the issue and no commit,
comment or review on an open PR they opened that references the issue. The
sweep warns WARN_HOURS before that and then removes the claim. Issues labeled
`claim-locked` are left alone.

State lives in the visible history: the bot's own comments start with
BOT_PREFIX and name the claimant. Claims made before the bot existed are
read from the latest human `Claiming:` comment on an issue labeled `claimed`.
The functions above main() are pure and tested by ci/test_claim_bot.py.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone

REPO = "obs-rust/obs-studio"
BOT = "github-actions[bot]"
BOT_PREFIX = "**Claim bot:**"
CLAIMED = "claimed"
CRITICAL = "critical"
LOCKED = "claim-locked"
STALE_HOURS = 48
CRITICAL_STALE_HOURS = 8
WARN_HOURS = 2
MAINTAINERS = {"OWNER", "MEMBER", "COLLABORATOR"}

CLAIM_RE = re.compile(r"^\s*claiming\b", re.I)
RELEASE_RE = re.compile(r"^\s*releasing\b", re.I)
TAKEOVER_RE = re.compile(r"^\s*taking over stale claim from @([A-Za-z0-9-]+)", re.I)
REASSIGN_RE = re.compile(r"^\s*/reassign\s+@([A-Za-z0-9-]+)", re.I)
# The bot's own records. Each names the claimant after it, or none.
BOT_CLAIM_RE = re.compile(re.escape(BOT_PREFIX) + r" (?:claimed by|claim moved to) @([A-Za-z0-9-]+)")
BOT_CLEAR_RE = re.compile(re.escape(BOT_PREFIX) + r" claim (?:released|expired)")
BOT_WARN_RE = re.compile(re.escape(BOT_PREFIX) + r" claim by @[A-Za-z0-9-]+ expires")


@dataclass
class Comment:
    author: str
    association: str
    body: str
    created: datetime

    def first_line(self) -> str:
        return self.body.strip().splitlines()[0] if self.body.strip() else ""


@dataclass
class LinkedPR:
    number: int
    author: str
    activity: list[datetime] = field(default_factory=list)


@dataclass
class Issue:
    number: int
    labels: set[str]
    assignees: list[str]
    comments: list[Comment]
    prs: list[LinkedPR] = field(default_factory=list)


@dataclass
class Claim:
    user: str
    since: datetime


def is_bot(login: str) -> bool:
    return login.endswith("[bot]")


def current_claim(issue: Issue) -> Claim | None:
    """The claimant, from the bot's records, else a pre-bot `Claiming:` comment."""
    claim: Claim | None = None
    seen_bot_record = False
    for c in sorted(issue.comments, key=lambda c: c.created):
        if c.author != BOT:
            continue
        if m := BOT_CLAIM_RE.match(c.body):
            claim, seen_bot_record = Claim(m.group(1), c.created), True
        elif BOT_CLEAR_RE.match(c.body):
            claim, seen_bot_record = None, True
    if seen_bot_record or CLAIMED not in issue.labels:
        return claim
    human = [c for c in issue.comments if not is_bot(c.author) and CLAIM_RE.match(c.first_line())]
    if not human:
        return None
    last = max(human, key=lambda c: c.created)
    return Claim(last.author, last.created)


def stale_hours(issue: Issue) -> int:
    return CRITICAL_STALE_HOURS if CRITICAL in issue.labels else STALE_HOURS


def last_activity(issue: Issue, claim: Claim) -> datetime:
    """Latest sign of the claimant working: the claim, their issue comments, their PRs."""
    times = [claim.since]
    times += [c.created for c in issue.comments if c.author.lower() == claim.user.lower()]
    for pr in issue.prs:
        if pr.author.lower() == claim.user.lower():
            times += pr.activity
    return max(times)


def is_stale(issue: Issue, claim: Claim, now: datetime) -> bool:
    return now - last_activity(issue, claim) >= timedelta(hours=stale_hours(issue))


def decide_comment(issue: Issue, comment: Comment, now: datetime) -> list[tuple]:
    """Actions for one new comment. There is no per-account claim limit:
    several agents can share one maintainer's account, and expiry handles
    hoarding."""
    if is_bot(comment.author) or LOCKED in issue.labels:
        return []
    line = comment.first_line()
    claim = current_claim(issue)
    who = comment.author

    if m := REASSIGN_RE.match(line):
        if comment.association not in MAINTAINERS:
            return [("comment", f"{BOT_PREFIX} only maintainers can `/reassign`.")]
        return move(claim, m.group(1), f"reassigned by @{who}")

    if m := TAKEOVER_RE.match(line):
        if claim is None:
            return grant(who, "the issue had no claim")
        if claim.user.lower() != m.group(1).lower():
            return [("comment", f"{BOT_PREFIX} @{who}, the current claim is held by @{claim.user}, not @{m.group(1)}.")]
        if not is_stale(issue, claim, now):
            left = timedelta(hours=stale_hours(issue)) - (now - last_activity(issue, claim))
            return [("comment", f"{BOT_PREFIX} @{who}, the claim by @{claim.user} is still active; "
                     f"it goes stale in {fmt_hours(left)} without progress.")]
        return move(claim, who, f"stale claim taken over from @{claim.user}")

    if CLAIM_RE.match(line):
        if claim and claim.user.lower() == who.lower():
            return []
        if claim and not is_stale(issue, claim, now):
            return [("comment", f"{BOT_PREFIX} @{who}, this is already claimed by @{claim.user}. "
                     "Pick something else or offer help here; once the claim goes stale, "
                     f"comment `Taking over stale claim from @{claim.user}`.")]
        if claim:
            return move(claim, who, f"stale claim from @{claim.user}")
        return grant(who, None)

    if RELEASE_RE.match(line) and claim and claim.user.lower() == who.lower():
        return [("unassign", claim.user), ("label_remove", CLAIMED),
                ("comment", f"{BOT_PREFIX} claim released by @{who}.")]
    return []


def grant(who: str, note: str | None) -> list[tuple]:
    suffix = f" ({note})" if note else ""
    return [("label_add", CLAIMED), ("assign", who),
            ("comment", f"{BOT_PREFIX} claimed by @{who}{suffix}. {window_note()}")]


def move(claim: Claim | None, to: str, why: str) -> list[tuple]:
    acts: list[tuple] = []
    if claim and claim.user.lower() != to.lower():
        acts.append(("unassign", claim.user))
    acts += [("label_add", CLAIMED), ("assign", to),
             ("comment", f"{BOT_PREFIX} claim moved to @{to} ({why}). {window_note()}")]
    return acts


def decide_sweep(issue: Issue, now: datetime) -> list[tuple]:
    """Warn WARN_HOURS before a claim goes stale, and expire it once it has."""
    if LOCKED in issue.labels:
        return []
    claim = current_claim(issue)
    if claim is None:
        return []
    last = last_activity(issue, claim)
    idle = now - last
    limit = timedelta(hours=stale_hours(issue))
    if idle >= limit:
        return [("unassign", claim.user), ("label_remove", CLAIMED),
                ("comment", f"{BOT_PREFIX} claim expired: no progress from @{claim.user} in "
                 f"{stale_hours(issue)} h. Anyone may claim it now.")]
    if idle >= limit - timedelta(hours=WARN_HOURS):
        warned = any(c.author == BOT and BOT_WARN_RE.match(c.body) and c.created > last for c in issue.comments)
        if not warned:
            return [("comment", f"{BOT_PREFIX} claim by @{claim.user} expires in {fmt_hours(limit - idle)} "
                     "unless there is progress (a comment here, or a push, comment or review on your PR).")]
    return []


def window_note() -> str:
    return (f"It expires after {STALE_HOURS} h ({CRITICAL_STALE_HOURS} h on `critical` issues) without "
            "a progress comment here or a push, comment or review on a linked PR.")


def fmt_hours(d: timedelta) -> str:
    hours = max(0.0, d.total_seconds() / 3600)
    return f"{hours:.0f} h" if hours >= 1 else f"{max(1, round(hours * 60))} min"


# ---- GitHub I/O -------------------------------------------------------------


def gh_json(path: str, paginate: bool = True):
    args = ["gh", "api", "-H", "Accept: application/vnd.github+json", path]
    if paginate:
        args[2:2] = ["--paginate", "--slurp"]
    out = json.loads(subprocess.run(args, check=True, capture_output=True, text=True).stdout)
    if paginate:
        return [x for page in out for x in page]
    return out


def ts(s: str) -> datetime:
    return datetime.fromisoformat(s.replace("Z", "+00:00"))


def load_pr(number: int) -> LinkedPR:
    pr = gh_json(f"repos/{REPO}/pulls/{number}", paginate=False)
    activity = [ts(pr["created_at"])]
    for ev in gh_json(f"repos/{REPO}/issues/{number}/timeline?per_page=100"):
        if ev.get("event") == "committed":
            activity.append(ts(ev["committer"]["date"]))
        elif ev.get("event") in ("commented", "reviewed", "ready_for_review", "head_ref_force_pushed"):
            stamp = ev.get("created_at") or ev.get("submitted_at")
            actor = (ev.get("user") or ev.get("actor") or {}).get("login", "")
            if stamp and actor.lower() == pr["user"]["login"].lower():
                activity.append(ts(stamp))
    return LinkedPR(number, pr["user"]["login"], activity)


def load_issue(number: int) -> Issue:
    raw = gh_json(f"repos/{REPO}/issues/{number}", paginate=False)
    comments = [Comment(c["user"]["login"], c["author_association"], c["body"] or "", ts(c["created_at"]))
                for c in gh_json(f"repos/{REPO}/issues/{number}/comments?per_page=100")]
    prs = []
    for ev in gh_json(f"repos/{REPO}/issues/{number}/timeline?per_page=100"):
        src = (ev.get("source") or {}).get("issue") or {}
        if ev.get("event") == "cross-referenced" and src.get("pull_request") and src.get("state") == "open" \
                and src.get("repository", {}).get("full_name", REPO) == REPO:
            prs.append(load_pr(src["number"]))
    return Issue(number, {l["name"] for l in raw["labels"]}, [a["login"] for a in raw["assignees"]], comments, prs)


def claimed_issue_numbers() -> list[int]:
    items = gh_json(f"repos/{REPO}/issues?state=open&labels={CLAIMED}&per_page=100")
    return [i["number"] for i in items if "pull_request" not in i]


def apply(number: int, actions: list[tuple], dry_run: bool) -> None:
    for kind, arg in actions:
        print(f"#{number}: {kind} {arg if kind != 'comment' else arg[:80]!r}")
        if dry_run:
            continue
        base = ["gh", "issue", "edit", str(number), "--repo", REPO]
        cmd = {
            "label_add": base + ["--add-label", arg],
            "label_remove": base + ["--remove-label", arg],
            "assign": base + ["--add-assignee", arg],
            "unassign": base + ["--remove-assignee", arg],
            "comment": ["gh", "issue", "comment", str(number), "--repo", REPO, "--body", arg],
        }[kind]
        # Assigning someone GitHub won't accept must not stop the rest; the
        # label and the bot's comment are the record of the claim.
        subprocess.run(cmd, check=kind not in ("assign", "unassign"))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["comment", "sweep"])
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    now = datetime.now(timezone.utc)

    if args.mode == "comment":
        with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as f:
            event = json.load(f)
        if "pull_request" in event["issue"]:
            return 0
        c = event["comment"]
        comment = Comment(c["user"]["login"], c["author_association"], c["body"] or "", ts(c["created_at"]))
        line = comment.first_line()
        if is_bot(comment.author) or not any(r.match(line) for r in (CLAIM_RE, RELEASE_RE, TAKEOVER_RE, REASSIGN_RE)):
            return 0
        number = event["issue"]["number"]
        issue = load_issue(number)
        # The new comment is in the issue's history; decide as of before it.
        issue.comments = [x for x in issue.comments if x.created < comment.created]
        apply(number, decide_comment(issue, comment, now), args.dry_run)
        return 0

    for number in claimed_issue_numbers():
        apply(number, decide_sweep(load_issue(number), now), args.dry_run)
    return 0


if __name__ == "__main__":
    sys.exit(main())
