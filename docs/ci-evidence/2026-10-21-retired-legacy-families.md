# Retire unused pre-zccache reservations (#1269)

Three declarations remain after their consumers were removed:

| Family | Literal key prefix | Removed allowance |
| --- | --- | ---: |
| `tier2-ccache` | `tier2-ccache-linux-` | 110 MiB |
| `appimage-apt` | `apt-appimage-` | 250 MiB |
| `appimage-ccache` | `ccache-appimage-` | 100 MiB |

The current workflow/composite-action cache key and restore-key inventory
contains none of these prefixes. Searches of `.github/`, `build-aux/`,
`rust/tools/`, `cmake/`, and production CI scripts likewise find no literal
consumer. `zccache-appimage-` is an active, different prefix and is preserved;
a substring search for `ccache-appimage-` must not conflate them.

A fresh paginated API listing during this change contained 41 entries, with
zero entries under each retired prefix. This is a point-in-time observation,
not a retention-duration result. No cache was deleted for this change.

Removing these families reduces declared capacity by **482,344,960 bytes**,
from **15,171,007,282** to **14,688,662,322 bytes**. It does not reclaim
additional live storage, and the resulting declaration still exceeds the
10 GiB global budget. The unchanged global budget must not be represented
as an enforced whole-repository bound by this retirement.

The RED configuration regression reports all three obsolete reservations.
It passes after removal and verifies there are no remaining workflow cache
consumers before permitting retirement. Existing declaration coverage and
Tier 2 retention tests preserve the active zccache, apt, dependency and
native cache families. No producer, test invocation, package input, cache
restore, or janitor safety rule is changed.

The full validation run started on `f1783cf62` predates this declaration-only
retirement. Its native consumer results can qualify the unchanged producers,
but are not a full-run result for this later revision. PR CI remains the
merge gate for the new configuration. Whole-budget allocation and durable
producer/consumer retention remain open under #1269.
