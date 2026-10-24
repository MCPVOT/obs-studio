#!/usr/bin/env bash
# Delete the objects ninja recorded with no header dependencies, so the next
# build compiles them again (#2244).
#   drop-depless-objects.sh <build-dir>
# Every C/C++ object includes at least one header, so "#deps 0" means ninja read
# an empty depfile. Such an object is never rebuilt after a header edit, and
# Tier 2 would test old code. No build tree yet: nothing to do.
set -euo pipefail
build="$1"
deps="$(ninja -C "$build" -t deps 2>/dev/null)" || exit 0
dropped=0
while IFS= read -r object; do
  rm -f -- "$build/$object"
  dropped=$((dropped + 1))
done < <(sed -n 's/^\([^ ][^:]*\): #deps 0,.*/\1/p' <<<"$deps")
if [ "$dropped" -gt 0 ]; then
  echo "== $dropped objects had no recorded header dependencies; removed so they are compiled again (#2244)"
fi
