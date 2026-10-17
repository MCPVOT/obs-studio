#!/bin/bash
# Refuse an empty timing cache: both implementations must have measured tests.
# Usage: bash check-costs.sh COST_DIR
set -euo pipefail
directory=${1:?usage: check-costs.sh COST_DIR}
for mode in OFF ON; do
  file="$directory/ctest-cost-$mode.txt"
  if [ ! -s "$file" ]; then
    echo "Missing Tier 2 timing data for $mode: $file" >&2
    exit 1
  fi
  count=$(awk '
    $0 == "---" { exit }
    NF == 3 && $2 ~ /^[0-9]+$/ && $2 > 0 && $3 ~ /^[0-9]+([.][0-9]+)?([eE][-+]?[0-9]+)?$/ { n++ }
    END { print n + 0 }
  ' "$file")
  if [ "$count" -eq 0 ]; then
    echo "No measured Tier 2 tests for $mode: $file" >&2
    exit 1
  fi
  echo "$mode: $count measured tests ($(wc -c <"$file") bytes)"
done
