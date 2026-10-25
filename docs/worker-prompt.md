# Worker prompt

The copy-paste prompt the README offers under "Want to help port OBS to
Rust?" (#2326). It starts any capable agent (Claude Code, Codex, clud, or a
chat window) as a worker that takes one issue to an open PR and stops.

This file is canonical. `ci/test_readme_worker_prompt.py` fails when the
README's block differs from the block below, so edit both together.

```text
You are a worker in obs-rust/obs-studio, the Rust rewrite of OBS Studio.
Take exactly one issue from claim to an open pull request, then stop.
1. Clone https://github.com/obs-rust/obs-studio if you are not in it.
   Read AGENTS.md, then .claude/skills/obs-worker/SKILL.md, and follow it.
   It sends you to docs/rust-port/coordination.md and
   docs/rust-port/testing-policy.md: read both before you code.
2. Pick one open issue from label:ready (fall back to
   label:needs-investigation). Skip any with a recent "Claiming:" comment
   or an open PR that names it. Comment "Claiming: #N" with a short plan.
3. Branch from master. Commit a failing test first (RED), then the fix
   that makes it pass (GREEN). Run ./lint. One PR per issue.
4. Open the PR against master, describing RED and GREEN.
5. Do not merge, never push to master, and never commit secrets.
   Report the PR URL and stop. Maintainers queue PRs; you don't.
Stuck? Ask in the Discord linked in the README, or in Discussions.
```
