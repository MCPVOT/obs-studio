<!-- docs/rust-port/coordination.md -->
Fixes #  <!-- or Refs # when this does not complete the issue -->

### What and why

### RED → GREEN
- RED commit: <!-- SHA, and how it fails (test names / counts); N/A for docs-only. A focused CI fix may push RED and GREEN together with local RED evidence -->
- GREEN: <!-- result, e.g. Tier 2 N/N against hybrid libobs and libobs-c-reference; N/A for docs-only -->
- Real app (UI, protocol output or logging changes): <!-- Tier 4 run or manual run, and what it showed -->

### Checklist
- [ ] Base is `master` (or the description starts with a **Stacked:** note)
- [ ] I claimed the issue, or no one else has an open PR for it
- [ ] Characterization test (if the unit had none), then RED, then GREEN commits (N/A for docs-only; a focused CI fix may push RED and GREEN together)
- [ ] User-visible change: its line in `docs/features.md` is added or updated (not applicable: no user-visible change)
- [ ] (Optional) Local gate passed and the pushed head is stamped; otherwise PR CI runs the full suite
- [ ] Warning or flag changes (`-W...`, ratchets): built with scripting ON (PR CI builds no OBS app: run `gh workflow run ci.yml --ref <branch>` for the full builds) and cross-checked with clang and MSVC
- [ ] Intentional differences from C listed in the parity test header and rust/README.md (ports)
