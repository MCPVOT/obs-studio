#!/usr/bin/env -S uv run --script
"""A port that only hides a symbol in all three libobs symbol lists starts
no macOS or Windows job (#2288). Windows never reads the lists, and
rust/obs-util's export_list test checks on Linux that the three agree.

    uv run --no-project ci/test_hidden_symbol_scope.py
"""

from __future__ import annotations

import unittest

from ci_scope import HIDDEN_LISTS, hidden_lists_mirrored, scope

HIDDEN = "# hidden shims\nbase_begin_crash\n"
MAP = "{\n  local:\n    _ZN*;\n    /* base.c */\n    base_begin_crash;\n};\n"
MACOS = "# ld64\n__ZN*\n_base_begin_crash\n"
BASE = dict(zip(HIDDEN_LISTS, (HIDDEN, MAP, MACOS)))


def add(name, *, hidden=True, map_=True, macos=True, comment=False):
    new = dict(BASE)
    paths = dict(zip(("hidden", "map", "macos"), HIDDEN_LISTS))
    if hidden:
        new[paths["hidden"]] += f"{name}\n"
    if map_:
        note = "    /* obs-foo.c: not EXPORT. */\n" if comment else ""
        new[paths["map"]] = new[paths["map"]].replace("};", f"{note}    {name};\n}};")
    if macos:
        new[paths["macos"]] += f"_{name}\n"
    return new


class HiddenSymbolScopeTest(unittest.TestCase):
    def mirrored(self, new):
        return hidden_lists_mirrored(BASE.get, new.get)

    def test_mirrored_addition_is_recognised(self):
        self.assertTrue(self.mirrored(add("obs_foo_plan")))
        self.assertTrue(self.mirrored(add("obs_foo_plan", comment=True)))

    def test_mirrored_removal_is_recognised(self):
        self.assertTrue(hidden_lists_mirrored(add("obs_foo_plan").get, BASE.get))

    def test_unmirrored_change_is_not(self):
        self.assertFalse(self.mirrored(add("obs_foo_plan", macos=False)))
        self.assertFalse(self.mirrored(add("obs_foo_plan", map_=False)))
        self.assertFalse(self.mirrored(add("obs_foo_plan", hidden=False)))

    def test_structural_map_change_is_not(self):
        new = dict(BASE)
        new[HIDDEN_LISTS[1]] = MAP.replace("{\n", "{\n  global:\n    obs_foo;\n")
        self.assertFalse(self.mirrored(new))
        new[HIDDEN_LISTS[1]] = MAP.replace("_ZN*;", "_ZN*;\n    _R*;")
        new[HIDDEN_LISTS[0]] = HIDDEN + "_R*\n"
        new[HIDDEN_LISTS[2]] = MACOS + "__R*\n"
        self.assertFalse(self.mirrored(new), "wildcards change what every linker hides")

    def test_mirrored_lists_start_no_native_platform(self):
        files = [*HIDDEN_LISTS, "rust/obs-core/src/ffi/foo.rs", "libobs/obs-foo.c.h"]
        got = scope(files, hidden_mirrored=True)
        self.assertTrue(got.core and got.tier2)
        self.assertFalse(got.macos or got.windows)

    def test_unmirrored_lists_still_start_every_platform(self):
        got = scope(list(HIDDEN_LISTS))
        self.assertTrue(got.macos and got.windows)

    def test_other_libobs_cmake_files_stay_shared(self):
        got = scope(["libobs/cmake/rust-exports.d/obs-foo.txt", HIDDEN_LISTS[0]], hidden_mirrored=True)
        self.assertTrue(got.macos and got.windows)


if __name__ == "__main__":
    unittest.main()
