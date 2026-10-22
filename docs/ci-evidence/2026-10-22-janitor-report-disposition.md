# Janitor incident report: acceptance and continuing work (#1269)

This audit maps the original report's acceptance criteria to retained evidence.
It does not certify the broader CI performance backlog as complete. Global
capacity and useful-seed durability remain open in the verified native child
[#1801](https://github.com/obs-rust/obs-studio/issues/1801); native cache/PCH work
remains with #782, consumer correctness with #1286, Flatpak measurements with
#1760 and queue latency/admission with #930 and #1793.

## Original acceptance audit

| Requirement from #1269 | Evidence | Disposition |
| --- | --- | --- |
| RED → GREEN guards exclude untrusted janitor events and remove build/cancellation prerequisites | Merged [#1270](https://github.com/obs-rust/obs-studio/pull/1270): RED `5f4eefe46`; GREEN `9e3c03e4e`, refined `170469463`; 16 focused checks recorded. Current policy subset has 15 passing tests. | Verified |
| Actual fork PR has no failed janitor | PR #1698's head repository is `rdoupe/obs-studio`. [Run 39711017914](https://github.com/obs-rust/obs-studio/actions/runs/39711017914) skipped janitor `119602108833`; its gate passed. | Verified |
| Trusted merge group runs/safely skips janitor without holding the gate | [Run 39715838880](https://github.com/obs-rust/obs-studio/actions/runs/39715838880): janitor `119617609226` and gate `119618547655` succeeded. Current tests verify dependency edges and gate exclusion; timestamps alone do not prove graph independence. | Verified |
| Preserve OFF/ON coverage, add no workflow file or broad unmeasured deletion | #1270 edited three existing workflows and one guard test. Latest observed OFF/ON case sets match: 361 passed plus four explicit Pulse skips in each mode. Native compatibility and protocol results are retained separately, including failures/cancellations. | Verified for the incident fix; no blanket native qualification claim |
| Link follow-up measurements and unresolved items before completing the report | Merged day/cohort and native reports below; actionable owning issues in the continuing-work table. | Linked; engineering requirements remain open |

Current cheap policy verification:

```sh
uv run --no-project -m unittest ci.test_ci_pre ci.test_merge_queue ci.test_scheduled_cache_policy
```

Observed: **15 passed**. This does not run a compiler or add a CI lane.

## Findings and strategy: disposition

| Original finding | Implemented or measured result | Remaining owner / next step |
| --- | --- | --- |
| Janitor permissions and dependency serialization | #1270 isolation and #1272 documentation; actual fork and queue evidence above | Keep policy guards; admission gap #1793 is separate from janitor exclusion |
| Independent scheduled cleanup | Merged #1289 removed the independent purge and repaired startup; current scheduled-policy tests pass | #1801 reconciles later whole-fleet capacity and actual deletion provenance |
| Tier 2 test execution remains critical | Scheduling/sharding and execution reports retained under #930/#1581/#1694; latest OFF/ON counts below | #930 owns code-only gate target and remaining runtime work |
| Missing touched-target warming and empty timing data | #1313/#1322 and follow-ups produced real warm target/apt use; #1288/#1311 repaired timing production and later consumption. Detailed run links are in #1286's completed checklist items. | #1801 owns durability after writer bursts; #1286 retains consumer measurement scope |
| Qt, dependency restore overhead and off-master writes | Measured Qt pruning/allocation reports; master-only setup-soldr and setup-uv remote publication; rejected Windows 350M experiment | #1801 requires an honest allocation and explicit #1723 disposition; #782 owns native reuse/PCH; #1286 owns cook/profile work |
| Representative traffic and cost observation | [24-hour cohort](2026-10-22-ci-cache-day-cohort.md), 240 PR / 139 merge-group runs, exact API records and offline reproduction | #930/#1286 retain code-only matched analysis, actual consumer durability and cost limits |

The mixed day cohort reports gate timing, failures/cancellations and incomplete
job/step timing proxies separately by event. It does not prove the code-only
eight-minute target, causal gains or metered billing. These missing conclusions
are explicit remaining work, not inferred successes.

## Latest hosted observation: partial success, cancelled overall

[Run 39810319992](https://github.com/obs-rust/obs-studio/actions/runs/39810319992)
on `2bc7b591ee3c5375547f9410b586e20237465ac6` concluded **cancelled**:
34 job entries comprise **15 success / 8 skipped / 10 cancelled / 1 failed gate**.
Windows x64's annotation explicitly attributes cancellation to a higher-priority
waiting request for `Pull @ refs/heads/master`. A newer same-SHA dispatch,
39812201578, was created at 16:13:50 UTC. No surviving work was cancelled by this
report's author, and no complete platform validation is claimed.

Known non-skipped job wall time sums to **6727 seconds**. One cancelled routing
entry (`119941576674`) has invalid/missing duration and is excluded. This is an
incomplete API timing proxy, not billed runner time. All outcomes remain in the
companion; cancelled native validation is not relabeled as passed or skipped.

| Completed job | Actual observation |
| --- | --- |
| Qt | Mirror helper ran; apt step 71 s, 115 MB package fetch in 20 s. 19/19 CTest cases passed in 5.25 s. Compiler cache missed: 0/309 cacheable hits; new seed saved. |
| Ubuntu app | Mirror helper ran; 135 MB package fetch in 7 s. Installed app passed 7/7 GPU/library and 206/206 WebSocket checks. |
| Linux OFF/ON | 365 registered each: 361 passed, four Pulse skips. 187.41/189.03 s; all completion records reconcile and case-name sets match. |
| Windows ARM64 | 259/261 cacheable hits; 163 uncacheable calls. App build passed and new master seed saved. |
| macOS ARM64 | Restored 660,223,910 B Actions archive; 648/660 cacheable tasks hit. Build passed and new master seed saved. |
| macOS x86_64 | Cache restore missed. App build passed and new seed saved; compiler hit count is not inferred. |
| Flatpak | New projected-state key missed and used legacy fallbacks. All six dependency modules hit, but compiler was cold: 0/331 hits. Successful trusted publication created cache `9351848007`, 2,266,246,245 B. |

These are individual observations with different sources/cache conditions.
The Qt/Ubuntu package results do not establish causal mirror speedup. Flatpak's
first projected publication fits its existing 2.5 GiB allowance but does not
establish later warm reuse, durable retention or its performance target.

## Current capacity and unresolved deletion actors

The pinned read-only audit at 16:16:44–45 UTC measured **13,542,359,039 B**
declared against **10,737,418,240 B** budget: excess **2,804,940,799 B**.
Paginated inventory was 10,364,929,123 B; the separate usage endpoint reported
11,125,448,570 B. These calls are non-atomic; neither number proves stable
headroom, a causal eviction mechanism or billed storage.

Native architecture reservations in held draft #1723 would exceed budget by
5,696,493,976 B. No adoption or capacity increase is authorized by this report.
#1801 requires an explicit within-budget allocation or measured rejection with
owning follow-ups, and retains the newest-seed/grace/correctness safeguards.

Known deletion actor: #1795's queue janitor `119888947766` deleted Windows x64
transport seed `9339088459` under the pooled 800 MiB allowance. Prior ARM64
losses are retained in the [native build-reuse report](2026-10-22-native-cache-build-reuse.md).
Qt seed `9344459457` was explicitly retained by master janitor `119885378601`
at 14:36:12 UTC, but absent from a later inventory and the actual Qt lookup.
The inspected queue janitors do not identify its removal. Qt and the earlier
Flatpak disappearance actors remain **unknown**.

## Evidence integrity and reproduction

The companion holds **39 exact saved records**, including the pinned audit's
configuration/input/report, source-case records, run/job metadata, cancellation
annotations and historical acceptance verification, plus **18 raw-log hashes
with numbered excerpts**. Saved API collector bytes are not HTTP wire-byte
claims. Full binaries, recordings and compiler-cache payloads are not embedded.

Companion SHA256:
`bc5b072d9ab3995e1ea0e02ddac5c0654f0380e7a224753d2c5b27a09136a333`.

Validate saved records and recompute outcomes without network or dependencies:

```sh
uv run --no-project - <<'PY'
import base64, collections, gzip, hashlib, json
from pathlib import Path
p = Path('docs/ci-evidence/2026-10-22-janitor-report-disposition.json.gz')
x = json.loads(gzip.decompress(p.read_bytes()))
records = {}
for item in x['records']:
    raw = base64.b64decode(item['base64'], validate=True)
    assert len(raw) == item['bytes']
    assert hashlib.sha256(raw).hexdigest() == item['sha256']
    if item['kind'] == 'json':
        records[item['path']] = json.loads(raw)
jobs = [j for page in records['master-observation-final-jobs.json'] for j in page['jobs']]
assert dict(collections.Counter(j['conclusion'] for j in jobs)) == x['summary']['outcomes']
print(len(x['records']), 'records verified;', x['summary'])
PY
```

The earlier [native failed qualification](2026-10-22-native-cache-build-reuse.md)
remains failed: strict Windows profiles were cold and macOS installed-binary
freshness was unproven. #1792 repaired the harness path; no retroactive pass or
new full retry follows from this report. Completing the original janitor
incident report leaves #1801, #782, #1286, #1760 and #930 actionable, and #1667
open until its entire tracked backlog is completed or explicitly dispositioned.
