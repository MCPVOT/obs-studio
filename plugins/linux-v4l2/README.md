# Linux V4L2 capture module

The CMake target is `linux-v4l2` (`OBS::v4l2`). It links against libobs,
Libv4l2 and FFmpeg's avcodec, avformat and avutil libraries. Udev support
is enabled by default and additionally requires Libudev. See
[CMakeLists.txt](CMakeLists.txt) for the authoritative dependency checks
and [BUILD.md](../../BUILD.md) for the repository build environment.

## Scoped CI build

Changes under this module select the existing Linux target build in
Fast Gate. CI configures with the frontend and other optional integrations
disabled, then builds `linux-v4l2` and its dependencies. This compile/link
check does not replace tests that need a real capture device.

The scoped lane uses its own `targets-ccache-linux-*` compiler cache,
matching the same CMake configuration on PR and master. Trusted master
builds save the shared entry; PRs restore it. A batched master push must
select paths from the push's full before/after range to warm this module
even when its change is not the final commit.

Read the compiler's hit/miss statistics and total job time together.
Downloading an Actions cache archive alone does not establish reuse.
The [CI cache audit](../../docs/ci-cache-audit-2026-10-17.md) tracks the
controlled cold/warm measurement and its source/runner limitations.

The scoped build restores a host-specific apt package archive before
installing its distro dependencies. It refreshes apt metadata normally and
downloads missing package versions as needed. Trusted master builds save
this bounded archive; PRs and merge groups only restore it.

The host apt namespace includes the runner image and package-list hash.
Master uploads only snapshots within the 200 MiB family bound; an oversized
snapshot leaves the previous remote entry available. For performance audits,
record the usable package count and full job time alongside compiler hits:
the measured compiler-only warm trial had 41/41 hits while package download
variance still made the full job slower.
