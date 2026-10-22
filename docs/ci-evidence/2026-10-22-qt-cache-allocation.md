# Qt allowance after observed pruning and warm reuse (#1269)

The [hosted chain](2026-10-22-qt-cache-used.md) observes a published
507,448,340-byte archive and a later successful consumer with 306/310 hits,
five facade tests and 19 CTest cases passed. The consumer's post-cleanup
working store is 508,489,050 bytes. The old ~1.2 GB stores contained unused
objects; the successful pruned consumer supplies the replacement baseline.

The candidate sets ccache's working limit to **650M** (650,000,000 bytes) and
its retention family to **750MB** (786,432,000 bytes under pinned ci-lint's
binary parser). This leaves 27.83% working headroom over the observed store;
the retention allowance exceeds the working limit. It does not reduce
assertions, build targets, Qt tests, cache correctness checks or publication
trust guards. Hosted qualification at the candidate limit remains required.

The [saved inventory/configurations](2026-10-22-qt-cache-allocation.json)
reproduce the pinned janitor before/after the retention change, using
`2026-10-21-cache-retention-audit.py --replay`. Neither capture-time nor the
+601-second hypothetical plan selects additional deletion IDs. The snapshot
contains two Qt generations; existing supersession cleanup already selects
the older generation. No remote delete callback was provided.

Declared fleet reservations fall from **14,688,662,322** to **14,079,229,951**
bytes, a **609,432,371-byte** reduction. This releases declared reservation,
not measured live storage. The resulting allowance is still **3,341,811,711
bytes above 10 GiB**. Global allocation, native architecture retention and
representative-day durability remain unresolved; this is one bounded unit.

RED commit `0d4e4c461` fails the old 1200M limit. Focused checks pass with the
candidate. The test also fixes MB/GB interpretation to agree with ci-lint;
ccache M remains decimal. No smaller native Windows allowance is adopted.

Reproduce both offline plans from the committed capture:

```sh
qt_capture_root=$(safe-mktemp /tmp/obs-qt-allocation-replay)
uv run --no-project - "$qt_capture_root" <<'PY'
import json
import sys
from pathlib import Path
record = json.loads(Path('docs/ci-evidence/2026-10-22-qt-cache-allocation.json').read_text())
for variant in ('baseline', 'candidate'):
    directory = Path(sys.argv[1]) / variant
    directory.mkdir()
    (directory / 'inventory.json').write_text(json.dumps(record['capture']))
    (directory / 'ci.toml').write_text(record[variant + '_ci_toml'])
PY
for variant in baseline candidate; do
  uv run --no-project docs/ci-evidence/2026-10-21-cache-retention-audit.py --output "$qt_capture_root/$variant" --replay
done
safe-rm -r "$qt_capture_root"
```

The audit's dependency pin supplies the size parser and janitor. Replay does
not call authenticated GitHub APIs or supply a delete callback.
