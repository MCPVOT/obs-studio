# Complete the owned cache writer policy (#1269)

Following #1670 (uv) and #1672 (Flatpak), a fresh inventory found additional
owned steps that could publish base caches from non-master refs:

- AppImage compiler, Cargo and test-package saves used `always()` without a
  ref restriction.
- Qt/FFmpeg dependency archives, Windows cmocka binaries and the service
  validator's timestamp cache used the combined `actions/cache` action,
  whose post step can save after a non-master job succeeds.

AppImage keeps its existing failure-save behavior on master; other refs now
only restore. Test-package exact hits still skip upload. The four combined
actions become independent restores and master-only saves, with the exact
same paths, keys, fallback keys and platform/service conditions. Their saves
run after the existing consumer/build/test steps with normal success gating
and skip an exact restore hit. Product commands and validation steps do not
change, including AppImage saved-profile/runtime coverage.

The configuration regression failed on the existing unguarded AppImage
saves and first combined cache action. GREEN inventories every owned workflow
and composite-action `actions/cache/save` step, requires an explicit master
guard, and rejects a reintroduced combined action. Separate pairing checks
verify the four split paths/keys and exact-hit conditions. Existing Qt,
Windows Tier 2, packaging and duplicate-key configuration tests also pass.

This guard concerns directly owned `actions/cache` steps. The separate uv,
setup-soldr and Flatpak controls remain necessary for those actions' internal
publication. It is not proof about every future upstream action side effect.

No live cache deletion or new family is introduced. No runtime saving,
storage reclamation or representative-day retention benefit is claimed.
The full validation on `f1783cf62` predates these guards and exercises the
unchanged consumer keys on master; it is not a hosted result for these new
conditions. Normal PR CI validates this revision, while future natural
master/non-master runs can supply publication/restore evidence.
