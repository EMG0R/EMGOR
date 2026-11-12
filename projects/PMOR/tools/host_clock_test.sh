#!/usr/bin/env bash
# Host-only measurement of the clock-agent changes (no board, no audio). Run from anywhere.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$here/.."
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
c++ -std=gnu++14 -O1 -Wall -Wno-misleading-indentation -I"$here/hoststub" -I"$root" -I"$root/pmor_core" -I"$root/pmor_core/third_party" \
    -o "$tmp/host_clock_test" "$here/host_clock_test.cpp" "$root/pmor_core/clock.cpp" "$root/pmor_core/sequencer.cpp" "$root/pmor_core/engine_util.cpp" "$root/pmor_core/host/platform_host.cpp"
"$tmp/host_clock_test"
