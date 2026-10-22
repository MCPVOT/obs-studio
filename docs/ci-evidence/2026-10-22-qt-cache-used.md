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
