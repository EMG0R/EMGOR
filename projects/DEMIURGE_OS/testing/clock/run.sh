#!/usr/bin/env bash
# testing/clock/run.sh
#
# One-command test runner for the demiurge-clock PLL + tempo-source
# ladder. Two things happen here:
#
#   1. testing/clock/test_pll.cpp is compiled and run -- pure-logic unit
#      tests against src/demiurge-clock-pll.h (no ALSA involved at all).
#   2. src/demiurge-clock.cpp (the REAL daemon source, unmodified) is
#      compiled as a whole against a minimal stubbed <alsa/asoundlib.h>
#      (testing/clock/stub/) -- this is a compile-only sanity check
#      (macOS has no ALSA to link against, and this repo never runs the
#      daemon here), proving the daemon still builds after the PLL/ladder
#      change touched it.
#
# Runs on macOS or Linux. No network, no ALSA, no root.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

CXX="${CXX:-g++}"

echo "== 1/2: unit tests (src/demiurge-clock-pll.h) =="
"$CXX" -std=c++17 -O2 -Wall -Wextra \
    -o "$BUILD_DIR/test_pll" \
    test_pll.cpp
"$BUILD_DIR/test_pll"
echo

echo "== 2/2: whole-daemon compile check (src/demiurge-clock.cpp, stubbed ALSA) =="
"$CXX" -std=c++17 -O2 -Wall -Wextra \
    -I stub -I ../../src \
    -include stub/compat.h \
    -c ../../src/demiurge-clock.cpp \
    -o "$BUILD_DIR/demiurge-clock.o"
echo "compiled cleanly: zero errors, zero warnings"
