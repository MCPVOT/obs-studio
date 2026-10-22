# Isolated pruned-state application rebuild (#1269)

The prior single capacity experiment (39722540093, #1712) completed
SUCCESS. Its canonical compression proxy fell from 2,703,989,420 bytes to
1,624,465,624 bytes when omitting downloads and compiler state. Fresh
source acquisition took 26.691 seconds. These observations do not establish
actual transport savings or a successful application rebuild from that state.

This default-off manual probe copies retained builder state into an isolated
temporary directory. The revised candidate retains only the cache dependency
repository, omitting downloads, ccache and transient builder scaffolding. It never
builds in, moves, deletes or prunes original state. Structural repository symlinks and scratch inside original state are rejected.
OSTree checksum-addressed file link objects are preserved as opaque links,
never followed; the copied repository must pass fsck before building. Copying may affect original atime.

A temporary prepared manifest beside the original preserves relative source
resolution. It adds an executable-existence check and a unique stdout marker
to the final obs module's post-install commands. This changes that module's
recipe checksum so an unchanged application-module hit cannot masquerade as
a rebuild. The helper requires execution of the marker, rejects an obs cache
hit, and requires cache hits for all unchanged dependency modules. The
original prepared manifest is unchanged and the temporary manifest is removed
on both success and failure. No source, existing C test or product packaging
policy is changed.

The isolated build uses the installed builder/SDK, xvfb, an empty compiler
cache, a separate build directory and a separate repository. It then checks
binary/icon/XML outputs and runs the existing flatpak-builder-lint exception
rules on AppStream, builddir and repo. Existing normal build/output validation
runs first and remains required. Build timeout is 30 minutes, each lint two
minutes, and the optional step 40 minutes. Failures fail the job and produce
a small failed-status JSON; no continue-on-error bypass. Temporary trees are
cleaned by context managers, including command failures.

## Verification and defined trial

RED `54645ed68`: four focused tests fail (three absent-helper errors and
one absent-wiring assertion). GREEN fixtures exercise orchestration with a
simulated command boundary: they verify copied/omitted state, relative source
resolution, required validation, original-manifest preservation, cleanup and
rejection of missing rebuild signals. Fixtures do not claim a real build.

After review, run exactly one full workflow on the pushed GREEN branch with
flatpak-pruned-build-probe=true and native-cache-probe=false. Keep complete
normal selection and validation. This is a distinct changed-state rebuild
experiment, not a retry of the completed capacity measurement. Record source,
commands, actual module reuse/rebuild output, versions, output validation,
JSON, hashes and terminal selected-platform results. The initial hosted result and revised candidate are recorded below.

No production cache writer, key, path or limit changes are adopted. A later
production transport migration needs matching restore/save identities and
actual trusted producer/consumer evidence. Global allocation, writer headroom,
independent native architecture retention and representative durability remain
unresolved under #1269. This does not run a packaged bundle or saved user
profile and does not claim runtime validation or end-to-end speedup.


## Initial hosted failure and revised dependency projection

Run39725882580 on659756295 completed Flatpak job119650969777 with
FAILURE. Its normal Flatpak build and output validation passed, but the
optional probe rejected symbolic links before starting the isolated rebuild.
The [failed record](2026-10-22-flatpak-pruned-build-failed.json.gz) preserves
job metadata, capture-time parent status, failed JSON and log hash/excerpts.
The original generic error did not identify the link location; do not infer
which subtree contained it or claim any pruned rebuild succeeded.

[Upstream flatpak-builder 1.4.9 builder-cache.c](https://github.com/flatpak/flatpak-builder/blob/1.4.9/src/builder-cache.c)
locates the dependency OSTree repository in the cache directory and creates
it in bare-user-only mode. That source matches the installed version label;
it is not a byte identity attestation of the distributed binary.

The revised projection copies only that repository. Transient state is
regenerated, and no transient checkout tree can supply compiler outputs to
the rebuild. Symbolic links in retained dependency state remain rejected;
the error now identifies a bounded relative path. No general permission to
follow links into original state or external paths is introduced. Regression
RED89f231a81 (rebased from local9e503de5e) reproduces rejection of a transient-tree link by the original
helper; the revised projection passes while preserving original state.

The revised candidate still requires an actual forced application rebuild,
dependency hits and output validation. After the initial full run is terminal,
one explicitly defined new-source experiment may qualify the changed
projection. Never retry the unchanged workflow or treat the original failure
as successful validation. Production transport/allocation remains unchanged.


### Terminal first-trial outcome

First full run39725882580 completed FAILURE. Only the Flatpak probe job
and final aggregate gate failed; all other selected jobs passed. Windows
completed four175-test CTest passes and differential. macOS completed
four200-registered/199-executed passes with one SVT skip each and
differential. Exact Linux OFF/ON counts, skips, log hashes and excerpts
are in the failed record alongside appended terminal run/job metadata.
Earlier capture-time metadata is retained. None establishes pruned rebuild
success: the optional guard failed before the isolated build began.

After the first run was terminal, the single revised trial39729660321
was dispatched on446a12945b44a3c647995d5e86c2f96502638cad with
flatpak-pruned-build-probe=true, flatpak-cache-probe=false and
native-cache-probe=false. It preserves full selection and existing normal
validation before probing the narrower dependency projection. No result
claimed yet; no unchanged-workflow retry or production policy adoption.


## Second hosted failure and typed object-copy correction

Second trial39729660321 on446a12945 completed Flatpak job119661622603
with FAILURE after normal output validation. The diagnostic now identifies
`cache/objects/19/295cb5c20e23c52c02cb635c5bfcf569a0f02ad8c62de8bdbb24b5f8e74ee2.file`.
The isolated application rebuild never started. Its link target and repository
mode were not logged; do not infer them from the builder version. Existing
caches can predate that builder. Metadata, failed JSON and log excerpts/hash
are appended under second_trial in the failed record.

Windows job119662888783 independently failed to link cm_print_error from
test_video_output_plan before CTest. It is recorded with existing #1713
ownership. #1580 was the resolved occurrence in a different test, not a
reopened blocker. No C test or public header was changed here. Parent was
still active at capture; this is not full-green qualification.

REDe5cc741a2 (pre-rebase4d941bfcd) rejects a checksum-shaped OSTree link object in the old guard.
The correction preserves only paths matching cache/objects/HH/HASH.file as
links, using copytree(symlinks=True); it never dereferences their targets.
Control-path links remain rejected. Real ostree fsck validates the isolated
copy before any application build; version/mode/object count/fsck duration
are recorded. Original files and link targets are not passed as writable
state to any command. Failed configuration parsing also produces failed JSON.

The [real format fixture](2026-10-22-ostree-copy-fixture.py) passed with
local OSTree2026.1 in bare mode: a real committed symlink object survives
copy/fsck, modifying copied config leaves original config unchanged, original
link target is unchanged, source fsck still passes and temporary paths clean
up. The [result](2026-10-22-ostree-copy-fixture.json) records binary/helper/
fixture identities and limits. It intentionally stops before building OBS;
this is not a hosted application rebuild or parity of tool versions.

Reproduce with ostree on PATH (the observed invocation used a Nix shell):

```sh
ostree_fixture_root=$(safe-mktemp /tmp/obs-ostree-copy-fixture)
nix shell nixpkgs#ostree --command uv run --no-project docs/ci-evidence/2026-10-22-ostree-copy-fixture.py "$ostree_fixture_root"
safe-rm -r "$ostree_fixture_root"
```

The Nix package locator may advance; the observed binary identity/version
is recorded rather than claiming the locator fixes a future tool version.
After the second full run is terminal and the correction is reviewed, one
new-source trial must still qualify the actual forced build, six dependency
hits and output validation. No third dispatch yet, no unchanged workflow retry
and no production adoption or success claim.


### Terminal second-trial outcome

Run39729660321 is now terminal FAILURE. Flatpak and Windows Tier 2 failed
for the independent reasons above; macOS Tier 2 and all other selected
non-aggregate jobs passed. The aggregate CI Gate failed. Full terminal
run/job metadata is appended under second_trial; earlier capture-time
metadata remains preserved. No isolated application rebuild has succeeded.

## Third changed-source qualification (pending Flatpak result)

Run39732161172 uses reviewed f4cbc955df8da30a29238f3b5d267a29e22e4564.
Both previous full trials were terminal before this dispatch. Normal Flatpak
build/output validation completed and the pruned rebuild probe started at
2026-10-22T06:22:47Z. This observation does not yet prove the isolated
build or any of its validation checks completed.

Completed Linux OFF/ON jobs119670535142/119670535097 each report358
registered tests,355 executed and three expected PulseAudio skips, zero
failures. macOS job119672176825 passed four202-registered CTest runs,
each with one SVT skip. Windows job119672176937 failed before CTest with
cm_print_error from test_video_output_plan, retaining the failure from the
previous trial. The immediate fix remains under #1713 ownership; #1739
records why the narrowly scoped earlier guard and Linux-only PR selection
missed the recurrence. #1739 requests proportionate temporary prevention,
not broad additional Windows CI or permanent infrastructure for cmocka.

The [third-trial record](2026-10-22-flatpak-pruned-build-third-trial.json.gz)
contains capture-time metadata and hashes/excerpts for the completed jobs.
Parent and Flatpak outcome remain pending in this capture. A green PR Gate
does not substitute for the actual manual probe, and a later successful
Flatpak job cannot erase the full trial's independent Windows failure.


### Terminal third-trial failure

Run39732161172 completed FAILURE. Isolated OSTree fsck succeeded for10
commits/19,104 objects and the forced application marker appeared. However,
all six dependencies rebuilt instead of producing cache hits. The probe
correctly failed with `dependency module reuse missing: obs-deps-buildstream-base`.
Output validation after that guard never ran. This proves the object-copy
barrier is resolved, but does not qualify the pruned dependency projection.
The Windows link failure remains independent. Full terminal metadata and
Flatpak artifact/log hash/excerpts are appended in the third-trial record.
No fourth trial or production change is authorized by this result alone;
first determine why the dependency repository was not reused.


### Identified cache branch mismatch

Pinned action79327416609af08178ad73b352877e51450790b3 passes
`--state-dir` and its prepared manifest path. flatpak-builder1.4.9
`builder-main.c:975-992` derives the branch from architecture plus the
manifest's absolute path whenever that option is present, then URI-escapes
it with percent replaced by underscore. The probe's random temporary
manifest therefore selects a different branch from the original prepared
manifest in the copied repository. Removing transient scaffolding alone
cannot preserve this identity. Source URLs, hashes and excerpts are in
the third-trial record. A correction must preserve the original stage
identity on the isolated copy without writing the original manifest/state,
and still force the final application module and require all six hits.
No new hosted dispatch has occurred.


### Ref identity RED and local correction

RED28a6b1efc makes the existing isolated-build fixture select dependency
refs using the prepared manifest's absolute-path namespace; old helper fails.
The correction aliases the original stage refs into that namespace only
inside the independent copy, then runs fsck. Final module changes still
force OBS to rebuild; the six dependency-hit checks remain mandatory.
The real OSTree fixture now verifies the selected remapped ref resolves
to the original commit and that no matching ref was created in the source.
It passed with local OSTree2026.1; previous copy-only execution is retained
in the result wrapper. This still stops before any real application build.
31 focused tests pass. No fourth hosted dispatch yet.

## Fourth trial: dependency reuse established, output lint failed

Run39735957434 on218c48278 completed FAILURE. The private repository
passed fsck; all six unchanged dependency modules reported cache hits;
OBS rebuilt and emitted the marker. Binary/icon/XML checks and AppStream
lint passed. Build-directory lint then rejected
`appstream-external-screenshot-url`: screenshots were not mirrored to
https://dl.flathub.org/media. Repository lint after that failure did not run.
The [fourth-trial record](2026-10-22-flatpak-pruned-build-fourth-trial.json.gz)
retains terminal metadata, artifact, hashes/excerpts, Linux OFF/ON and macOS
coverage, and the independent Windows cm_print_error link failure.

The pinned action supplies `--mirror-screenshots-url` and
`--compose-url-policy=full` (index.js250-253), then commits media with
`ostree commit --canonical-permissions --branch=screenshots/x86_64`
(index.js318-337). The probe omitted this production behavior. REDf1d53548f
requires the builder options; RED7fdff3615 requires the screenshot commit
before lint. The correction matches both on the isolated output, preserving
all validation and exceptions.31 focused checks pass. No fifth dispatch yet;
actual builddir/repository validation remains unproven.


The large trial records use deterministic gzip without changing their JSON
bytes. Read one with `gzip -dc <record.json.gz>`; decompression was verified
byte-for-byte against its original, including preserved failure evidence.

## Fifth trial: isolated rebuild and output validation succeeded

[Run39739711001](https://github.com/obs-rust/obs-studio/actions/runs/39739711001)
on ae128dc16 is terminal FAILURE solely due to Windows Tier 2 and the
aggregate gate. Flatpak job119694606911 completed SUCCESS. The
[fifth-trial record](2026-10-22-flatpak-pruned-build-fifth-trial.json.gz)
retains terminal metadata, the success artifact, hashes/excerpts, Linux
OFF/ON and macOS coverage, and the independent Windows failure.

The actual copied repository reports bare-user-only with70 symbolic objects;
hosted OSTree2026.2 fsck passed in18.644s. All six dependencies hit cache,
OBS rebuilt and emitted its marker, required binary/icon/XML checks passed,
and AppStream/builddir/repository lint all succeeded. Screenshot composition
and media ref commit used the production options. Isolated build935.793s;
copy, fsck and output validation are excluded from that build timer.

Linux OFF/ON each registered358 tests with355 executed and3 expected
PulseAudio skips. macOS passed four202-registered runs with201 executed
and1 SVT skip each. Windows failed before CTest on cm_print_error in
test_video_output_plan; it remains owned under #1713 and explained by #1739.
No full-green trial or Windows coverage success is claimed.

This qualifies the isolated cache-only rebuild/output-validation mechanism.
It does not establish actual Actions archive savings, release/registry
transport, a trusted producer/consumer rollout, runtime with a saved profile,
representative cost or global allocation. No production cache policy changed.
#1269 and the global retention work remain open; #1723 remains held.
