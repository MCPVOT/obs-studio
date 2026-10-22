# Qt cache: keep current-build objects and measure stale payload (#1269)

The completed Qt job 119563480332 in
[39699009288](https://github.com/obs-rust/obs-studio/actions/runs/39699009288/job/119563480332)
restored a 1,173,492,625-byte archive and reported 1,250 stored cache files after
310 cacheable calls (293 hits, 17 misses). This motivates measuring unused
payload; file count alone does not prove how many bytes can be reclaimed.

The proposed policy records an epoch before the build and evicts entries
unused since that time, with a 60-second margin, after the existing build/tests.
It then applies the unchanged 1200M limit. Before/after filesystem bytes are
logged. PRs operate on their runner-local copy and cannot publish remote
replacements. A missing epoch skips age eviction; failed jobs do not publish.
No build command, assertion, required lane, C test or public header changes.

Ccache 4.12.3, used by the hosted job, updates a retrieved local entry's mtime
before returning it ([pinned implementation](https://github.com/ccache/ccache/blob/v4.12.3/src/ccache/storage/local/localstorage.cpp#L506)).
The elapsed-time-plus-margin cutoff therefore retains entries accessed by the
current build. This is the same age-pruning strategy already used for the
Windows app cache; it does not impose the smaller working set rejected by the
[native experiment](2026-10-22-native-cache-budget.md).

## Local mechanism check

The [probe](2026-10-22-qt-cache-used-smoke.py) executes the actual preparation
and report scripts extracted from the workflow. It compiles two tiny C objects,
ages their entries by an hour, reuses only one during the simulated current
build, runs the report/pruning script, then compiles both again.

The [recorded result](2026-10-22-qt-cache-used-local.json), using local ccache
4.13.6 and GCC 15.2.0, retains a direct hit with zero misses for the used object;
the unused object becomes a miss. Two stale cache files are removed. Full
counter data and workflow-script hashes are preserved.

This is a mechanism check with two small objects and a different ccache version
from CI. It does not establish the hosted Qt working-set size, next consumer's
hit rate, archive size, CI speedup or durable retention. Hosted Qt validation
and actual publication/consumer observations remain required under #1269.

From the repository root, reproduce with installed ccache and a C compiler,
or use the Nix-provided ccache (the probe records the actual version):

```sh
probe_scratch=$(safe-mktemp /tmp/obs-qt-cache-used)
nix shell nixpkgs#ccache --command uv run --no-project docs/ci-evidence/2026-10-22-qt-cache-used-smoke.py --output "$probe_scratch/probe"
safe-rm -r "$probe_scratch"
```

The probe requires a new output directory and keeps all fixture data inside
it. It does not alter the developer's existing compiler cache. No smaller
production cache allowance or global-budget completion is claimed.

## Hosted lane selection correction

The first rebased PR run, 39704674207, skipped Qt: its scope table included
`ci/test_qt_fastpath.py` but omitted the cache budget and identity contract tests.
That run supplies no hosted pruning evidence. A separate local RED commit
2889dce5e fails for both omitted paths. The scope correction selects Qt for
`ci/test_qt_cache_budget.py` and `ci/test_qt_cache_identity.py`; all 40 scope tests
and the 28 Qt/configuration tests then pass. Other lane selection stays intact.

## Hosted PR and merge-group qualification

#1690 merged as `224602f79e0afd027c274dfe641051b8d68e5401` after required
PR run 39705091053 and merge-group run 39705788492 passed. The [hosted
record](2026-10-22-qt-cache-used-hosted.json) preserves measurement excerpts.

| Observation | Local bytes before | Local bytes after | Hits / cacheable calls |
| --- | ---: | ---: | ---: |
| PR Qt job 119583323367 | 1,196,582,208 | 507,503,415 | 303 / 310 |
| Queue Qt job 119585476202 | 1,196,582,200 | 507,503,407 | 303 / 310 |

Both hosted ccache 4.12.3 jobs passed five facade tests and all 19 CTest cases.
Both removed 689,078,793 local bytes (57.59%), ending with 615 stored files.
These related source observations are not independent randomized trials.
The hit counts were measured during builds before pruning; they do not
establish the next consumer's hit rate. Neither event publishes a remote
replacement. Master archive publication is recorded below; subsequent reuse is recorded below.

The [rollout-day baseline](2026-10-21-rollout-day.md) separately reports code
PR and queue timing/cost samples; it precedes this pruning publication and
must not be used as a post-change speedup claim.

## Master publication

Master Qt job [119594181344](https://github.com/obs-rust/obs-studio/actions/runs/39707134569/job/119594181344)
passed on merged source `224602f79`. It restored a 1,187,113,796-byte archive,
then reported 310 cacheable calls (297 hits, 13 misses), five facade tests
passed and all 19 CTest cases passed. Local bytes fell from 1,197,203,950 to
507,500,741 after pruning, ending with 616 stored files.

The job saved master cache **9314403583**, key
`qt-ccache-linux-inputs-v2-7021a2ec3e14af5bf46e4fa736dbce3d876a44a8f2325d8cec330852ec663168`.
The API inventory reports **507,448,340 compressed archive bytes**, created
2026-10-22T01:59:55.925949Z: 679,665,456 fewer than the archive restored by
this producer. This is an observed replacement size, not a causal speedup
or proof of future retention. The parent run was still in progress when
these completed-job logs were captured. The later real warm consumer is recorded below.

## Observed cleanup and later warm consumer

Janitor job [119598202010](https://github.com/obs-rust/obs-studio/actions/runs/39709813400/job/119598202010)
explicitly deleted superseded cache 9313657491 at 02:09:41Z, retaining cache
9314403583. Its Qt family row fell from two entries / 1,694,562,136 bytes to
one / 507,448,340 bytes. This identifies the actual deletion actor; no manual
cache deletion was performed. A following API inventory contained only the
new seed. The producer's full run 39707134569 subsequently passed.

The later master consumer on source `4f28621ff`, Qt job
[119599861846](https://github.com/obs-rust/obs-studio/actions/runs/39710022658/job/119599861846),
restored the exact 507,448,340-byte published archive. It passed five facade
tests and all 19 CTest cases, with **306 hits / 310 cacheable calls (98.71%)**
and four misses. Local bytes were 509,142,950 before cleanup and 508,489,050
afterward, with 620 stored files. It published a new source-identity key
`qt-ccache-linux-inputs-v2-503f01dfd2251754a7c94c53d8717b3220ae65266ea98d8a85e8ebcdab1fff7e`.

This qualifies actual reuse of the pruned archive after observed janitor
cleanup, across one later source revision. It is not a matched timing
experiment, an independent population sample, representative-day durability,
or proof that the later parent workflow passed. Global allocation and native
architecture retention remain unresolved under #1269.
