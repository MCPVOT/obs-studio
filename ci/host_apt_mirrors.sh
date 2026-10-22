#!/usr/bin/env bash
# Host-only mirror preference; no package or repository changes.
set -euo pipefail

if (( $# > 1 )); then
    echo 'usage: host_apt_mirrors.sh [mirror-list]' >&2
    exit 2
fi
mirrors=${1:-/etc/apt/apt-mirrors.txt}
if [[ ! -f "$mirrors" || -L "$mirrors" ]]; then
    echo 'host apt: keeping missing or linked mirror configuration'
    exit 0
fi

# Recognize only the runner-images configure-apt-sources.sh layout. Additional
# metadata, duplicate known URIs and other image layouts retain their policy.
azure='http://azure\.archive\.ubuntu\.com/ubuntu/'
archive='https://archive\.ubuntu\.com/ubuntu/'
if [[ $(grep -Ec "^${azure}([[:blank:]]|$)" -- "$mirrors") == 1 ]] \
    && [[ $(grep -Ec "^${archive}([[:blank:]]|$)" -- "$mirrors") == 1 ]] \
    && grep -Eq "^${azure}[[:blank:]]+priority:1$" -- "$mirrors" \
    && grep -Eq "^${archive}[[:blank:]]+priority:2$" -- "$mirrors"; then
    # Lower priority numbers are tried first. Keep Azure and security fallback
    # routes; only promote the HTTPS archive already trusted by this image.
    sed -i -E \
        -e "s@^(${azure}[[:blank:]]+)priority:1\$@\1priority:2@" \
        -e "s@^(${archive}[[:blank:]]+)priority:2\$@\1priority:1@" -- "$mirrors"
    echo 'host apt: preferring existing HTTPS Ubuntu archive; fallbacks retained'
else
    echo 'host apt: keeping existing or unrecognized mirror priorities'
fi
