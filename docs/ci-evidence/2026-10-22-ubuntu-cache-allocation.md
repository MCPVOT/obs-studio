# Ubuntu app reservation follows its current producer (#1269)

The existing Ubuntu application matrix is `rust: ['ON']`; it produces one
hybrid application. This change does not alter that matrix or any workflow.
Linux Tier 2's separate matrix remains `mode: ["OFF", "ON"]`, with the
existing C-reference/hybrid tests and differential proof untouched.

The [saved inventory and pinned replay](2026-10-22-ubuntu-cache-allocation.json)
contain one live master Ubuntu app archive, **905,383,834 bytes**, cache
9309124360, key
`zccache-ubuntu-ubuntu-26.04-x86_64-rustON-RelWithDebInfo-f1783cf62e4f9ad34ee7abe3d00fc351d2ed4921`.
The earlier two-app archive evidence is preserved in
`2026-10-18-ubuntu-retention-plan.json`; that OFF+ON producer assumption no
longer describes the current application matrix.

The candidate family allowance is **1.1GB** under pinned ci-lint's binary
parser: **1,181,116,006 bytes**, 30.45% above the observed archive. The existing
compiler working-store cap, mode/configuration restore boundaries, isolated
Ubuntu family and trusted publication guards remain unchanged. A family
reservation is not a working-store limit and does not prove future growth
will remain within it.

Both capture-time and +601-second hypothetical janitor plans retain the
observed ON seed and select exactly the same deletion IDs as the baseline.
No delete callback was supplied; this change does not manually delete any
cache or claim an observed performance gain. It does not introduce a new
compiler configuration, so a new full app dispatch is not needed to test a
build change; existing producer evidence and the actual pinned retention
algorithm are the relevant signals here.

Declared reservations fall from **14,079,229,951** to **13,542,359,039 bytes**,
releasing **536,870,912 bytes (512 MiB)** of unused declared allowance. This
is not measured live storage reclamation. The total remains **2,804,940,799
bytes above 10 GiB**; full allocation and native architecture retention remain
unresolved under #1269.

RED commit `8d006946e` fails the old 1.6GB allowance. The updated contract
requires at least 25% archive growth room, bounds the single-producer
reservation and explicitly checks the existing production and Tier 2 modes.
It preserves independent LRU protection from queue/AppImage/sanitizer stores.

## Replay

Extract the saved snapshot and both exact configurations, then run the
existing audit. Its dependency pin supplies the production janitor; the
replay never calls authenticated APIs or supplies a delete callback.

```sh
ubuntu_capture_root=$(safe-mktemp /tmp/obs-ubuntu-allocation-replay)
uv run --no-project - "$ubuntu_capture_root" <<'PY'
import json
import sys
from pathlib import Path
record = json.loads(Path('docs/ci-evidence/2026-10-22-ubuntu-cache-allocation.json').read_text())
for variant in ('baseline', 'candidate'):
    directory = Path(sys.argv[1]) / variant
    directory.mkdir()
    (directory / 'inventory.json').write_text(json.dumps(record['capture']))
    (directory / 'ci.toml').write_text(record[variant + '_ci_toml'])
PY
for variant in baseline candidate; do
  uv run --no-project docs/ci-evidence/2026-10-21-cache-retention-audit.py --output "$ubuntu_capture_root/$variant" --replay
done
safe-rm -r "$ubuntu_capture_root"
```
