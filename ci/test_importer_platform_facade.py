#!/usr/bin/env -S uv run --script
"""Importer host boundaries and legacy path/source behavior (#881)."""

import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import platform_ifdef_ratchet as ratchet  # noqa: E402
from test_settings_platform_facade import function  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OPERATIONS = ("PlatformImporterLastSeparator", "GetPlatformClassicImportDirectory",
              "GetPlatformStreamlabsImportDirectory", "GetPlatformXSplitImportDirectory",
              "TranslatePlatformImportedSource")

# Expected mappings from the pre-facade shared implementation. Settings are
# preserved even for ClearTranslation: its temporary empty settings object is
# overwritten at the end. Characterized here, not endorsed or repaired (#881).
MAPPINGS = {
    "mac": dict(zip(
        ["text_gdiplus", "game_capture", "wasapi_input_capture", "wasapi_output_capture",
         "pulse_input_capture", "pulse_output_capture", "jack_output_capture", "alsa_input_capture",
         "dshow_input", "v4l2_input", "xcomposite_input", "xshm_input"],
        ["text_ft2_source", "syphon-input", "coreaudio_input_capture", "coreaudio_output_capture",
         "coreaudio_input_capture", "coreaudio_output_capture", "coreaudio_output_capture",
         "coreaudio_input_capture", "av_capture_input", "av_capture_input", "window_capture", "monitor_capture"])),
    "win": dict(zip(
        ["text_ft2_source", "syphon-input", "coreaudio_input_capture", "coreaudio_output_capture",
         "pulse_input_capture", "pulse_output_capture", "jack_output_capture", "alsa_input_capture",
         "av_capture_input", "v4l2_input", "xcomposite_input"],
        ["text_gdiplus", "game_capture", "wasapi_input_capture", "wasapi_output_capture",
         "wasapi_input_capture", "wasapi_output_capture", "wasapi_output_capture",
         "wasapi_input_capture", "dshow_input", "dshow_input", "window_capture"])),
    "nix": dict(zip(
        ["text_gdiplus", "coreaudio_input_capture", "coreaudio_output_capture",
         "wasapi_input_capture", "wasapi_output_capture", "av_capture_input", "dshow_input"],
        ["text_ft2_source", "pulse_input_capture", "pulse_output_capture", "pulse_input_capture",
         "pulse_output_capture", "v4l2_input", "v4l2_input"])),
}


class ImporterFacadeTest(unittest.TestCase):
    def test_shared_files_have_no_host_decisions(self):
        for name in ("importers/classic.cpp.hpp", "importers/sl.cpp.hpp", "importers/studio.cpp.hpp",
                     "importers/xsplit.cpp.hpp", "importers/importers.hpp", "build/frontend-importers.pch.hpp"):
            with self.subTest(file=name):
                self.assertEqual(ratchet.count_platform_conditionals((ROOT / "frontend" / name).read_text()), [])

    def test_each_platform_defines_the_operations(self):
        header = (ROOT / "frontend/utility/platform.hpp").read_text()
        for operation in OPERATIONS:
            self.assertIn(operation + "(", header)
            for host in ("x11.cpp", "windows.cpp", "osx.mm"):
                with self.subTest(operation=operation, host=host):
                    self.assertRegex((ROOT / f"frontend/utility/platform-{host}.hpp").read_text(),
                                     rf"\b{operation}\([^;]*?\)\s*\{{")

    def test_classic_search_with_strict_native_warnings(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable; native builds still required")
        text = (ROOT / "frontend/importers/classic.cpp.hpp").read_text()
        body = function(text, "OBSImporterFiles ClassicImporter::FindFiles(")
        code = r"""
#include <string>
#include <vector>
#include <cstring>
using namespace std;
using OBSImporterFiles = vector<string>;
struct ClassicImporter { OBSImporterFiles FindFiles(); };
struct os_dir_t { size_t index = 0; };
struct os_dirent { bool directory; const char *d_name; };
os_dirent entries[] = {{false, "scene.xconfig"}, {false, ".hidden.xconfig"},
                      {true, "directory.xconfig"}, {false, "scene.xconfig.more"}, {false, "other"}};
bool GetPlatformClassicImportDirectory(char *path, size_t size) {
    if (size < 11) return false;
    strcpy(path, "/profiles/"); return true;
}
os_dir_t directory;
os_dir_t *os_opendir(const char *) { return &directory; }
os_dirent *os_readdir(os_dir_t *dir) {
    return dir->index < sizeof(entries) / sizeof(entries[0]) ? &entries[dir->index++] : nullptr;
}
void os_closedir(os_dir_t *) {}
""" + body + r"""
int main() {
    return ClassicImporter().FindFiles() == OBSImporterFiles{"/profiles/scene.xconfig"} ? 0 : 1;
}
"""
        with tempfile.TemporaryDirectory(prefix="obs-classic-search-") as tmp:
            source, binary = Path(tmp) / "probe.cpp", Path(tmp) / "probe"
            source.write_text(code)
            result = subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                                     str(source), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(binary)], check=True)

    def test_search_directory_policy_and_error_results(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable; native builds still required")
        for host, enabled in (("windows.cpp", (True, True, True)),
                              ("osx.mm", (False, True, False)),
                              ("x11.cpp", (False, False, False))):
            with self.subTest(host=host):
                text = (ROOT / f"frontend/utility/platform-{host}.hpp").read_text()
                names = ("Classic", "Streamlabs", "XSplit")
                locations = ("OBS\\sceneCollection\\", "slobs-client/SceneCollections/",
                             "SplitMediaLabs\\XSplit\\Presentation2.0\\")
                # Escape trailing Windows separators without changing the
                # runtime strings expected from the original implementation.
                preamble = """
#include <string>
#include <cstddef>
int status, calls, kind; size_t suppliedSize; std::string requested;
int record(char *directory, size_t size, const char *path, int type) {
    ++calls; suppliedSize = size; requested = path; kind = type;
    directory[0] = 'X'; return status;
}
int os_get_config_path(char *d, size_t n, const char *p) { return record(d, n, p, 1); }
int os_get_program_data_path(char *d, size_t n, const char *p) { return record(d, n, p, 2); }
"""
                bodies = "\n".join(function(text, f"bool GetPlatform{name}ImportDirectory(") for name in names)
                checks = "int main() { char directory[17];\n"
                for name, supported, location, api in zip(names, enabled, locations, (1, 1, 2)):
                    checks += f"""
for (int code : {{-1, 0, 23}}) {{
    status = code; calls = 0; directory[0] = 'U';
    bool result = GetPlatform{name}ImportDirectory(directory, sizeof(directory));
    if (result != ({str(supported).lower()} && code != -1)) return 1;
    if (calls != {int(supported)}) return 2;
    if ({str(supported).lower()} && (suppliedSize != sizeof(directory) || requested != {json.dumps(location)} || kind != {api} || directory[0] != 'X')) return 3;
    if (!{str(supported).lower()} && directory[0] != 'U') return 4;
}}
"""
                checks += "}"
                with tempfile.TemporaryDirectory(prefix="obs-importer-directory-") as tmp:
                    source, binary = Path(tmp) / "probe.cpp", Path(tmp) / "probe"
                    source.write_text(preamble + bodies + checks)
                    subprocess.run([compiler, "-std=c++17", str(source), "-o", str(binary)],
                                   check=True, capture_output=True)
                    subprocess.run([str(binary)], check=True)

    def test_legacy_paths_and_translation_matrix(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable; native builds still required")
        header = (ROOT / "frontend/importers/importers.hpp").read_text()
        paths = ["", "file.txt", "dir/file.txt", r"dir\file.txt", r"a/b\file.txt",
                 r"a\b/file.txt", "/a.b/file", "/a/.hidden", "dir/", r"C:\a\b.ext"]
        ids = sorted(set().union(*(mapping.keys() for mapping in MAPPINGS.values())) |
                     {"unknown", "window_capture", "monitor_capture"})
        for host, kind, macro in (("windows.cpp", "win", "_WIN32"),
                                  ("osx.mm", "mac", "__APPLE__"),
                                  ("x11.cpp", "nix", "__linux__")):
            with self.subTest(host=host):
                text = (ROOT / f"frontend/utility/platform-{host}.hpp").read_text()
                # Compile the real platform function and the shared path
                # wrappers against the same expected results recorded in RED.
                bodies = [function(text, "size_t PlatformImporterLastSeparator(")]
                bodies += [function(header, "static inline std::string " + name + "(")
                           for name in ("GetFilenameFromPath", "GetFolderFromPath")]
                bodies.append(function(text, "void TranslatePlatformImportedSource("))
                preamble = r'''
#include <string>
#include <iostream>
#include "json11.hpp"
using namespace std;
using namespace json11;
constexpr int OBS_NIX_PLATFORM_X11_EGL = 1;
int session = 1;
int obs_get_nix_platform() { return session; }
const char *obs_get_latest_input_type_id(const char *id) {
    static string out; out = string("latest:") + id; return out.c_str();
}
'''
                # Host macros apply only to the extracted legacy functions,
                # never to the standard library or json11 build.
                code = preamble + f"\n#define {macro} 1\n" + "\n".join(bodies) + r'''
int main() {
    string line;
    while (getline(cin, line)) {
        string error; Json input = Json::parse(line, error);
        session = input["session"].int_value();
        if (input["path"].is_string()) {
            string p = input["path"].string_value();
            cout << Json(Json::array{GetFilenameFromPath(p), GetFolderFromPath(p)}).dump() << '\n';
        } else {
            Json source = input["source"]; TranslatePlatformImportedSource(source);
            cout << source.dump() << '\n';
        }
    }
}
'''
                requests, expected = [], []
                for path in paths:
                    slash, back = path.rfind('/'), path.rfind('\\')
                    # Preserve the Windows npos comparison quirk: when '/' is
                    # absent it wins over a real backslash position.
                    pos = max(slash, back) if kind == "win" and slash != -1 else slash
                    ext = path.rfind('.')
                    filename = path[pos + 1:] if ext < pos or pos == -1 else path[pos + 1:ext]
                    requests.append({"path": path}); expected.append([filename, path[:pos + 1]])
                for session in (1, 2):
                    for id_ in ids:
                        for settings in ({}, {"keep": 7, "capture_cursor": False},
                                         {"keep": 7, "show_cursor": False, "capture_cursor": True}):
                            source = {"id": id_, "versioned_id": "original", "settings": settings,
                                      "other": "preserved"}
                            out = json.loads(json.dumps(source)); mapping = dict(MAPPINGS[kind])
                            if kind == "nix":
                                mapping.update({x: "xcomposite_input" if session == 1 else
                                                "pipewire-screen-capture-source" for x in ("game_capture", "window_capture")})
                            if id_ in mapping:
                                out.update(id=mapping[id_], versioned_id="latest:" + mapping[id_])
                            if kind == "win" and id_ in ("monitor_capture", "xshm_input"):
                                out["id"] = "monitor_capture"
                                out["settings"].setdefault("show_cursor", None)
                                if settings.get("show_cursor") is not None:
                                    out["settings"]["capture_cursor"] = settings["show_cursor"]
                            elif kind != "win" and id_ == "monitor_capture":
                                if kind == "nix":
                                    out["id"] = "xshm_input"
                                out["settings"].setdefault("show_cursor", None)
                                if settings.get("show_cursor") is None:
                                    out["settings"].setdefault("capture_cursor", None)
                                if settings.get("show_cursor") is None and settings.get("capture_cursor") is not None:
                                    out["settings"]["show_cursor"] = settings["capture_cursor"]
                            requests.append({"session": session, "source": source}); expected.append(out)
                with tempfile.TemporaryDirectory(prefix="obs-importer-policy-") as tmp:
                    source, binary = Path(tmp) / "probe.cpp", Path(tmp) / "probe"
                    source.write_text(code)
                    subprocess.run([compiler, "-std=c++17", "-I", str(ROOT / "deps/json11"),
                                    str(source), str(ROOT / "deps/json11/json11.cpp"), "-o", str(binary)],
                                   check=True, capture_output=True)
                    result = subprocess.run([str(binary)], input="\n".join(map(json.dumps, requests)) + "\n",
                                            text=True, capture_output=True, check=True)
                    actual = [json.loads(line) for line in result.stdout.splitlines()]
                    self.assertEqual(len(actual), len(expected))
                    for request, want, got in zip(requests, expected, actual):
                        with self.subTest(host=host, request=request):
                            self.assertEqual(got, want)


if __name__ == "__main__":
    unittest.main()
