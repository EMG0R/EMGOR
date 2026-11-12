#!/usr/bin/env bash
# testing/neptr-clock/run.sh
#
# One-command headless test of the csound-side clock arbiter
# (chase_bliss_clock.orc instr 100's loop-vs-external tempo authority
# rule, and the gkGLITCH_extBpmSeen latch regression). No audio device,
# no MIDI hardware -- see test_arbiter.csd's header for what's real vs.
# reimplemented and why.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

if ! command -v csound >/dev/null 2>&1; then
    echo "csound not found on PATH -- install it (e.g. 'brew install csound') to run this test." >&2
    exit 1
fi

# Locate the real NEPTR phase4 csound/ dir (chase_bliss_clock.orc lives
# there, not in this repo) -- default assumes it's a sibling checkout of
# DEMIURGE_OS, override with NEPTR_CSOUND_DIR if that's not true here.
# 2026-10: NEPTR's csound moved INTO this repo as neptr/csound (commit
# 0c7e007 "Rename DEMIURGE-NEPTR-UI to neptr/"). This default pointed at
# the old sibling-checkout layout and had been silently stale since then.
DEFAULT_NEPTR_DIR="$(cd ../.. && pwd)/neptr/csound"
NEPTR_CSOUND_DIR="${NEPTR_CSOUND_DIR:-$DEFAULT_NEPTR_DIR}"

if [ ! -f "$NEPTR_CSOUND_DIR/chase_bliss_clock.orc" ]; then
    echo "Could not find chase_bliss_clock.orc under: $NEPTR_CSOUND_DIR" >&2
    echo "Set NEPTR_CSOUND_DIR to the NEPTR phase4/csound directory and re-run." >&2
    exit 1
fi

echo "== csound-side clock arbiter test (test_arbiter.csd) =="
echo "   using $NEPTR_CSOUND_DIR"

TMP_CSD="$(mktemp -t neptr_test_arbiter_XXXX).csd"
trap 'rm -f "$TMP_CSD"' EXIT
sed -e "s|@CHASE_BLISS_ORC_PATH@|$NEPTR_CSOUND_DIR/chase_bliss_clock.orc|" \
    -e "s|@LOOP_IMPLIED_BPM_PATH@|$NEPTR_CSOUND_DIR/loop_implied_bpm.inc.orc|" \
    test_arbiter.csd > "$TMP_CSD"

OUT="$(csound "$TMP_CSD" 2>&1)" || true
echo "$OUT" | grep -E "^(PASS|FAIL):|checks passed|FAILURE|ALL PASSED"
echo

if echo "$OUT" | grep -q "^FAIL:"; then
    echo "$OUT" > /tmp/test_arbiter_full_output.log 2>/dev/null || true
    echo "FAILED -- full csound output saved to /tmp/test_arbiter_full_output.log"
    exit 1
fi

if ! echo "$OUT" | grep -q "ALL PASSED"; then
    echo "did not find 'ALL PASSED' marker -- csound may have exited before the summary instrument ran. Full output:"
    echo "$OUT"
    exit 1
fi

echo "== cross-file consistency: recency window constants =="
# CLOCK_SRC_RECENCY_WINDOW_S (src/demiurge-clock-pll.h) and
# iChaseBlissExtWindow (chase_bliss_clock.orc) MUST agree -- they are two
# independent implementations of "how long is an external tempo signal
# considered fresh", documented as intentionally matching in both files.
# A silent drift here would desync the C++ daemon's tempo-source ladder
# from the csound arbiter without either side's own tests catching it.
PLL_H="../../src/demiurge-clock-pll.h"
CPP_WINDOW="$(grep -oE '#define CLOCK_SRC_RECENCY_WINDOW_S [0-9.]+' "$PLL_H" | grep -oE '[0-9.]+$')"
ORC_WINDOW="$(grep -oE 'iChaseBlissExtWindow *= *[0-9.]+' "$NEPTR_CSOUND_DIR/chase_bliss_clock.orc" | head -1 | grep -oE '[0-9.]+$')"
echo "   demiurge-clock-pll.h CLOCK_SRC_RECENCY_WINDOW_S = ${CPP_WINDOW:-<not found>}"
echo "   chase_bliss_clock.orc iChaseBlissExtWindow       = ${ORC_WINDOW:-<not found>}"
if [ -z "${CPP_WINDOW:-}" ] || [ -z "${ORC_WINDOW:-}" ]; then
    echo "FAILED -- could not extract one or both recency-window constants" >&2
    exit 1
fi
if [ "$CPP_WINDOW" != "$ORC_WINDOW" ]; then
    echo "FAILED -- recency windows have drifted apart: C++ $CPP_WINDOW s vs csound $ORC_WINDOW s" >&2
    exit 1
fi
echo "   PASS: recency windows match ($CPP_WINDOW s)"
echo
echo "ALL PASSED"
