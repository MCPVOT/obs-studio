#!/bin/bash
# Run before flatpak-builder: its build sandbox has no network access.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .flatpak-rust/cargo
# A cold soldr bootstrap can print rustup's welcome message to stdout before
# Cargo's configuration. Keep the raw output separate from the TOML input.
soldr cargo vendor --locked .flatpak-rust/vendor > .flatpak-rust/vendor-config.stdout
uv run --no-project python - <<'PY'
import re
import tomllib
from pathlib import Path

output = Path(".flatpak-rust/vendor-config.stdout").read_text()
start = output.find("[source.")
if start < 0:
    raise SystemExit("cargo vendor did not emit source configuration")
config = re.sub(r'^directory = ".*"$', 'directory = "/run/build/obs/.flatpak-rust/vendor"',
                output[start:], flags=re.MULTILINE)
parsed = tomllib.loads(config)
if parsed["source"]["vendored-sources"]["directory"] != "/run/build/obs/.flatpak-rust/vendor":
    raise SystemExit("cargo vendor configuration does not select the sandbox vendor directory")
Path(".flatpak-rust/cargo/config.toml").write_text(config)
PY
