#!/usr/bin/env -S uv run --script
"""Canonical AppImage names and updater metadata (#624)."""

from pathlib import Path


def asset_name(version_output: str) -> str:
    raise NotImplementedError("RED: derive the asset name from the executable identity")


def check_metadata(appimage: Path, version_output: str, update_info: str, repository: str, require_update=False):
    raise NotImplementedError("RED: reject inconsistent updater assets")
