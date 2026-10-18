#!/usr/bin/env -S uv run --script
"""Enforce one Rust test executable per selected OBS category.

Manifest discovery counts unit harnesses as well as integration executables.

    uv run ci/test_binary_cap.py --report
    uv run python -m unittest ci/test_binary_cap.py
"""

from __future__ import annotations

import sys
import tomllib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CATEGORY_BINARIES = frozenset(
    f"obs-tests-{category}"
    for category in ("util", "core", "graphics", "media", "codec", "gpu", "preflight", "app")
)

# Keep private tests inside their owning library harness: no second executable
# and no test-only production API. Other categories use integration targets.
CATEGORY_OWNERS = {
    ("obs-util", "obs_util", "lib"): "util",
    ("obs-media-io", "obs_media_io", "lib"): "media",
    ("obs-data", "obs-tests-core", "test"): "core",
    ("obs-graphics", "obs-tests-graphics", "test"): "graphics",
    ("obs-nvenc", "obs-tests-codec", "test"): "codec",
    ("obs-gpu-status", "obs-tests-gpu", "test"): "gpu",
    ("obs-preflight", "obs-tests-preflight", "test"): "preflight",
    ("obs-studio-rust", "obs-tests-app", "test"): "app",
}


def cargo_test_targets(metadata: dict) -> list[tuple[str, str, str]]:
    """Use Cargo discovery, including empty unit harnesses and custom tests.

    Metadata inventories all platform targets conservatively; required features
    do not exempt a target from the budget. Examples and benchmarks are separate.
    """
    members = set(metadata["workspace_members"])
    targets = []
    for package in metadata["packages"]:
        if package["id"] not in members:
            continue
        for target in package["targets"]:
            kinds = set(target["kind"])
            if kinds & {"example", "bench", "custom-build"}:
                continue
            if "test" in kinds or target.get("test", False):
                targets.append((package["name"], target["name"], ",".join(sorted(kinds))))
    return sorted(targets)


def category_violations(targets: list[tuple[str, str, str]]) -> list[str]:
    errors = []
    seen = set()
    categories = set()
    for package, name, kind in targets:
        category = CATEGORY_OWNERS.get((package, name, kind))
        if category is not None:
            categories.add(category)
        if (package, name, kind) not in CATEGORY_OWNERS:
            errors.append(
                f"{package}:{name} ({kind}): consolidate coverage into a selected category binary; "
                "disable the old unit harness only after preserving its tests"
            )
        if name in seen:
            errors.append(f"{name}: duplicate category binary across workspace targets")
        seen.add(name)
    if len(targets) > len(CATEGORY_BINARIES):
        errors.append(f"workspace: {len(targets)} test binaries (category budget {len(CATEGORY_BINARIES)})")
    for missing in sorted(set(CATEGORY_OWNERS.values()) - categories):
        errors.append(f"{missing}: missing category test binary; preserve its coverage")
    return errors


def workspace_manifests(root: Path) -> list[Path]:
    root = root.resolve()
    document = tomllib.loads((root / "Cargo.toml").read_text())
    workspace = document["workspace"]
    excluded = {path.resolve() for pattern in workspace.get("exclude", []) for path in root.glob(pattern)}
    pending = [path.resolve() for pattern in workspace.get("members", []) for path in root.glob(pattern)]
    if "package" in document:
        pending.append(root)
    seen = set()
    manifests = []
    while pending:
        crate = pending.pop()
        manifest = crate / "Cargo.toml"
        if crate in seen or crate in excluded or not crate.is_relative_to(root) or not manifest.is_file():
            continue
        seen.add(crate)
        data = tomllib.loads(manifest.read_text())
        if crate != root and "workspace" in data:
            continue
        manifests.append(manifest)
        scopes = [data, *data.get("target", {}).values()]
        for scope in scopes:
            for kind in ("dependencies", "dev-dependencies", "build-dependencies"):
                for name, entry in scope.get(kind, {}).items():
                    base = crate
                    if isinstance(entry, dict) and entry.get("workspace", False):
                        entry = workspace.get("dependencies", {}).get(name, {})
                        base = root
                    if isinstance(entry, dict) and "path" in entry:
                        pending.append((base / entry["path"]).resolve())
    return sorted(manifests)


def workspace_test_targets(root: Path = ROOT) -> list[tuple[str, str, str]]:
    """Read manifests without invoking a compiler or requiring soldr in Checks.

    Cargo metadata equivalence is tested separately on hosts with soldr.
    """
    targets = []
    for manifest_path in workspace_manifests(root):
        crate = manifest_path.parent
        manifest = tomllib.loads((crate / "Cargo.toml").read_text())
        package = manifest["package"]
        name = package["name"]
        library = manifest.get("lib", {})
        if ("lib" in manifest or package.get("autolib", True) and (crate / "src/lib.rs").exists()) and library.get("test", True):
            kinds = ",".join(sorted(library.get("crate-type", ["proc-macro"] if library.get("proc-macro", False) else ["lib"])))
            targets.append((name, library.get("name", name.replace("-", "_")), kinds))
        binaries = {entry["name"]: entry for entry in manifest.get("bin", [])}
        explicit_bins = {(crate / entry["path"]).resolve() for entry in binaries.values() if "path" in entry}
        if package.get("autobins", True):
            if (crate / "src/main.rs").exists() and (crate / "src/main.rs").resolve() not in explicit_bins:
                binaries.setdefault(name, {})
            for path in sorted((crate / "src/bin").glob("*.rs")):
                if path.resolve() not in explicit_bins:
                    binaries.setdefault(path.stem, {})
            for path in sorted((crate / "src/bin").glob("*/main.rs")):
                if path.resolve() not in explicit_bins:
                    binaries.setdefault(path.parent.name, {})
        for binary, entry in binaries.items():
            if entry.get("test", True):
                targets.append((name, binary, "bin"))
        targets.extend((name, target, "test") for target in test_targets(crate))
    return sorted(targets)


def test_targets(crate: Path) -> list[str]:
    """Names of the integration-test targets Cargo builds for `crate`."""
    manifest = tomllib.loads((crate / "Cargo.toml").read_text())
    names: dict[str, None] = {}
    explicit_paths = set()
    for entry in manifest.get("test", []):
        name = entry.get("name") or Path(entry.get("path", "")).stem
        names[name] = None
        if "path" in entry:
            explicit_paths.add((crate / entry["path"]).resolve())
    autotests = manifest.get("package", {}).get("autotests", True)
    tests = crate / "tests"
    if autotests and tests.is_dir():
        for path in sorted(tests.glob("*.rs")):
            if path.resolve() not in explicit_paths:
                names.setdefault(path.stem, None)
        for path in sorted(tests.glob("*/main.rs")):
            if path.resolve() not in explicit_paths:
                names.setdefault(path.parent.name, None)
    return list(names)



class BinaryCapTest(unittest.TestCase):
    def test_workspace_within_cap(self):
        errors = category_violations(workspace_test_targets())
        self.assertEqual(errors, [], "\n" + "\n".join(errors))


def main(argv: list[str]) -> int:
    targets = workspace_test_targets()
    if "--report" in argv:
        for package, name, kind in targets:
            print(f"{package}:{name} ({kind})")
        print(f"{len(targets)} total (category budget {len(CATEGORY_BINARIES)})")
    errors = category_violations(targets)
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
