#!/usr/bin/env bash
# Host measurement of the REAL usb/clock_engine.c MIDI-clock FOLLOWER + the REAL pmor_core/clock.cpp grid (no board, no audio).
# usage: bash tools/follower_sim.sh [--assert-baseline]
# FOLLOWER_SRC=/path/to/patched_clock_engine.c bash tools/follower_sim.sh   (A/B a candidate fix without touching usb/)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; root="$here/.."
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
inc=(-I"$here/followerstub" -I"$here/hoststub" -I"$root/usb" -I"$root" -I"$root/pmor_core" -I"$root/pmor_core/third_party")
cc  -std=gnu11 -O1 -w "${inc[@]}" -c "${FOLLOWER_SRC:-$root/usb/clock_engine.c}" -o "$tmp/clock_engine.o"
c++ -std=gnu++14 -O1 -Wall -Wno-misleading-indentation "${inc[@]}" -c "$here/follower_sim.cpp" -o "$tmp/sim.o"
c++ -std=gnu++14 -O1 -w "${inc[@]}" -c "$root/pmor_core/clock.cpp" -o "$tmp/clock.o"
c++ -std=gnu++14 -O1 -w "${inc[@]}" -c "$root/pmor_core/host/platform_host.cpp" -o "$tmp/plat.o"
c++ -std=gnu++14 -O1 -w "${inc[@]}" -c "$root/hw/transport_link.cpp" -o "$tmp/tl.o"
c++ -o "$tmp/follower_sim" "$tmp"/*.o
"$tmp/follower_sim" "$@"
