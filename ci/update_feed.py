#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["cryptography>=43,<47"]
# ///
"""Write the in-app update feed for a certified OBS-Studio-Rust release.

    uv run --script ci/update_feed.py build --tag obs-studio-rust-v<version> \\
        --assets <dir> --mac-info mac.json --rsa-key key.pem --out feed

release-rust.yaml's certify job runs this, uploads `feed/*` as assets of the
release, and only then makes it a full release marked Latest. The apps read
https://github.com/obs-rust/obs-studio/releases/latest/download/<file>, which
GitHub resolves to the newest full release, never a pre-release (daily beta).

<dir> holds the release's assets and its SHA256SUMS; every file used is checked
against SHA256SUMS. mac.json gives, per macOS arch (arm64, x86_64), the DMG
app's bundle_version (CFBundleVersion), short_version, minimum_system_version
and the Sparkle ed_signature from `sign_update` (the EdDSA key stays with
Sparkle's own tool). Outputs:

- updates_<arch>_v2.xml: Sparkle appcast, one item, enclosure on the tag's DMG.
- manifest.json (+ .sig): the Windows updater's manifest: version, tag, notes
  and, per arch (x64, arm64), the ZIP's url, sha256 and size.
- branches.json (+ .sig): no extra branches; the app always adds "stable".

.sig files are hex RSA PKCS#1 v1.5 SHA-512 signatures, as
frontend/utility/WhatsNewInfoThread.cpp.hpp's FetchAndVerifyFile checks them
against frontend/data/OBSPublicRSAKey.pem.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import re
import sys
from pathlib import Path
from xml.sax.saxutils import quoteattr

REPO = "https://github.com/obs-rust/obs-studio"
TAG_RE = re.compile(r"^obs-studio-rust-v(\d+\.\d+\.\d+(?:-[0-9A-Za-z.]+)?)$")
MAC_ARCHES = ("arm64", "x86_64")
WINDOWS_ARCHES = ("x64", "arm64")


class FeedError(Exception):
    pass


def read_sums(path: Path) -> dict[str, str]:
    sums = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.strip():
            digest, name = line.split(None, 1)
            sums[name.lstrip("*").strip()] = digest.lower()
    return sums


def checked(assets: Path, sums: dict[str, str], name: str) -> Path:
    path = assets / name
    if not path.is_file():
        raise FeedError(f"{name} is not among the release assets")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if sums.get(name) != digest:
        raise FeedError(f"{name} does not match SHA256SUMS")
    return path


def appcast(tag: str, version: str, dmg: Path, mac: dict) -> str:
    url = f"{REPO}/releases/download/{tag}/{dmg.name}"
    notes = f"{REPO}/releases/tag/{tag}"
    return f"""<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0" xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
  <channel>
    <title>OBS-Studio-Rust</title>
    <link>{REPO}</link>
    <item>
      <title>OBS-Studio-Rust {html.escape(version)}</title>
      <sparkle:version>{html.escape(str(mac["bundle_version"]))}</sparkle:version>
      <sparkle:shortVersionString>{html.escape(str(mac["short_version"]))}</sparkle:shortVersionString>
      <sparkle:minimumSystemVersion>{html.escape(str(mac["minimum_system_version"]))}</sparkle:minimumSystemVersion>
      <sparkle:fullReleaseNotesLink>{notes}</sparkle:fullReleaseNotesLink>
      <enclosure url={quoteattr(url)} length="{dmg.stat().st_size}" type="application/octet-stream" sparkle:edSignature={quoteattr(str(mac["ed_signature"]))}/>
    </item>
  </channel>
</rss>
"""


def sign(key_pem: bytes, data: bytes) -> str:
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import padding

    key = serialization.load_pem_private_key(key_pem, password=None)
    return key.sign(data, padding.PKCS1v15(), hashes.SHA512()).hex()


def build(tag: str, assets: Path, mac_info: dict, key_pem: bytes, out: Path) -> None:
    match = TAG_RE.match(tag)
    if not match:
        raise FeedError(f"{tag!r} is not an obs-studio-rust-v<version> release tag")
    version = match.group(1)
    sums = read_sums(assets / "SHA256SUMS")

    files: dict[str, bytes] = {}
    for arch in MAC_ARCHES:
        if arch not in mac_info:
            raise FeedError(f"mac info has no {arch} entry")
        dmg = checked(assets, sums, f"OBS-Studio-Rust-{version}-macos-{arch}.dmg")
        files[f"updates_{arch}_v2.xml"] = appcast(tag, version, dmg, mac_info[arch]).encode()

    windows = {}
    for arch in WINDOWS_ARCHES:
        name = f"OBS-Studio-Rust-{version}-windows-{arch}.zip"
        path = checked(assets, sums, name)
        windows[arch] = {
            "name": name,
            "url": f"{REPO}/releases/download/{tag}/{name}",
            "sha256": sums[name],
            "size": path.stat().st_size,
        }
    notes_url = f"{REPO}/releases/tag/{tag}"
    manifest = {
        "version": version,
        "tag": tag,
        "notes": f'<p>OBS-Studio-Rust {html.escape(version)}</p><p><a href="{notes_url}">{notes_url}</a></p>',
        "windows": windows,
    }
    files["manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    files["branches.json"] = b"[]\n"
    for name in ("manifest.json", "branches.json"):
        files[f"{name}.sig"] = (sign(key_pem, files[name]) + "\n").encode()

    out.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (out / name).write_bytes(data)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    b = sub.add_parser("build")
    b.add_argument("--tag", required=True)
    b.add_argument("--assets", required=True, type=Path)
    b.add_argument("--mac-info", required=True, type=Path)
    b.add_argument("--rsa-key", required=True, type=Path)
    b.add_argument("--out", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        build(
            args.tag,
            args.assets,
            json.loads(args.mac_info.read_text(encoding="utf-8")),
            args.rsa_key.read_bytes(),
            args.out,
        )
    except (FeedError, OSError, ValueError, KeyError) as error:
        print(f"update_feed: {error}", file=sys.stderr)
        return 1
    print(f"update_feed: wrote {args.tag} feed to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
