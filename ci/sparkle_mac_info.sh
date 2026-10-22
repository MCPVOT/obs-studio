#!/usr/bin/env bash
# Write the macOS half of an update feed's input (ci/update_feed.py --mac-info).
#
#   ci/sparkle_mac_info.sh <dist> <sparkle-ed-key-file> <sign_update> <out.json>
#
# For each DMG in <dist> (*-macos-arm64.dmg, *-macos-x86_64.dmg) it reads the
# app's CFBundleVersion, CFBundleShortVersionString and LSMinimumSystemVersion
# and signs the DMG with Sparkle's sign_update (EdDSA). macOS only (hdiutil,
# plutil). release-rust.yaml's certify (stable feed) and nightly_feed jobs
# share it (#1742, #1759).
set -euo pipefail
dist=$1 key=$2 sign_update=$3 out=$4
echo '{}' >"$out"
for arch in arm64 x86_64; do
  dmg=$(ls "$dist"/*-macos-"$arch".dmg)
  mnt=$(mktemp -d)
  hdiutil attach -nobrowse -readonly -mountpoint "$mnt" "$dmg" >/dev/null
  plist=$(ls -d "$mnt"/*.app)/Contents/Info.plist
  bundle=$(plutil -extract CFBundleVersion raw -o - "$plist")
  short=$(plutil -extract CFBundleShortVersionString raw -o - "$plist")
  minsys=$(plutil -extract LSMinimumSystemVersion raw -o - "$plist")
  hdiutil detach "$mnt" >/dev/null
  sig=$("$sign_update" --ed-key-file "$key" "$dmg" |
    sed -n 's/.*sparkle:edSignature="\([^"]*\)".*/\1/p')
  [ -n "$sig" ] || { echo "::error::sign_update produced no signature for $dmg"; exit 1; }
  jq --arg a "$arch" --arg b "$bundle" --arg s "$short" --arg m "$minsys" --arg e "$sig" \
    '.[$a] = {bundle_version: $b, short_version: $s, minimum_system_version: $m, ed_signature: $e}' \
    "$out" >"$out.new" && mv "$out.new" "$out"
done
