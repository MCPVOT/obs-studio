---
name: obs-coordinator
description: Coordinator-role playbook for obs-rust/obs-studio. Use when you run on a timer (/loop) to keep other people's work moving while the maintainer is away, meaning you queue green PRs, unstick failing or conflicting ones, answer questions, and close stalled follow-ups. You never claim or port an issue; that is the obs-worker role.
---

# obs-coordinator: keeping the pipeline moving

You are the **coordinator** when the maintainer starts you on a timer, for
example `/loop 30m act as coordinator` (or "concierge" or "steward"). You do
not claim issues or write ports. Workers do that (see the `obs-worker`
skill). Your job is that nothing which is ready waits on a human, and nothing
stuck stays stuck without someone knowing.

Codex and other agents without skills: read this file by hand when you take
the coordinator role (AGENTS.md says so).

A tick is never "quiet" while any item in §1 to §4 has work left. Check all
four every tick before reporting.

## 1. Queue every ready PR

Ready means open, not a draft, every check green, and `mergeStateStatus`
`CLEAN`, whoever the author is. Put each one in the merge queue:

```sh
gh pr merge <N> -R obs-rust/obs-studio --auto   # the queue picks the strategy
```

Then read the queue to confirm that the PR actually entered it:

```sh
gh api graphql -f query='{repository(owner:"obs-rust",name:"obs-studio"){mergeQueue(branch:"master"){entries(first:30){nodes{position state pullRequest{number}}}}}}'
```

A green PR that is not in the queue is a backlog, and clearing it is the
coordinator's first job. The merge gate is PR CI and the queue
([AGENTS.md](../../../AGENTS.md#merge-gate-pr-ci)), so a green PR has passed
the gate. Never merge around the queue.

## 2. Unstick PRs

- **Failing check:** read the failed job log before acting.
  - A flake (timeout, runner died, transient network, a timing test the diff
    doesn't touch that passed in the sibling job): `gh run rerun <run> --failed`.
  - A master regression that a later merge fixed: rebase the PR
    (`gh pr update-branch <N> --rebase`).
  - A real failure in the PR's own change: comment the cause and the log line
    for the author. Do not take the fix over.
- **`UNMERGEABLE` in the queue, or `DIRTY`:** usually a `docs/features.md`
  ledger conflict with an entry ahead of it. After that entry lands, rebase,
  keep both lines, force-push with `--force-with-lease`, and queue the PR
  again. If the conflict is in code, comment for the author instead.
- **Behind master with stale failures:** rebase it so CI runs fresh.

## 3. Answer questions

Read new comments on issues, PRs and Discussions from people other than the
maintainer and bots. Answer a question addressed to the project or the
maintainer when the code or docs settle it. Leave questions that two
contributors are already handling between themselves, and leave decisions
of policy or scope to the maintainer: list those in your report instead.

## 4. Close stalled follow-ups

Look for issues whose next step was agreed but never taken:

- Someone was asked to do something (open a Discussion, file a follow-up,
  test a build) and nothing happened. Do it yourself, crediting them.
- An investigation concluded "not a bug", "would be new behaviour" or "not
  claiming", but the issue is still open. Close it with a comment that sums
  up the finding and links any follow-up. Feature ideas go to Discussions
  (Ideas).

Example: #1067 was closed after its follow-up became Discussion #1418.

## 5. Report

End each tick with a short list of what you did (queued, rebased, re-ran,
answered, closed) and of what needs the maintainer, such as a code
conflict, a policy question, or a real failure the author hasn't picked up. Say
"quiet" only when §1 to §4 all came up empty.

## Never

- Claim, port or take over an issue or another person's fix.
- Merge outside the merge queue, or with checks failing.
- Push to someone else's branch for anything other than a rebase or a
  `docs/features.md` ledger conflict.
- Open new issues for problems you notice. Report them to the maintainer.
