# Cache retention and writer audit (#1269)

This is one read-only snapshot, not a throughput or durability benchmark.
The exact configuration and raw paginated inventory are in
[the input bundle](2026-10-21-cache-retention-inputs.json); the pinned offline
executor's decisions are in [the report](2026-10-21-cache-retention-report.json).

## Current capacity

The pinned ci-lint parser interprets MB/GB as binary units. Using its own
`family_footprint` implementation, declared capacity is **16,363,867,340 bytes**
against **10,737,418,240 bytes** of configured budget. The inventory contains
**9,377,257,293 bytes**, leaving **1,360,160,947 bytes** against that budget.
The separate usage endpoint reports **9,361,747,575 bytes**. These calls are
non-atomic; neither discrepancy nor remaining space establishes an eviction
actor or stable headroom.

Windows app compiler state is 1,075,832,992 bytes versus its 838,860,800-byte
family allowance. Flatpak builder state is 2,747,460,995 bytes versus
2,684,354,560 bytes. Each is the newest base entry in its family, so the
executor protects it despite its size. Reducing family declarations cannot
by itself enforce a whole-repository capacity bound. The declared total is
not a prediction of simultaneous occupancy.

The touched-target compiler and apt families have no entry in this snapshot.
Absence alone does not identify why they disappeared or establish that their
producer/consumer is broken. The snapshot contains one Ubuntu mode archive;
it does not prove that both modes remain warm after nightly writes.

## Fix selected from the evidence

The exact pinned setup-uv action
[`bec219d24cd3e171d82865faccec33120bb574f4`](https://github.com/astral-sh/setup-uv/blob/bec219d24cd3e171d82865faccec33120bb574f4/action.yml)
defaults `save-cache` to `true`. The snapshot contains PR/merge-group uv
archives even though ci.toml permits only trusted master/nightly base writes.
Their refs are incompatible with reuse by master, and the janitor selects
them as CACHE-003 after grace. This is observed writer-policy churn.

All 29 setup-uv callsites now set
`save-cache: ${{ github.ref == 'refs/heads/master' }}`. Restores retain their
default behavior. The two existing Tier 2 container callsites retain
`enable-cache: false`. Local uv caches are unaffected by this remote-save input.
Tag/release and non-master dispatch refs cannot publish these base archives.

The offline plan at capture time selects 1,083 bytes. With only its clock
advanced 601 seconds, it selects **30,567,460 bytes**, all uv entries on
non-master refs. This is a hypothetical grace-expired plan, not an actual
deletion or an ongoing savings estimate. No delete callback was supplied,
and no live cache was deleted. Repeated future writes would change the plan.

The focused regression initially failed at all 29 callsites. It passes with
the explicit master-only input. The first test draft also prohibited the
two pre-existing disabled-cache settings; that overbroad assertion was
corrected to check restore disabling, preserving the existing environments.

## Allocation decision and remaining evidence

Do not inflate the global budget or delete protected useful seeds to make
the static total look compliant. First stop the demonstrated untrusted-ref
writer churn. Keep current compiler/apt/Qt/dependency retention and freshness
safeguards while evaluating a capacity allocation against actual consumers.
This PR makes no changes to family budgets or the janitor's safety rules.

#1269 remains open for a complete budget/writer-burst allocation, durable
queue-seed retention across nightly publication, and a representative day of
consumer/cost evidence. In particular, a 30.6 MB hypothetical cleanup cannot
resolve the 5.6 GB declared overcommit or ensure the missing target seeds.
#1286 owns the overlapping compiler reuse/freshness investigation; #1581
owns the separate scheduling/sharding rollout validation. No CI speedup or
under-eight-minute population result is claimed.

## Reproduce

Capture current read-only evidence from the repository root:

```sh
uv run docs/ci-evidence/2026-10-21-cache-retention-audit.py --output /path/to/capture
uv run docs/ci-evidence/2026-10-21-cache-retention-audit.py --output /path/to/capture --replay
```

For the published observation, extract `inventory.json` and `ci.toml` from
the input bundle into the capture directory and use `--replay`. That command
uses the saved configuration and snapshot; its executor fetch is an offline
cache-list adapter which rejects other API paths. Missing PR-state queries
retain uncertain state conservatively. The report retains executor warnings.
