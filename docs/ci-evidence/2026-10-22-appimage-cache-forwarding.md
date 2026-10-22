# AppImage compiler cache forwarding (#1832, under #1801)

## Existing failure and bounded repair

Successful release run [39845783549](https://github.com/obs-rust/obs-studio/actions/runs/39845783549)
on `9f37f663bce528eeaf10314d7323b16675adf61c` includes AppImage build
[120058695191](https://github.com/obs-rust/obs-studio/actions/runs/39845783549/job/120058695191).
The native compiler cache restore missed at 20:05:55 UTC. At 20:16:06, status
reported **314 total compilations, zero cached, 314 cold, zero non-cacheable**.
Saving `.zccache` then failed at 20:16:07 with a path-validation warning because
the directory did not exist. The warning did not fail the build/workflow.

The separate cargo archive restored 31,088,475 inner archive bytes in the log
and later saved 31,091,655 Actions bytes. That hit is not native compiler reuse.
The existing Arch Linux, AlmaLinux 8, Ubuntu 22.04 and NixOS runtime jobs all
succeeded. These are pre-fix observations, not post-fix qualification.

The workflow sets `ZCCACHE_CACHE_DIR` to the workspace's `.zccache`, which its
Actions restore/save steps use. The container launcher mounts that workspace at
the same path, but its explicit environment-name filter omitted this variable.
Docker does not inherit arbitrary host variables. The compiler launcher, status
and stop commands therefore lacked the configured persistent directory.
This identifies a broken environment contract; it does not attribute every
historical miss, or all 314 cold calls, solely to this omission.

The repair forwards **only `ZCCACHE_CACHE_DIR`** through the existing `-e NAME`
mechanism. Other zccache settings remain filtered. Cache paths, keys, limits,
formats, build flags, trust, mounted roots and compiler correctness safeguards
are unchanged. No workflow, native/compiler job or test executable is added.
No product packaging contents or runtime checks are changed.

## Local RED → GREEN

RED `da8b8dc62` adds two cases to the existing AppImage Python module. They run
the actual shell launcher with a fake Docker executable that records NUL-separated
arguments, with paths containing spaces and literal shell metacharacters.
The configured-directory case fails because `ZCCACHE_CACHE_DIR` is missing from
explicit `-e` arguments; the existing forwarding/filter/mount/quoting case passes.
Two cases take 0.031 seconds in that observation. An initial fixture-only PATH
problem on the Nix host was corrected before this RED commit.

GREEN extends the environment-name filter. All **10 tests** in
`ci.test_appimage_build_script` pass (0.570 seconds). The test records argv; it
does not compile OBS, start Docker or claim to exercise a real daemon/cache.
The check explicitly rejects forwarding an unrelated zccache remote-storage
setting. Temporary fixture files are owned and removed by `TemporaryDirectory`.
Full `./lint` passed in 83.3 seconds; the sole primary review was clean and
verified all eight record hashes. No native/container build was run locally.

```sh
uv run --no-project -m unittest ci.test_appimage_build_script
```

## Qualification still required

#1832 stays open until the real trusted AppImage workflow publishes a nonempty
compiler archive at the configured directory, followed by an eligible consumer
with actual compiler hits, transfer/build costs and source-edit freshness.
Use existing normal traffic where possible. Do not launch an expensive native
experiment merely to test environment argument selection or policy arithmetic.

Preserve required packaged saved-profile/runtime checks and all selected failure,
cancellation and skip outcomes. A successful directory/save alone does not prove
reuse, correct invalidation or net speedup. This producer repair also does not
complete #1801's allocation: restored payloads and overlapping publications must
fit its unchanged global budget without deleting useful seeds or adopting paid
storage. No speedup, durable headroom or warm rollout is claimed here.

## Retained evidence

The compressed companion holds eight exact records: terminal release/build
metadata, the complete build log, local RED/GREEN outputs, launcher before/after
and test source. Each retains length, SHA256 and base64 bytes. Companion SHA256:
`ec4719cc7d93fdfcaa7288cfe7cd946a0e848f178305531eb372cc28addda841`.

Verify without executing builds or deleting live caches:

```sh
uv run --no-project - <<'PY'
import base64, gzip, hashlib, json
from pathlib import Path
bundle = Path('docs/ci-evidence/2026-10-22-appimage-cache-forwarding.json.gz')
for record in json.loads(gzip.decompress(bundle.read_bytes()))['records']:
    raw = base64.b64decode(record['base64'], validate=True)
    assert len(raw) == record['bytes']
    assert hashlib.sha256(raw).hexdigest() == record['sha256']
print('8 evidence records verified')
PY
```

Original hosted logs can expire; the complete retained build log distinguishes
inner payload sizes, Action archive sizes and compiler counters. Job walls are
not billed allocated-runner time, and this single source/runner observation is
not a representative performance comparison.
