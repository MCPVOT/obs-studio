#!/bin/sh
# Usage: install-zccache.sh [DEST_DIR]
# Installs the pinned zccache release (static musl build) into DEST_DIR
# (default ~/.local/bin) after checking its SHA-256. It is the compiler
# launcher of every Linux CMake build here: Tier 2 (the Dockerfile next to
# this script), the Ubuntu build, the AppImage and the sanitizers. soldr
# 0.9.35 embeds zccache 1.15.4 for rustc; this standalone launcher gets
# the 1.15.5 PCH header-mtime replay fix independently of soldr.
set -eu
ZCCACHE_VERSION="1.15.5"
dest=${1:-"$HOME/.local/bin"}
case "$(uname -m)" in
x86_64 | amd64)
  arch=x86_64
  sha=39906c92b0287e2aac51ae9169b368d36eabc3af363c3c7bd29dc3783975e579
  ;;
aarch64 | arm64)
  arch=aarch64
  sha=a266bf8319d008708da870f49843bfc3a3ce2bb078be2c1448358584701b215a
  ;;
*)
  echo "install-zccache.sh: unsupported arch $(uname -m)" >&2
  exit 1
  ;;
esac
if [ -x "$dest/zccache" ] && "$dest/zccache" --version 2>/dev/null | grep -qx "zccache $ZCCACHE_VERSION"; then
  exit 0
fi
name="zccache-v$ZCCACHE_VERSION-$arch-unknown-linux-musl"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
curl -fsSL --retry 3 -o "$tmp/$name.tar.gz" \
  "https://github.com/zackees/zccache/releases/download/$ZCCACHE_VERSION/$name.tar.gz"
(cd "$tmp" && echo "$sha  $name.tar.gz" | sha256sum -c - >/dev/null)
tar -xzf "$tmp/$name.tar.gz" -C "$tmp"
mkdir -p "$dest"
install -m 0755 "$tmp/$name/zccache" "$dest/zccache"
"$dest/zccache" --version
