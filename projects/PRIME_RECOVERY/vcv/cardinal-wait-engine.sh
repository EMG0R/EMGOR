#!/bin/bash
# ExecStartPre gate for cardinal.service. Read-only: only LISTS the graph (pw-link -l); never links.
# Go when csound6 exists and its main output is linked into a HAT playback port.
# Pinned light: this runs once per (re)start, polls every 2 s.
for i in $(seq 1 60); do
  out="$(pw-link -l 2>/dev/null)"
  if grep -q '^csound6:output1' <<<"$out" && grep -A8 '^csound6:output1' <<<"$out" | grep -q 'playback_AUX0'; then
    exit 0
  fi
  sleep 2
done
echo "cardinal-wait-engine: csound6 not linked to HAT after 120 s" >&2
exit 1
