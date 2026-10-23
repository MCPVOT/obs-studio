# Zackees draft PR handoff — OBS repository only

Tracking issue: [#1845](https://github.com/obs-rust/obs-studio/issues/1845).
Snapshot: **2026-10-22 23:32 UTC**, master `582a4c3ed`.

The maintainer requested a handoff of Zackees's draft PRs in
`obs-rust/obs-studio`. Work is limited to this repository. Do not resume earlier
FastLED, fbuild, template, toolchain or other cross-repository tasks. This
handoff does not transfer another worker's active implementation claim.

## Current drafts

### #1723 — independent native architecture cache retention

- [PR](https://github.com/obs-rust/obs-studio/pull/1723), branch
  `infra/1269-native-architecture-retention`.
- Verified head: `1f7f7944854e35114fdf3b94c4c80bd8e48fdaed`.
- Tracks [#1801](https://github.com/obs-rust/obs-studio/issues/1801) and
  [#782](https://github.com/obs-rust/obs-studio/issues/782), under #1667.
  The original #1269 is closed.
- **Hold: revise; do not merge unchanged.** Splitting Windows/macOS architecture
  families prevents one architecture replacing another's seed, but does not
  supply global capacity. Preserve the recorded deletion/replay evidence.
- The candidate's original arithmetic was 16,433,912,216 declared bytes against
  10,737,418,240 bytes. Its body also records conditional later reservation
  retirement. These are historical configuration calculations; recompute against
  the actual current configuration before proposing adoption.
- Next: reconcile all live writers, architecture variants, growth, overlap and
  qualified transport under #1801. Do not shrink working compiler caches merely
  to fit declarations, raise the budget or adopt unqualified release transport.

### #1838 — CI performance handoff and native measurement scope

- [PR](https://github.com/obs-rust/obs-studio/pull/1838), branch
  `infra/1801-native-measurement-scope`.
- Verified head: `72131b3a519b725d1a7a5d184611b3183ac387ac`.
- **Maintainer-paused: do not queue or automatically launch experiments.**
- Its detailed [handoff document at the captured head](https://github.com/obs-rust/obs-studio/blob/72131b3a519b725d1a7a5d184611b3183ac387ac/docs/ci-evidence/2026-10-22-ci-performance-handoff.md)
  and compressed companion retain evidence, hashes, limitations and commands.
  Use those rather than reconstructing experiments from this summary.
- Scope is default-off routing for a future native-cache measurement. Its
  reported local checks passed; the latest check query found no pending/failed
  checks. That does not remove the explicit maintainer hold or qualify a backend.
- Remaining subjects include capacity/version retention, AppImage publication
  and freshness, strict native compiler/SDK matching, and full measurement costs.
  Preserve failures and distinguish transfer/cache hits from actual compiler reuse.

### #1844 — native C++ language standards in the shared test graph

- [PR](https://github.com/obs-rust/obs-studio/pull/1844), branch
  `infra/1332-native-language-standards`.
- Verified head: `6b6ed3a0db97c76fb8ba936ff6e7d97721361348`.
- Tracks [#1332](https://github.com/obs-rust/obs-studio/issues/1332).
- RED contract: mixed C/C++20 executable in the existing application test binary;
  preserve C compilation, require C++ `consteval`/`std::bit_width` and ISO mode,
  exercise both CMake and Cargo consumers, reject invalid standards.
- Reported local RED rejects the absent `cpp_standard` field. Initial RED commit
  `bafc7a9b3`; the captured head tightens the CMake extension-property assertion.
- [PR run 39871147510](https://github.com/obs-rust/obs-studio/actions/runs/39871147510)
  has failed Fast Gate and required CI Gate. Inspect actual logs and current
  implementation ownership before making changes; do not infer all failure
  details from the local RED description.
- Next implementation: carry typed C++ standards through the shared target and
  both consumers, then migrate applicable unchanged Qt-free C++ tests. Preserve
  all assertions, source/header contracts and the existing executable budget.
  CMake remains authoritative; this does not complete #1332.

## Completed items that must not be restarted

- [#1772](https://github.com/obs-rust/obs-studio/pull/1772) merged as
  `5208e989a3311f4cb295ff78e813af32465c4477` after refreshed native CI passed.
  Its earlier macOS media-seek failure was a baseline issue fixed by #1775.
- [#1805](https://github.com/obs-rust/obs-studio/pull/1805) is merged. Captured
  final head: `f6b0c25127aac5bc835cb0fd45fc71823d7b26e2`. Earlier running jobs and
  partial test-graph inventories are historical; inspect merged source for the
  current graph before continuing #1844.
- A duplicate local #1772 merge was never pushed: another worker published the
  identical tree first. The temporary worktree/branch was removed after equality
  verification. No unique implementation remains in that scratch checkout.

## Resume procedure

1. Read `AGENTS.md`, `.claude/skills/obs-worker/SKILL.md`, coordination and testing
   policy, `docs/ci-tooling.md`, and `BUILD.md` when native sources/builds apply.
   Read current issue claims and PR descriptions before choosing a unit.
2. Start clean and fetch/rebase onto `origin/master`. Use an owned branch; do not
   overwrite another worker's checkout or fresh claim.
3. Query the direct PR API as well as individual PR records. A previous draft
   search returned an empty list while direct records proved drafts still open.

   ```sh
   gh api 'repos/obs-rust/obs-studio/pulls?state=open&per_page=100' \
     --jq '[.[] | select(.draft and ((.user.login | ascii_downcase) == "zackees")) | {number,title,head:.head.sha}]'
   gh pr view 1844 --repo obs-rust/obs-studio --json body,headRefOid,statusCheckRollup
   gh pr view 1838 --repo obs-rust/obs-studio --json body,headRefOid,statusCheckRollup
   gh pr view 1723 --repo obs-rust/obs-studio --json body,headRefOid,statusCheckRollup
   ```

   Follow pagination if there are more than 100 open PRs. Revalidate each exact
   head and any newly opened drafts; this document is a snapshot, not live state.
4. For a failure, inspect the current run/job log first. Re-poll confirmed live
   handles; an observation timeout or inconsistent summary is not a terminal
   result. Do not restart unchanged workflows to seek a passing result.
5. Implement only the chosen authorized unit, RED then GREEN, with required
   native/differential/protocol coverage. Use `soldr cargo` and `uv run`/`uvx`.
   Do not edit existing C/C++ tests or public headers to accommodate a port.
6. Keep #1723/#1838 held until their documented conditions are resolved. Do not
   add workflows, dispatch expensive experiments, alter paid storage or bypass
   merge gates as part of this handoff. Queue ready changes normally after the
   actual required CI Gate passes.

## Handoff validation and limits

This change is documentation only. PR states, immutable heads, issue states and
check results above were queried directly during handoff. No application code,
test assertion, workflow, cache policy or external repository was changed.
No new build or expensive experiment was launched. This handoff does not claim
the migration, global capacity work or remaining drafts are complete.
