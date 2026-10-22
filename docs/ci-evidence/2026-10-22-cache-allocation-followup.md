# Cache allocation and version retention follow-up (#1801)

## Decision and scope

After merged #1826, `ci.toml` declares **13,332,643,839 B** against the unchanged
**10,737,418,240 B** budget: **2,595,225,599 B overcommitted**. The retired
standalone Flatpak reservation accounts for 209,715,200 B of improvement in
declarations, with no observed live bytes reclaimed. Replacement and cold
qualification are in the [retirement report](2026-10-22-flatpak-legacy-retirement.md).

**Reject adopting #1723 unchanged; revise its allocation first.** With this
retirement, that candidate would still declare 16,224,197,016 B, exceeding the
budget by 5,486,778,776 B. Independent architecture retention fixes a real
selection defect but cannot create storage. The candidate remains draft and
unqueued. This does not prove every possible 10 GiB allocation impossible.

Keep useful seeds, identity checks, ten-minute grace and correct cold builds.
Smaller declarations cannot establish durability. The
[350M Windows trial](2026-10-22-native-cache-budget.md) nearly doubled rebuild
time; it remains rejected. Native release transport remains default-off pending
reuse/freshness/cost qualification, preserved in the
[native trial](2026-10-22-native-cache-build-reuse.md). Its failures remain
failures; later harness fixes do not retroactively qualify that trial.

**#1801 remains open.** Growth, transferred bytes, net consumer value and durable
retention are not fully measured for every family below. Owners identify where
to investigate, not completed handoffs. No new workflow, paid storage, budget
increase, manual deletion or hosted experiment was introduced.

## Inventory and writer map

The first inventory completed at **21:16:19 UTC**, source `1751566fe`, with
9,171,584,364 listed bytes. The second completed at **21:27:51 UTC**, with
10,259,430,637 listed bytes and **477,987,603 B listed headroom**. Each separate
usage endpoint agreed with its listing in these captures; calls remain
non-atomic. Neither is an observed peak, minimum working set or steady headroom.

The second capture used the then-pending #1826 configuration. The bundle also
replays that inventory against the exact earlier production configuration.
The registry selections below are identical. #1826 subsequently merged at
`233db443aa3f528b30da7fe2dea19fddabf52f6c`; its candidate `ci.toml` is
byte-identical to the merged configuration.

The table includes **all 25 remaining configured families**. Declarations use
the pinned parser's shape/platform cardinality. Listed sizes are compressed
Actions archives, not runner working stores or enforceable caps. Several
families already exceed their declaration; newest-entry protection and grace
mean declarations are not instantaneous occupancy limits.

| Family | Declared B | Listed B at 21:16 | Listed B at 21:27 | Writer / identity variants | Investigation owner |
| --- | ---: | ---: | ---: | --- | --- |
| `registry` | 188,743,680 | 33,490,683 | 82,421,146 | setup-soldr Cargo registry; host OS/architecture + lock/source identity | #1801 / upstream ci.yml#75 |
| `deps` | 2,604,243,353 | 1,877,377,611 | 1,877,377,611 | setup-soldr cook; host OS/architecture/libc + rustc + flags + lock + soldr + namespace; host/container differ | #1286 |
| `toolchain` | 597,688,320 | 474,649,985 | 474,649,985 | setup-soldr solo toolchain; host OS/architecture/libc + rustc + components/targets + soldr | #1286 |
| `soldr-mini` | 47,185,920 | 32,606,208 | 44,096,987 | setup-soldr binary; host OS/architecture/libc + soldr version; exact-only | #1801 / upstream ci.yml#75 |
| `uv` | 52,428,800 | 58,903,302 | 58,904,353 | setup-uv in workflows/composites; dependency/action key; multiple jobs/generations | #1801 |
| `zccache-tier2` | 576,716,800 | 511,627,930 | 1,031,126,183 | build-project rust-tier2 Linux build; both-mode source generation; compatible prefix fallback | #1801 |
| `zccache-ubuntu` | 1,181,116,006 | 792,124,048 | 792,124,048 | build-project Ubuntu app; OS/architecture/Rust mode/config/source; current production ON-only | #1801 |
| `zccache` | 734,003,200 | 0 | 0 | AppImage and sanitizers; AppImage run ID; sanitizer tool/mode/source; distinct shapes | #1801 |
| `windows-ccache` | 838,860,800 | 856,812,615 | 856,812,615 | build-project Windows app and native seed; release restore; x64/ARM64 + config/source; pooled retention currently | #782 |
| `xcode` | 524,288,000 | 667,020,432 | 667,020,432 | build-project macOS app and native seed; release restore; x86_64/ARM64 + config/source; pooled retention currently | #782 |
| `flatpak-builder` | 2,684,354,560 | 2,274,739,023 | 2,274,739,023 | build-project production Flatpak projected state; x86_64 + manifest/source; two-directory projection; one generation ~2.27GB | #1801 |
| `appimage-cargo` | 524,288,000 | 31,091,655 | 31,091,655 | appimage build; Cargo.lock/toolchain + run ID | #1801 |
| `appimage-testpkgs` | 262,144,000 | 340,789,311 | 340,789,311 | appimage packaged tests; test image + test script hash; two observed variants | #1801 |
| `codeql-overlay` | 209,715,200 | 0 | 0 | pinned scheduled CodeQL action internal; language/database overlay identity; no live entry in snapshot | #1801 |
| `service-check` | 1,048,576 | 0 | 0 | services-validator composite; workflow run ID + compatible prefix; master-only save | #1801 |
| `tier2-apt` | 419,430,400 | 360,061,197 | 360,061,197 | build-project Linux Tier2 build; multiple readers in ci.yml; container distribution/image + apt package hash | #1801 |
| `targets-ccache` | 314,572,800 | 0 | 0 | build-project targets Linux; source generation + prefix; no current entry is not proof unused | #1286 |
| `targets-apt` | 209,715,200 | 0 | 0 | build-project targets Linux; host image + package hash/source; separate from container | #1286 |
| `tier2-ctest-cost` | 1,048,576 | 7,558 | 7,558 | build-project Linux Tier2 test; source generation; scheduling metadata only, never passing results | #1286 |
| `windows-tier2-ccache` | 134,217,728 | 123,703,805 | 123,703,805 | build-project Windows Tier2; source generation + Windows prefix; OFF/ON reuse | #782 |
| `macos-tier2-xcas` | 134,217,728 | 136,466,246 | 136,466,246 | build-project macOS Tier2; source generation + macOS prefix; OFF/ON reuse | #782 |
| `qt-ccache` | 786,432,000 | 507,919,668 | 1,015,845,395 | build-project Qt Linux; inputs-v2 digest; guarded publication | #1801 |
| `vcpkg-cmocka` | 104,857,600 | 515,570 | 515,570 | build-project Windows Tier2; x64 Windows v1 | #782 |
| `libs-qt` | 67,108,864 | 25,346,743 | 25,346,743 | build-project Qt Linux; immutable Qt digest | #1801 |
| `libs-ffmpeg` | 134,217,728 | 66,330,774 | 66,330,774 | build-project Qt Linux; immutable FFmpeg digest | #1801 |

The companion source inventory records 101 cache/setup steps across twelve
workflow/composite files, with source hashes and exact step text retaining
conditions, paths, keys and inputs. This is source evidence, not execution proof.
It covers direct Actions cache, setup-soldr and setup-uv calls; implicit upstream
behavior is not exhaustively enumerated. Eight pinned setup-soldr source files
support the identity case below. The CodeQL row is a configured known namespace
and scheduled action call, not newly observed publication or net benefit.

Owned normal remote publishers are restricted to trusted master executions;
retained step text permits review of each condition. Eligible readers do not
establish that producers populated every variant. Default-off diagnostic/native
transport jobs and release readers are distinct from production writers.
Release asset storage and protocol ceilings are not adopted Actions capacity.

## Overlap and actual cleanup

Six new IDs appeared and one small uv ID was absent between captures. New Qt
and Tier 2 generations added 507,925,727 and 519,498,253 B, while their previous
507,919,668 and 511,627,930 B generations coexisted. Registry and mini also gained
same-key, different-version entries. The exact delta preserves IDs, refs,
versions, creation/access times and sizes. The uv absence does not identify an
actor.

Queue janitor [120099575434](https://github.com/obs-rust/obs-studio/actions/runs/39857545469/job/120099575434)
explicitly logged these three deletions at **21:28:41 UTC**:

| ID | Actions bytes | Logged selection |
| --- | ---: | --- |
| 9365021246 | 511,627,930 | Older Tier 2 generation; retained 9366948718 |
| 9365035221 | 507,919,668 | Older Qt input digest; retained 9366945969 |
| 9365023520 | 6,325,087 | Registry consumed by container; retained host 9366974950 |

These are actual logged actions. They do not prove deleting every older Qt input
digest is harmless to concurrent PRs. Earlier Qt/Flatpak disappearances still
have unknown actors. No manual cache delete callback was supplied here.

## Confirmed registry version identity gap

Master run [39856485267](https://github.com/obs-rust/obs-studio/actions/runs/39856485267)
on `6e84f3d62fb5aaf74d1289dbc0b744cf157350f0` completed **SUCCESS**:
14 successful jobs and 14 skipped jobs. Skips are not executed coverage.

Container Tier 2 build [120096064408](https://github.com/obs-rust/obs-studio/actions/runs/39856485267/job/120096064408)
restored the exact registry key at 21:22:31 UTC. Its `CARGO_HOME` was
`/github/home/.cargo`. Extraction reported 6,323,269 inner archive bytes,
27,491,621 restored bytes, 1,897 files and 281 ms. These are extraction metrics,
not full download time or end-to-end savings.

Host Fast Gate [120096064486](https://github.com/obs-rust/obs-studio/actions/runs/39856485267/job/120096064486)
used `/home/runner/.cargo` and published a different Actions version under the
same literal key. Both entries use
`setup-soldr-cargoregistry-v1-linux-x64-b829da24ac5be2aa-625183da3e11e618`:

| Context | ID | Actions version | Actions bytes |
| --- | --- | --- | ---: |
| Container | 9365023520 | `ecb564718b9d18ef98917abb022b2e18c22bec3b8c71bb4ca8252ce18f70a171` | 6,325,087 |
| Host | 9366974950 | `8eb2fc6ba181aeed396279239dc04cd2de559ec9a7425e6a1aa351bf43a1c1cc` | 48,930,463 |

The later janitor removed the consumed container version, explicitly naming the
host version as its replacement. At report time, the next container miss after
this deletion has **not** been observed.

Pinned setup-soldr `b782f606a2f3beadc35b9e3bf5b4565e005f52ed` constructs the
legacy registry archive at `${cargoHome}/registry.tar.zst`. Its mini archive
likewise derives from the install directory. Host mini restored at
`/home/runner/work/_temp/setup-soldr/bin`; container mini missed the same literal
key at `/__w/_temp/setup-soldr/bin`, then published ID9366966283. The snapshot
preserves distinct host/container mini versions. The +601-second replay selects
container mini as superseded; **that mini deletion is hypothetical**.

This overlaps existing upstream [ci.yml#75](https://github.com/zackees/ci.yml/issues/75#issuecomment-6351752907).
Merged #1354 namespaced cook archives but explicitly left registry/mini unchanged.
The upstream issue now has current consumer and actual deletion evidence.

## Remaining decisions and next steps

- **Registry/mini, #1801 and upstream ci.yml#75:** use saved same-key/different-
  version RED→GREEN fixtures in the pinned executor, preserving distinct usable
  versions while reclaiming genuinely superseded generations. Integrate an
  upstream fix or proven compatible archive-path strategy; observe the later
  consumer before attributing a miss. No native compiler job for policy arithmetic.
- **Cook/toolchain and touched targets, #1286:** inspect consumer profiles and
  paths, measure setup plus command time and growth. Preserve stale-output
  safeguards and demonstrated touched-target compiler/apt reuse. No listed entry
  does not mean no demand.
- **Windows/macOS app/Tier 2/dependencies, #782 with #1801:** qualify every
  architecture/configuration and source-edit consumer. This snapshot holds only
  Windows app ARM64 and macOS app x86_64. Revise #1723 within a genuine global
  allocation/transport plan; retain default-off qualification and all failures.
- **Flatpak, #1801:** use the completed [#1760 retrospective](../ci-speed-flatpak-retrospective-1760.md)
  for projected consumers, source changes and cold correctness: final warm jobs
  437/434/447 s and cold 846 s, including independent workflow failures. Observe
  later cleanup/consumers before claiming projected-state durability complete.
- **Linux Tier 2/Ubuntu/Qt/apt and immutable Qt/FFmpeg, #1801:** retain useful
  queue seeds and trusted publication; attach per-family transfer, hits,
  freshness and growth from existing traffic. Different Qt input digests are
  not necessarily interchangeable. Publication overlap needs capacity even when
  later cleanup reclaims old entries.
- **AppImage cargo/compiler/test-package, sanitizer, uv, CodeQL and services,
  #1801:** inventory internal writers/variants and measure transfer/net value
  and overlapping generations before choosing bounded changes. AppImage's
  current 31 MB cargo archive does not prove its 500 MiB allowance can shrink;
  two package archives exceed their allowance. Empty scheduled families do not
  prove their consumers can be removed.

These are outstanding requirements, not a measured rejection of every remaining
family or completed global allocation. #1801 and the broad goal remain active.
No compiler/test binary, C test, public header, workflow graph or runtime artifact
changes here. Never cache passing test results or skip required OFF/ON,
differential or protocol execution.

## Reproduction and limits

The companion stores exact inventories/configurations and production replay
reports, three raw job logs with numbered excerpts, terminal master metadata,
queue metadata captured while active, writer-step text and pinned source.
Each of its **26 records** carries length, SHA256 and base64 bytes. Historical
raw draft matrix wording remains unchanged; the table above reflects the merged
retirement and assigns uv investigation to #1801.

Companion SHA256:
`6ddc1253a231c97ea8abfde4ad27c8fca5a8ec5d7c335fb12a1e4f2ed4aee7f5`.

```sh
capture_root=$(safe-mktemp /tmp/obs-1801-allocation-replay)
uv run --no-project - "$capture_root" <<'PY'
import base64, gzip, hashlib, json, sys
from pathlib import Path
bundle = Path('docs/ci-evidence/2026-10-22-cache-allocation-followup.json.gz')
for item in json.loads(gzip.decompress(bundle.read_bytes()))['records']:
    raw = base64.b64decode(item['base64'], validate=True)
    assert len(raw) == item['bytes']
    assert hashlib.sha256(raw).hexdigest() == item['sha256']
    target = Path(sys.argv[1]) / item['path']
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(raw)
PY
for variant in current-audit traffic-followup-audit traffic-production-replay; do
  uv run docs/ci-evidence/2026-10-21-cache-retention-audit.py --output "$capture_root/$variant" --replay
done
safe-rm -r "$capture_root"
```

The executor receives saved pages and no delete callback. Future-clock plans
are hypothetical. Raw logs establish only specifically named actual actions.
No representative-day, causal performance or billed runner-minute comparison
follows. Job walls can include unacquired-runner wait. Actions archives, inner
compressed payloads, extracted data, configured working limits, reservations
and external release assets remain distinct.
