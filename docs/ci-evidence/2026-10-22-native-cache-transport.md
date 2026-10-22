# Native compiler-cache transport candidate (#1269)

This is preparation for the [recorded transport plan](https://github.com/obs-rust/obs-studio/issues/1269#issuecomment-6335937323),
not a deployed backend or a completed allocation fix. #1715 separately
qualifies an isolated Flatpak rebuild. #1723 remains held; #1269 stays open.

## Archive boundary

`ci/native_cache_transport.py` packages native cache files/directories with
standard-library gzip level1 and tar. Hardlinked regular files are represented
as complete regular payloads; symlinks and special files fail explicitly.
Nothing is silently pruned. The gzip format is a portability candidate whose
actual native transfer/compression cost still needs measurement, not an
asserted improvement over Actions' zstd transport.

Metadata records schema/format, full producer commit, platform, architecture,
configuration, compiler, SDK and cache-format identity, compressed/expanded
bytes, entry count and SHA256. Consumers must supply the expected identity;
changing any identity field rejects the seed. Compiler-cache source freshness
remains the compiler cache's responsibility: matching archive identity alone
does not certify a source/object hit or compiler correctness.

The archive is strictly below2GiB; expanded data is at most4GiB and250,000
entries. These are format limits, not retention reservations or measured
working-store requirements. Archive and metadata limits apply independently.
Extraction verifies size/digest, rejects traversal/links/special entries and
Windows-ambiguous names, and stages all output before exclusively reserving a missing destination.
Verified files are linked into that owned directory; callers consume it only
after successful return, and a failed installation removes owned partial data. Existing output/destination paths are rejected. No network,
release/tag/asset, credentials, cache writer/key/path, budget, workflow or
required build/test lane has changed.

## RED → GREEN

- REDd2003bc0a: six focused cases fail against packaging/restore stubs.
- RED14a08064e: real hardlinked cache content fails packaging; an otherwise
  valid archive containing a Windows device filename is incorrectly accepted.
  This regression uses matching metadata so rejection is not accidentally
  caused by an unrelated entry-count mismatch.
- GREEN local focused signal: ten archive tests plus two stdlib checks pass.
  Covers real tar/gzip round trip, all identity fields, byte corruption,
  existing destination, malicious entries, actual hardlinks, pack/restore
  resource limits and cleanup. No network/compiler/native build is simulated
  as production evidence, and no expensive CI test lane is introduced.

## Remaining qualification before adoption

Integrate the trusted publisher and read-only consumer with immutable
checksummed identities, correct cold fallback, bounded per-architecture
retention/in-flight accounting and an explicit public cache namespace.
Preserve build-job read-only tokens and existing native test coverage. Measure
actual native archives, packing/transfer/extraction, compiler hits/misses and
publisher runner cost, including source/toolchain/configuration changes.
Validate real Windows and both macOS architectures before replacing existing
Actions writers or using this as global-capacity evidence. Actual release
publication/permissions remain a proposed route; no assets have been created.

Review follow-up RED3651d6f66 covers a destination created concurrently at
publication and case-insensitive collisions (including directory components).
Publication now uses exclusive mkdir ownership rather than POSIX rename;
path aliases are rejected on pack and restore.

## Local generation protocol (not hosted publication)

REDde912a329 adds five failing protocol cases. The implementation publishes
an immutable archive first and an immutable generation manifest last. That
manifest is the commit point; there is no mutable latest-pointer replacement.
Read-only consumers discover at most two matching complete generations,
verify downloaded bytes and archive metadata, and report a cold fallback
without installing partial data when no usable seed exists.

A local in-memory release fixture exercises actual archive bytes: round trip,
failed promotion retaining the previous usable seed, two completed retained
generations, missing/incompatible cold fallback, and untrusted context rejected
before external operations. Fifteen archive/protocol cases plus two stdlib
checks pass in0.419s. This is not a GitHub API, publication, native build or
performance measurement.

The proposed external bound is6GiB and6 assets per OS/architecture release,
including incomplete uploads, with two completed steady-state generations and
one publication in flight. Four native architecture releases therefore have a
24GiB hard maximum, independent of the10GiB Actions cache quota. These are
explicit candidate ceilings, not measured need or an adopted storage policy.
No assets/permissions have changed. Orphan recovery, retry reproducibility,
the actual GitHub adapter and workflow/consumer integration remain to be
finished and qualified before this draft is ready.

API references for the adapter design:

- [Release asset size/digest metadata](https://docs.github.com/en/rest/releases/assets).
- [Repository GITHUB_TOKEN event behavior](https://docs.github.com/en/actions/concepts/security/github_token).

Publication must use the explicit repository job token, never an inherited PAT
or custom token that could trigger product tag/release workflows. The actual
trusted job wiring and cache-only release namespace still require verification.

## Retry and abandoned-upload recovery

REDe7fc556c4 demonstrates non-reproducible gzip retry bytes, failed retries,
old orphan uploads exhausting admission, and starter-state manifests hiding a
usable prior seed. Gzip now fixes header timestamp/name while preserving
snapshot file metadata, so an unchanged snapshot produces identical bytes.
A retry verifies/reuses existing immutable assets without clobbering them.

Only uploaded, nonempty, digest-bearing archive/manifest pairs are complete.
A serialized publisher can reclaim incomplete generations after a ten-minute
creation grace period; valid complete pairs and reusable current-generation
assets remain protected. Recent orphans still count against admission limits:
publication refuses when necessary rather than exceeding the bound or
removing a retained usable seed. No background deletion job was added.

Twenty archive/protocol cases plus two stdlib checks pass in0.501s. Coverage
includes reproducibility across different wall times, a no-upload retry,
expired orphan recovery, recent orphan protection/budget refusal, and ignoring
starter manifests while retaining a real usable generation. The fixture is
local; GitHub adapter, actual publication and native qualification remain open.


### Review regressions: publication validation and malformed manifests

The additional local RED run produced six invalid-publication failures and four
non-object manifest errors. Publication now uses the same complete metadata and
streaming archive validator as restoration before accessing or mutating the
remote store. The producer reads the expanded stream without writing an extra
expanded copy; its CPU and read cost still require native qualification.
Non-object JSON manifests now fall back to the previous complete generation.
The focused transport and stdlib-policy suites passed 24 tests in 0.404 seconds.
These are local protocol fixtures, not evidence of real GitHub transport or
native build performance. No publication route has been enabled.


### Offline GitHub adapter boundary

A subsequent structural regression showed that a regular file `a` and child
`a/b` could pass publication scanning but fail restoration. Both entry orders
now fail before any store operation; implicit directory types are tracked.

The release-store adapter has six offline HTTP-boundary tests. The initial stub
failed all six; implementation plus the transport and stdlib-policy suites pass
31 tests in 0.396 seconds. These cover read-only missing inventory, permission
failure without creation, cache-only prerelease creation with `make_latest=false`,
foreign/product release rejection, bounded credential-free public download,
and bounded API JSON. Upload and real GitHub behavior remain unqualified.
The adapter has no workflow or CLI caller yet, and no real release, tag, asset,
permissions, cache writer or consumer has changed.


### Command orchestration

The publisher/consumer command now accepts an explicit bounded identity JSON
and writes a reserved, exclusive report. Publication validates the trusted
master Actions context before creating a store and uses only the explicitly
wired `NATIVE_CACHE_JOB_TOKEN`, never a `GH_TOKEN` fallback. The consumer creates
only a read-only store. Reports include packing, validation/publication and
total times; these local timings are not hosted performance evidence.

Two additional RED tests failed against the command stub. After implementation,
33 transport, adapter and stdlib-policy checks passed in 0.619 seconds. Review
of this command delta was clean. No native identity capture or workflow caller
exists yet: the identity fixture is not proof of real compiler/SDK freshness.

Workflow inspection corrected an earlier assumption: current native save guards
allow non-PR master dispatches as well as pushes. Therefore a qualification can
restore an exact same-SHA Actions seed after a successful explicit full dispatch,
without uploading a multi-gigabyte intermediate artifact or broadening the app
build token. Existing macOS master-push cold behavior remains distinct.


### Default-off native qualification route

The existing Pull and reusable Build Project workflows now expose the manual
`native-cache-transport-probe` input, default false. Ordinary PR/queue build and
test selection does not change. Successful full native builds capture selected
Xcode/clang/SDK identity or the Windows VS/default compiler binary and installed
toolset/SDK inventory. Windows inventory invalidation is conservative; it is not
a claim that CMake selected every installed SDK. Object caches keep their own
compiler/input validation.

The opt-in native steps pack and restore the real cache locally without creating
a network store, retain only tiny JSON evidence, and clean temporary copies.
A separate four-axis publisher can run only on a successful master dispatch.
It selects the matching Xcode, checks its identity against the completed build,
requires an exact same-SHA Actions seed, and uses an explicitly passed job token
for cache-only release writes. Its subsequent unauthenticated read-only consumer
must restore that same source SHA. Falling back to an older good generation is
valid ordinary consumer behavior but fails this qualification.

CI Gate includes this job, which is skipped outside the opt-in master dispatch.
Application build tokens stay read-only. Per-axis publication is serialized and
matrix failure does not cancel other axes. Existing uv master-only save policy
and literal cache-family prefixes are preserved. No new workflow file, ordinary
PR lane, actual release/tag/asset, or production cache backend change was made.

Local RED evidence covers the required hit, older-generation rejection, no-store
local verification and trust/scope wiring. The first full lint exposed existing
cache-family/uv/gate policy violations; those were corrected without exemptions.
The combined focused policy/transport suites then passed 60 tests in 0.712 seconds.
Native runner execution, actual release transport and compiler hit/build effects
remain unqualified. A branch dispatch can qualify archive handling without
publication; actual trusted publication needs the integrated route on master.
This route is a candidate experiment, not production backend adoption or closure
of #1269 or #1667.


### First actual native archive qualification (121236fc3)

Run [39752689352](https://github.com/obs-rust/obs-studio/actions/runs/39752689352)
completed all four native application jobs successfully. Each identity capture
and local pack/restore reported `verified-local` for the tested source SHA.
The remaining full-run Tier 2/macOS jobs were still active at this snapshot;
this is not yet a full-run green claim. Machine-readable artifact bytes,
API metadata, selected numbered log lines and complete-log SHA256 provenance
are in `2026-10-22-native-cache-first-qualification.json.gz`.

| Native axis | Archive bytes | Expanded bytes | Packing seconds | Total local verification seconds |
| --- | ---: | ---: | ---: | ---: |
| Windows x64 | 1,057,077,711 | 1,062,731,467 | 44.421 | 51.359 |
| Windows ARM64 | 895,505,830 | 900,444,076 | 25.578 | 31.750 |
| macOS x86_64 | 737,404,708 | 1,594,977,357 | 29.450 | 37.083 |
| macOS ARM64 | 845,117,547 | 1,826,582,787 | 36.078 | 44.237 |

All archives fit the candidate format bounds. Windows caches were cold: no
Actions seed found, 0/289 x64 and 0/263 ARM64 cacheable calls hit. macOS x86_64
also found no Actions seed. macOS ARM64 restored
`macOS-xcode-arm64-RelWithDebInfo-5db4658e11fb1066ea236223a55e40c584a69797`
and reported 464/660 cacheable tasks hitting (70%). These describe builds before
local archive verification, not builds consuming the new release transport.

The report's historical `validation_and_publication_seconds` field means local
validation/restoration in `verify-local` mode; no publication occurred. Packing
adds measurable producer cost. These gzip archive sizes cannot be substituted
for Actions/zstd inventory sizes or #1723 retention reservations. In particular,
new macOS ARM64 gzip bytes exceed 800 MiB, so a future allocation must use the
actual selected transport and current data, not silently reuse an older budget.

Next: retain the complete run outcome and required coverage, then land only the
integrated default-off experiment through normal PR CI/queue to permit a trusted
master publication qualification. Do not adopt the backend or claim hosted
transfer/native consumer reuse before those actual measurements.


### Completed first qualification run

Run 39752689352 is now terminal **success** at source
`121236fc30bff455ea23eb4655e63d8400036486`: 30 jobs total, 22 successful and
8 explicitly skipped. The evidence bundle preserves both the earlier live
snapshot and the final complete job/step metadata, plus Tier 2 log provenance.
No job was cancelled or retried.

- Linux Tier 2 OFF and ON: 360 registered, 356 executed and four Pulse skips
  per mode, zero failures. CTest times 166.10 / 187.45 seconds respectively.
- Windows Tier 2: four suites (CMake C/Rust and Cargo C/Rust), 178 tests each,
  all passing; times 83.48 / 81.37 / 82.85 / 81.86 seconds.
- macOS Tier 2: four CMake/Cargo C/Rust suites, 203 registered and 202 executed
  per suite, one SVT-AV1 skip each; times 135.63 / 120.22 / 121.81 / 120.41 seconds.
- Four native app builds and real local archive checks, Flatpak, Ubuntu app,
  Qt, Fast Gate, Rust tests/cross lint and format jobs all passed.

The native publisher and prepared-environment experiment were deliberately
skipped: this branch dispatch cannot publish cache releases. Existing event/scope
skips and the compatibility Linux summary job are recorded separately in the raw
job metadata. A green branch run does not qualify hosted transfer or a native
build consuming that transfer. Proceed through normal PR CI/merge queue with the
experiment default off, then measure one trusted master producer/read-only
consumer qualification before deciding backend adoption or #1269 closure.


## Follow-up: authenticated read-only transport consumer

The [first master qualification](2026-10-22-native-cache-hosted-qualification.md)
found a public API rate-limit failure after successful macOS ARM64 publication.
The follow-up moves archive restoration out of the publisher into a separate
manual-master-only job with `contents: read` and `actions: read`. Its explicit
job token authenticates API inventory calls; public asset downloads still omit
Authorization. No writer token is passed to this consumer.

The consumer uses the completed build's identity artifact and requires the exact
current source SHA. It runs all four archive axes on Linux, since it verifies
transport rather than executing native compiler artifacts. This is not evidence
that a native compiler accepted or reused that cache. Actual native consumers
must still validate their current compiler/SDK/configuration identity.

`always() && !cancelled()` permits evidence from surviving axes if another
publisher fails. Both publisher and consumer remain required by the final gate;
a successful restoration cannot override a publication failure. Normal PR and
queue runs skip the default-off probe. No new workflow files, native builds,
C test binaries or production cache writers are added.

Local RED: the workflow regression rejects consumption inside the writable
publisher and the absence of a separately scoped consumer (two failures).
GREEN: the focused workflow, release-adapter and archive suites pass, 37 checks.
Hosted validation remains outstanding. This only addresses the authentication
boundary: seed lifetime and publisher image drift still require resolution
before another complete qualification. It does not authorize an unchanged
full-workflow retry or production adoption.

GitHub documents [job-level token permissions](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idpermissions)
and the [separate authenticated API rate limit](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api).
Authentication avoids the observed shared unauthenticated allowance; it does
not promise immunity to authenticated primary or secondary rate limits.
