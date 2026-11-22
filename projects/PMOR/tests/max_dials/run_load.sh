#!/usr/bin/env bash
# run_load.sh -- headless Max 9: load every saved device patcher (max/devices/PMOR *.maxpat) and read Max's own console log
# (~/Library/Application Support/Cycling '74/Max 9/Logs/Max.log): zero "patchcord inlet/outlet out of range", missing objects or js errors.
# Also asserts pmor_where.js reported its resolved values to the console (the console path is alive). SKIP without Max.app.
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$here/../.."
[ -d /Applications/Max.app ] || { echo "SKIP: no Max.app"; exit 0; }
. "$here/maxrun.sh"
LOGD="$HOME/Library/Application Support/Cycling '74/Max 9/Logs"
w="$(cd "$(mktemp -d /tmp/pmor_load.XXXXXX)" && pwd -P)"; trap 'rm -rf "$w"' EXIT
cp "$root"/max/devices/*.maxpat "$root"/max/devices/*.js "$root/max/devices/schema.json" "$w/"
fail=0
for f in "$w"/PMOR*.maxpat; do
  n="$(basename "$f" .maxpat)"; t0="$(date '+%Y-%m-%d %H:%M:%S')"
  maxrun_open "$f" 7
  out="$(cat "$LOGD/Max.1.log" "$LOGD/Max.log" 2>/dev/null | awk -v t="$t0" 'substr($0,2,19) >= t')"
  bad="$(echo "$out" | grep -E ' error\] ' | grep -E 'out of range|patchcord|no such object|couldn.t|can.t find|Javascript|error calling|doesn.t understand|extra arguments|bad argument' )"
  if [ -n "$bad" ]; then echo "  FAIL $n:"; echo "$bad" | sed 's/^/       /' | head -5; fail=$((fail+1)); else echo "  ok   $n loads clean"; fi
  if [ "$n" = "PMOR core" ]; then echo "$out" | grep -q 'pmor_where: device=' && echo "  ok   core console shows pmor_where values" || { echo "  FAIL core: no pmor_where console line"; fail=$((fail+1)); }; fi
done
[ $fail -eq 0 ] && echo "max_load: PASS" || { echo "max_load: $fail FAIL"; exit 1; }
