# First hosted native-cache qualification (#1269)

[Run 39761999302](https://github.com/obs-rust/obs-studio/actions/runs/39761999302)
is terminal **failure**, at master `d0f05829b035b9811440dd3b2a304abc4492fcdd`.
All native builds/local archives and the full build/test validation passed.
The publisher matrix had one success and three failures. No unchanged retry
was started. Production cache selection remains unchanged.

| Axis | Publication | Read-only restoration | Evidence |
| --- | --- | --- | --- |
| Windows ARM64 | Passed; 895,501,851 remote bytes including manifest | Hit for the required SHA, 36.438 s | Publisher total 80.797 s, including 33.984 s packing |
| macOS ARM64 | Passed; 845,128,441 remote bytes including manifest | Cold; HTTP 403 rate limit exceeded | Publisher total 74.402 s, including 31.530 s packing |
| Windows x64 | Not reached | Not reached | Build image 20261018.275.1 versus publisher image 20261007.264.1; identity rejected |
| macOS x86_64 | Not reached | Not reached | Exact same-SHA Actions seed absent at restore |

Two cache-only prereleases now contain four assets totaling **1,740,630,292
external bytes**, within the proposed per-axis and aggregate ceilings. They
are actual remote storage, separate from Actions cache; they are not production
backend adoption or proof of a native compile consuming release transport.

All four exact-SHA Actions seeds were initially observed after their native
builds. The pre-publisher inventory retained only ARM64 entries. Janitor job
[119777525456](https://github.com/obs-rust/obs-studio/actions/runs/39764812601/job/119777525456)
deleted Windows x64 seed 9334004266 at 10:40:23 UTC:
the pooled `windows-ccache` family exceeded its 838,860,800-byte footprint.
That seed's initially recorded size was 1,062,667,849 bytes. The deletion actor
for macOS x86_64 has not been established; its absence alone does not prove one.
Waiting for unrelated full validation before scheduling transfer exposes this
Actions-seed bridge to retention churn.

The image mismatch guard correctly stopped Windows x64. Its diagnostics had a
separate defect: rejection occurred before writing the captured profile, leaving
no artifact. The follow-up preserves the current identity and bounded image/
differing-field diagnostics before rejecting the mismatch. It does not weaken
the check, add a native test lane, or qualify the different toolchain.

The unauthenticated API rate-limit failure is distinct from upload integrity or
native reuse. Further work must address the publisher/consumer job boundaries,
read-only API authentication, and seed lifetime before another defined trial.
Do not accept an older generation as proof that this trial succeeded.

The compressed companion preserves exact JSON artifact/API bytes and hashes,
final job/step metadata, release inventories, initial/pre-publisher Actions
inventories, and numbered log evidence with complete-log hashes. No raw-byte
hash is computed from normalized display text. #1269 and #1667 remain open;
#1723 remains held pending complete allocation/transport evidence.
