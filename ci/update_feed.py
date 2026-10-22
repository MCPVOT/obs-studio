#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["cryptography>=43,<47"]
# ///
"""Write the in-app update feed for a certified OBS-Studio-Rust release.

    uv run --script ci/update_feed.py check-certifiable --facts facts.json \\
        --min-age-days 2
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
from datetime import datetime, timezone
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


PUBLISH_JOB = "Publish pre-release"
CERTIFY_JOB = "Certify release"


def parse_time(value: str) -> datetime:
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def skippable(job: dict) -> bool:
    """A skip that a successful release run has by design.

    The certify job skips on every daily run, and a reusable workflow's own
    jobs ("Release gate 🚦 / Flatpak 📦", "AppImage 📦 / Publish to GitHub
    Releases 🚀") skip by their own conditions, decided by their caller job
    which must itself succeed. A skipped top-level job (a build, a smoke test,
    the gate) means the release was not fully verified.
    """
    if job.get("conclusion") != "skipped":
        return False
    return job["name"].startswith(CERTIFY_JOB) or " / " in job["name"]


def check_certifiable(facts: dict, now: str, min_age_days: float) -> list[str]:
    """Raise FeedError unless facts describe a certifiable release; return warnings.

    facts (the certify job assembles it with `gh api`): tag, tag_sha (the
    commit the tag names), release (isDraft, isPrerelease, publishedAt) and
    runs, the release-rust runs for tag_sha, each with id, conclusion and
    jobs [{name, conclusion}]. The run that published the tag is the newest
    one whose "Publish pre-release" job succeeded; it must have concluded
    success, and every one of its jobs must have succeeded (see skippable()).
    """
    tag = facts.get("tag", "")
    if not TAG_RE.match(tag):
        raise FeedError(f"{tag!r} is not an obs-studio-rust-v<version> release tag")
    release = facts["release"]
    if release.get("isDraft"):
        raise FeedError(f"{tag} is a draft; only a published daily release can be certified")
    if min_age_days < 0:
        raise FeedError(f"min_age_days must be 0 or more, not {min_age_days}")

    sha = facts["tag_sha"]
    published = [
        run
        for run in facts.get("runs", [])
        if run.get("head_sha") == sha
        and any(
            job["name"].startswith(PUBLISH_JOB) and job.get("conclusion") == "success" for job in run.get("jobs", [])
        )
    ]
    if not published:
        raise FeedError(f"no release-rust run for {sha[:9]} published {tag}")
    run = max(published, key=lambda r: r["id"])
    bad = [
        f"{job['name']} ({job.get('conclusion') or 'unfinished'})"
        for job in run.get("jobs", [])
        if job.get("conclusion") != "success" and not skippable(job)
    ]
    if bad or run.get("conclusion") != "success":
        detail = ", ".join(bad) or f"run concluded {run.get('conclusion') or 'unfinished'}"
        raise FeedError(f"release-rust run {run['id']} that published {tag} did not fully succeed: {detail}")

    age = parse_time(now) - parse_time(release["publishedAt"])
    days = age.total_seconds() / 86400
    if min_age_days == 0:
        return [f"min_age_days=0: certifying {tag} {days:.1f} days after publishing, skipping the age gate (emergency)"]
    if days < min_age_days:
        raise FeedError(
            f"{tag} is {days:.1f} days old; certify it once it is {min_age_days:g} days old"
            " (min_age_days, 0 for an emergency)"
        )
    return []


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    b = sub.add_parser("build")
    b.add_argument("--tag", required=True)
    b.add_argument("--assets", required=True, type=Path)
    b.add_argument("--mac-info", required=True, type=Path)
    b.add_argument("--rsa-key", required=True, type=Path)
    b.add_argument("--out", required=True, type=Path)
    c = sub.add_parser("check-certifiable")
    c.add_argument("--facts", required=True, type=Path)
    c.add_argument("--min-age-days", required=True, type=float)
    c.add_argument("--now", default=None, help="ISO time (default: now)")
    args = parser.parse_args(argv)
    if args.command == "check-certifiable":
        now = args.now or datetime.now(timezone.utc).isoformat()
        try:
            warnings = check_certifiable(json.loads(args.facts.read_text(encoding="utf-8")), now, args.min_age_days)
        except (FeedError, OSError, ValueError, KeyError) as error:
            print(f"::error::update_feed: {error}")
            return 1
        for warning in warnings:
            print(f"::warning::{warning}")
        print("update_feed: the release is certifiable")
        return 0
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
