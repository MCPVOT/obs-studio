# Building C, C++ and Objective-C: unity groups and precompiled headers

This is the build policy for every first-party C, C++ and Objective-C
source in this repository (`libobs/`, `libobs-*/`, `frontend/`, `plugins/`,
`shared/`). It exists so the native build is as fast as we can make it while
staying correct on every platform and every CMake generator. Rust has its
own rules in [docs/rust-port/testing-policy.md](docs/rust-port/testing-policy.md).

Out of scope: third-party code (`deps/`), the git submodules
(`plugins/obs-browser`, `plugins/obs-websocket`), the unchanged C tests
(`test/`) and the C oracle (`rust/obs-c-oracle/`). They build the way their
upstream ships them.

## Why

Compiling one translation unit per source re-parses the same headers
hundreds of times. Measured on a fresh Linux tree (`-j16`, no compiler
cache):

| Build | Wall | CPU |
|---|---|---|
| One TU per source | 315 s | 2416 s |
| Precompiled headers | 322 s | 1798 s |
| Precompiled headers + unity | 198 s | 1053 s |

CMake's own `UNITY_BUILD` got us those numbers but batches sources by count
in list order. Each OS has a different source list, so each OS gets different
batches, and a name clash between two files appears only on the platform
where they happen to share a batch: Linux built clean while Windows failed
in `libobs-d3d11`, `nv-filters`, `text-freetype2` and `win-dshow` (#1128,
#1139). Xcode also allows one precompiled header per target, so mixed
C++/Objective-C targets lost theirs, and a header that only compiled thanks
to the PCH broke on macOS alone. Per-file skip lists in CMake piled up to
work around both.

The fix is to make the grouping part of the source tree instead of a
build-system heuristic: the same files share a translation unit on every
compiler, OS and generator.

## The rules

### 1. File names say the role

| Role | C | C++ | Objective-C | Objective-C++ |
|---|---|---|---|---|
| Implementation (the code) | `foo.c.h` | `foo.cpp.hpp` | `foo.m.h` | `foo.mm.hpp` |
| Group (what the unity build compiles) | `name+.h.c` | `name+.hpp.cpp` | `name+.h.m` | `name+.hpp.mm` |
| Header (declarations) | `foo.h` | `foo.hpp` | `foo.h` | `foo.h` |

An implementation file is an ordinary source file: it compiles on its own
with `clang -x c foo.c.h` (or `-x c++ foo.cpp.hpp`). The double extension
only says that the default build reaches it through a group.

A group file contains nothing but includes of implementation files from
the same component, in alphabetical order:

```c
/* libobs/build/libobs.util+.h.c: PCH libobs */
#include "../util/bmem.c.h"
#include "../util/darray.c.h"
#include "../util/dstr.c.h"
```

Group files live in a `build/` directory next to the component's
`CMakeLists.txt` (`libobs/build/`, `plugins/obs-filters/build/`).

### 2. A group is one precompiled header

Every group names exactly one PCH variant on its first line
(`/* <path>: PCH <variant> */`), and every file in the group compiles with
that header first. Files are grouped by the header they need, not by
directory:

- A file that needs a feature macro defined before any system header
  (`_GNU_SOURCE`, `_FILE_OFFSET_BITS`) or the full `<windows.h>` (winsock's
  `u_long`) goes in a group whose PCH variant does exactly that
  (`libobs-gnu`, `libobs-win-full`). It never `#define`s the macro itself
  ahead of its includes: in a group, an earlier file has already included
  those headers.
- One language per group. C, C++, Objective-C and Objective-C++ files never
  share a group, which is what lets Xcode give each its own PCH.
- Platform-only files form platform groups (`libobs.windows+.h.c`), listed
  in the component's `cmake/os-<os>.cmake` like any platform source, with no
  `#ifdef` choosing between groups (see the platform rules in
  [AGENTS.md](AGENTS.md)).

PCH variants live in `cmake/pch/`. A variant is a header that includes the
headers its files share, and nothing project-specific beyond that. Add a
variant when files need a different set or a different first include; don't
exempt a file from the PCH.

CMake builds each group as its own `OBJECT` library carrying its PCH, and
the component's target links those libraries.

### 3. Unity and PCH are switches; the code is the same either way

| Option | Default | Off means |
|---|---|---|
| `ENABLE_UNITY_BUILD` | `ON` | Compile every implementation file as its own TU (`LANGUAGE` set from its extension); ignore group files |
| `ENABLE_PCH` | `ON` | Include the group's PCH variant as an ordinary header instead of precompiling it |

All four combinations must build. Unity off with PCH off is the check that
each implementation file includes what it uses, and the mode to use when
bisecting a miscompile or merging upstream changes.

### 4. Every implementation file is in exactly one group

A file in no group silently drops out of the unity build; a file in two
defines its symbols twice. A `./lint` check enforces exactly-one membership
and the one-PCH-line header of every group file. New code is added to a
group in the same commit that adds the file.

### 5. Names that are file-local must be unique within a group

Inside a group, every implementation file shares one translation unit, so:

- `static` functions and variables, file-scope `enum` enumerators and
  anonymous-namespace names get a prefix from their file
  (`od_` for `obs-output-delay`, `compressor_` for the compressor filter).
  An anonymous namespace does not help: the files share it.
- A file-scope `#define` is `#undef`ed at the end of the file. A macro that
  shadows a system name (`near`, `far`) is saved and restored with
  `push_macro`/`pop_macro`.
- Default arguments are declared once, in the header.
- Headers have `#pragma once`, are self-contained (they include or
  forward-declare everything they use, without relying on the PCH), and
  are never duplicated under two paths (a copy forwards to the original).
- No `using namespace std;` in a file that shares a group with headers
  that declare global names `std` also declares (`byte` from `<sapi.h>`):
  qualify instead, or move the file to its own group.

When a clash is cheaper to isolate than to rename (two backends that
deliberately define the same functions, a vendored fork), the file gets its
own group of one. That is the only exemption, and it lives in the source
tree, not in CMake.

### 6. Renames are mechanical and in place

Turning `foo.c` into `foo.c.h` keeps the directory and the stem, so
`git log --follow`, `git blame` and merges from `upstream/master` follow
the file. Each component converts in its own PR with this order of commits:

1. Only `git mv` of the implementation files (no content change), so rename
   detection is exact.
2. The group files, the CMake change, and path updates in `ci/`, the
   platform-ifdef baseline and `rust/` (oracle comments, READMEs).
3. Any clash fixes.

When an upstream merge adds a new `foo.c` to a converted component, rename
it and add it to a group in the merge PR.

## Verifying a change

- Linux: `./unittest` and the local gate as usual.
- Windows and macOS from Linux: `cmake/cross` (soldr's clang with the
  Windows SDK and the Apple SDK) builds libobs and the platform plugins
  through the same groups. Extend it when a component converts.
- A change to `cmake/common/`, `cmake/pch/` or any group file must pass a
  full Windows and macOS build (`gh workflow run pr-pull.yaml --ref
  <branch>`) before it merges: PR CI is path-scoped and does not build
  those apps.
- Check the off modes of a component you converted:
  `-DENABLE_UNITY_BUILD=OFF` and `-DENABLE_PCH=OFF`.

## Transition

Components not yet converted still build with CMake's `UNITY_BUILD`
batching and the skip lists in `cmake/common/unity.cmake` and
`cmake/common/pch.cmake`. Those lists only shrink: a new entry means the
file's component should be converted instead. When the last component
converts, the batching and the skip lists are deleted. Order: `libobs/`
first (most variants), then `libobs-*/`, `shared/`, `plugins/`,
`frontend/`.
