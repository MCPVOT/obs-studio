# Retire obsolete Flatpak cache migration paths (#1801)

## Evidence and decision

Projected dependency OSTree and compiler state now share one Actions archive.
The [completed Flatpak study](../ci-speed-flatpak-retrospective-1760.md)
retains subsequent production consumers, new source generations, three final
warm whole jobs (437/434/447 s), and an explicit 846 s cold build with cache
restoration and publication disabled. It also retains each workflow's independent
failures; the successful Flatpak jobs are not whole-workflow success claims.

The current read-only audit on `1751566fe03d314a0368bc3cb34440a47881657f`
contains one projected archive and **zero standalone `flatpak-ccache-` entries**.
All owned workflows already stopped writing the standalone family. Its remaining
use was a migration-only overlay after older whole-builder restores, attempted
only when the projected restore missed. The legacy namespace bridge is no longer
needed for a correct cold build or the observed projected consumers.

Remove those three legacy restore steps and the standalone family's **200 MiB
(209,715,200 B)** declaration. Keep the projected archive's two directories,
architecture/manifest/source keys and fallback prefixes, explicit cold guard,
counter reset, SDK setup, full build/export, all validators, post-build pruning
and master-only publication. A projected miss now follows the existing correct
cold path. No new publisher, test lane, compiler invocation or workflow is added.
The pinned builder's internal restore/save remain disabled.

## Capacity effect and limits

| Measure | Baseline | Candidate |
| --- | ---: | ---: |
| Declared family reservations | 13,542,359,039 B | 13,332,643,839 B |
| Configured global budget | 10,737,418,240 B | 10,737,418,240 B |
| Declared overcommit | 2,804,940,799 B | 2,595,225,599 B |
| Listed cache bytes in saved snapshot | 9,171,584,364 B | Same input |
| Standalone compiler entries | 0 | Same input |

This releases declared reservation; it reclaims **no observed live bytes** in
this snapshot. It does not establish fleet-wide headroom or resolve native
architecture reservations. #1801 remains open for all writers, overlapping
publications, growth, actual retention and #1723's explicit disposition.
The held native candidate remains unadopted.

The exact pinned executor returns identical empty deletion plans for both
configurations, at capture time and at a hypothetical +601 seconds. No live
cache is deleted and no delete callback is supplied. Useful projected, native,
queue compiler and apt entries in this input remain unselected. A future
standalone legacy entry would become undeclared; there is no remaining owned
reader/writer for it. Unknown historical deletion actors remain unknown.

No end-to-end speedup is claimed. Current warm jobs already skipped these
migration restores; savings would depend on a projected miss. This change
retires an obsolete compatibility bridge and reservation after replacement and
cold-build evidence, without reducing required application/test execution.

## RED → GREEN

RED `228d7b6cb` executes six focused configuration checks and fails four
assertions: legacy overlays, a standalone reader, its reservation and multiple
production build-cache restores remain. The new restore-count assertion is
scoped to the production job, excluding the independent diagnostic consumer.

GREEN removes the obsolete paths/family. All **53** CI contract and hybrid
packaging checks pass. Guards retain one projected restore before counter reset,
its exact two directories and cold/publication/trust conditions; no C test or
public header is changed. Arithmetic/retirement verification uses cheap existing
CI Python discovery, not another Flatpak or native build.

## Reproduce the saved policy replay

The compressed companion contains **eight exact records**: baseline/candidate
configurations, shared-inventory copies and executor reports, plus workflow
before/after text. Each record retains byte length, SHA256 and base64 bytes.
The inventory describes capture time, not a guarantee of future retention.

Companion SHA256: `3294f8e6c2de1dd989a51a2309b7c1a3606503eebb3ffc7f51dd871d9f16e48b`.

```sh
replay_root=$(safe-mktemp /tmp/obs-1801-legacy-replay)
uv run --no-project - "$replay_root" <<'PY'
import base64, gzip, hashlib, json, sys
from pathlib import Path
bundle = Path('docs/ci-evidence/2026-10-22-flatpak-legacy-retirement.json.gz')
for item in json.loads(gzip.decompress(bundle.read_bytes()))['records']:
    raw = base64.b64decode(item['base64'], validate=True)
    assert len(raw) == item['bytes']
    assert hashlib.sha256(raw).hexdigest() == item['sha256']
    path = Path(sys.argv[1]) / item['path']
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(raw)
PY
for variant in baseline candidate; do
  uv run docs/ci-evidence/2026-10-21-cache-retention-audit.py --output "$replay_root/$variant" --replay
done
safe-rm -r "$replay_root"
```

The existing audit script uses the pinned ci-lint size parser, classifier and
janitor. Its saved-cache adapter rejects other API paths and supplies no remote
delete operation. Preserve the full report's warnings and hypothetical-plan
labels when interpreting the result.
