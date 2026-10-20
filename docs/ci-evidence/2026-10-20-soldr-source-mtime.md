# Soldr source timestamp replay (#1359)

Adopt [Soldr 0.9.38](https://github.com/zackees/soldr/releases/tag/v0.9.38),
commit `7b5047c20733708693dc76468539f51a098118ac`. GitHub's compare API
confirms it contains the timestamp fix merged as
[`141974ee`](https://github.com/zackees/soldr/pull/3852).
The published Linux archive's SHA256 is
`137b722f760a546f849aa9e709564c8675b12499b0783b63ef18c4403d62225e`;
the CMake download table uses the six published host checksums. The release
still supports Rust 1.98.1, so the Rust toolchain pin does not change.

## Actual obs-util full and delta replay

The manual probe builds the real `obs-util` category test executable with
`SOLDR_RUSTC_WRAPPER=none`, regenerates full and delta archives, renames the
producer aside, and restores into the identical target path. A marker added
after the base save confirms the delta payload was applied. Source bytes are
unchanged; original tracked-file timestamps are restored in teardown.

`rust/obs-util/src/platform/windows/host.rs` is assigned the original failing
nanosecond timestamp before compilation. The consumer command is
`CARGO_LOG=cargo::core::compiler::fingerprint=info soldr cargo test -p obs-util --locked --no-run -vv`.

| Version | Original ns | Full restore ns | Base plus delta ns | Both consumers |
| --- | --- | --- | --- | --- |
| 0.9.35 (RED, exit 1) | 1792262706251336821 | 1792262706251000000 | 1792262706251000000 | Fresh |
| 0.9.38 (GREEN, exit 0) | 1792262706251336821 | 1792262706251336821 | 1792262706251336821 | Fresh |

Neither current-tree consumer logs `PrecalculatedComponentsChanged`. The
existing `obs-util/build.rs` input mitigation already prevents the old
package-wide rebuild; RED here proves the remaining precision loss. The
[earlier pre-mitigation failure](../ci-cache-audit-2026-10-17.md#build-script-inputs-can-defeat-deep-reuse)
and its exact-mtime control remain the evidence for that original rebuild.
This result makes no CI speedup claim.

Regenerated 0.9.38 full and delta archives are 119,910,945 and 12,328,484 bytes.
The [JSON evidence](2026-10-20-soldr-source-mtime.json) records their hashes,
source identity, commands, return codes, precision, and individual timings.
Raw logs and archives remain under `build_ci_audit/soldr1359/` locally.

Reproduce using fresh directory names and the published binaries:

```sh
uv run ci/soldr_source_mtime_probe.py --directory build_ci_audit/mtime-old-new-dir --soldr /absolute/path/to/old/soldr
uv run ci/soldr_source_mtime_probe.py --directory build_ci_audit/mtime-fixed-new-dir --soldr /absolute/path/to/fixed/soldr
uv run ci/deep_test_cache_probe.py --directory build_ci_audit/content-fixed-new-dir --soldr /absolute/path/to/fixed/soldr --verify-guard
```

## Cache migration and correctness limits

Pinned setup-soldr `b782f606` still defaults to Soldr 0.9.33. All seven
consumers therefore explicitly select 0.9.38. Its `src/lib/cook-cache.ts`
includes the resolved Soldr version in the full/base key and delta restore
prefix; base restoration uses an exact key with no fallback. Changing the
version creates a new namespace without broad cache deletion. Master-only
publication, profile flags, and host/container suffixes stay in place. Local
full/delta test archives were regenerated above; remote seeds are published
by the existing master producers after landing.

The independent same-size/same-mtime probe passes `--verify-guard`: ordinary
edits fail, targeted fingerprint invalidation rejects the same-stat edit,
and unchanged dependencies remain reusable. **Without the guard, stale unit
tests still succeed.** Thus `SOLDR_RUSTC_WRAPPER=none`, `build-cache: false`,
and the existing invalidation policy remain necessary. Timestamp fidelity
does not prove compiler-cache correctness or safety of arbitrary deep reuse.

An additional 0.9.35 manifest-only fixture loaded by 0.9.38 emits
`archive contains legacy millisecond source mtimes; submillisecond precision cannot be recovered`.
It restores the rounded millisecond value, confirming backward compatibility
without recovering missing digits. Keep that warning visible, regenerate
with the fixed producer, and retain the existing
`obs-util` input workaround. This Linux filesystem supports nanoseconds;
no equivalent precision result is claimed for a filesystem that cannot store
them. The probe refuses a filesystem that rounds the requested timestamp to
milliseconds. No Cargo freshness checks are disabled.
