# Ubuntu compiler retention reallocation (#1269)

The old shared nightly family could not retain the two compatible Ubuntu
archives measured in #1371: OFF 712,847,430 bytes and ON 795,699,586 bytes.
Its allowance was 1,153,433,600 bytes, shared with AppImage/sanitizer objects.
The subsequent actual ON deletion is recorded in
[#1269](https://github.com/obs-rust/obs-studio/issues/1269#issuecomment-6244868701).

## Allocation

A specific `zccache-ubuntu-` prefix now precedes the generic `zccache-`
prefix and has its own 1.6 GiB LRU allowance (1,717,986,918 bytes under the
pinned parser). Both measured modes fit with about 209 MB of additional
space. AppImage/sanitizer writes retain their previous 400/300 MiB pooled
allocation, isolated from Ubuntu. Linux Tier 2's independent family is
unchanged.

This capacity is reallocated from four allowances with measured smaller
payloads in the October 21 snapshot:

| Family | Measured archive bytes | Old allowance | New allowance |
| --- | ---: | ---: | ---: |
| Windows Tier 2 compiler | 41,427,262 | 800 MiB | 128 MiB |
| macOS Tier 2 CAS | 44,278,880 | 500 MiB | 128 MiB |
| Qt dependency archive | 25,342,371 | 1 GiB | 64 MiB |
| FFmpeg dependency archive | 66,331,281 | 500 MiB | 128 MiB |

Each new allowance exceeds twice its measured archive size; native Tier 2
has more than three times its observed size. No producer, restore key,
compiler content limit, or test invocation changes. These margins are
snapshot-based allocations, not a guarantee of future working-set size.
Newest-entry and ten-minute grace protection remain unchanged.

Total declared capacity falls from **16,363,867,340** to **15,171,007,282
bytes**, a reduction of **1,192,860,058 bytes**. It still exceeds the
10 GiB global budget. This fixes a measured family-local capacity failure;
it does not establish whole-repository headroom or stop GitHub's own
eviction under a writer burst.

## Reproduction and limits

The [offline generator](2026-10-21-ubuntu-retention-reallocation.py) calls
the exact pinned ci-lint classifier and janitor with no delete callback or
live API access:

```sh
uv run docs/ci-evidence/2026-10-21-ubuntu-retention-reallocation.py --output /path/to/probe
```

The fixture combines the saved October 21 inventory with the two measured
October 18 Ubuntu archives, then adds explicitly synthetic oversized newer
AppImage/sanitizer writes to challenge isolation. This is a hypothetical
coexistence scenario, not an observed live inventory. Missing historical
cache-version metadata is labeled; versions do not drive family classification.

The [result](2026-10-21-ubuntu-retention-reallocation.json) shows the old
configuration selecting both Ubuntu modes, while the new configuration
selects neither. No other fixture seed becomes newly deletable. The probe
checks the declarations decrease and both modes survive; no live cache is
deleted. The configuration regression fails before the change because Ubuntu
shares AppImage/sanitizer LRU, and passes after the split. The earlier
no-increase regression now counts both the recipient and donor families,
preserving the requirement that reallocation not increase their total.

Live producer/consumer retention across nightly publication remains to verify
under #1269. These results do not prove a performance improvement, a full-day
retention result, or completion of the global allocation requirement.
