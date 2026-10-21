# Flatpak remote writer policy (#1269)

The October 21 inventory contains a **2,747,460,995-byte** Flatpak builder
archive and **60,760,795 bytes** of separate Flatpak compiler state on
`refs/heads/infra/875-widget-platform`. Those are observed bytes, not an
estimate of ongoing savings. A master run cannot restore a sibling branch's
cache, and repeated non-master writers compete for the same cache capacity.
The inventory does not identify an eviction actor or prove a speedup.

The old workflow deliberately allowed every full run to save both snapshots.
That policy conflicts with the current ci.toml master-only writer policy.
Pinned Flatpak action v6.7 couples saving to `cache: true`, which also enables
restores and the compiler's `--ccache` flag. Disabling `cache` would remove
useful compilation reuse.

## Change

Upgrade to v6.8, pinned at
[`79327416609af08178ad73b352877e51450790b3`](https://github.com/flatpak/flatpak-github-actions/blob/79327416609af08178ad73b352877e51450790b3/flatpak-builder/action.yml).
It adds an independent `save-cache` input, defaulting to true. Explicitly
set that input to the master ref, retain `cache: true` and
`restore-cache: true`, and apply the same master guard to the separate
compiler snapshot save. Non-master full validation still builds and restores
master's available state. Local compilation caching stays enabled.

The upstream release also adds optional debug bundles (default false) and
artifact-name overrides (the default reproduces the old name), and updates
bundled dependencies. The workflow does not enable either optional feature.
Existing package inputs, build flags, validation steps and runtime checks
are preserved. This is a cache publication change; no package contents are
intentionally changed. Native hosted Flatpak validation remains necessary
to qualify the dependency upgrade in its real environment.

## Evidence

The configuration regression fails before the change at both cache writers.
It passes after the upgrade and guards, while asserting that cache/restore
remain true. Existing packaging and duplicate-key configuration tests pass.

The [offline Node probe](2026-10-21-flatpak-save-probe.cjs) extracts the actual
Configuration/build/prepareBuild function bodies from both pinned distributed
entrypoints. It injects cache and command adapters, so no download, save,
Flatpak compilation or app execution occurs. Across eight cases (save on/off,
exact/fallback cache, and bundle on/off), the two versions issue identical
restore and build/bundle/metadata commands and keep `--ccache`. The new
version suppresses saves with the independent input; the old one ignores it.
Exact hits skip uploads in both versions. [Results](2026-10-21-flatpak-save-probe.json).

Reproduce after downloading the distributed `flatpak-builder/dist/index.js`
files from the exact v6.7 (`401fe28a8384095fc1531b9d320b292f0ee45adb`) and
v6.8 revisions into a scratch directory:

```sh
node docs/ci-evidence/2026-10-21-flatpak-save-probe.cjs /path/to/v6.7.js /path/to/v6.8.js
```

SHA-256 of the inspected distributed entrypoints:

- v6.7: `64837952182b4e9d6976a26d210766bbac7eb578cb8826a9f97efb2bb1a91d67`
- v6.8: `676fd5f2f7dd0116164a1db0d3abb1a8cb3f5488d764a64f5b4a8e126e05bcae`

The adapter probe does not exercise bundled dependency initialization or
GitHub storage transport. It is not a packaged-app runtime test or a hosted
performance measurement. Preserve #1269's remaining allocation and real
producer/consumer acceptance requirements. No live cache deletion, new
workflow, external repository, or registry publication is part of this change.
