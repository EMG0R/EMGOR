#!/bin/bash
#
# test-nam.sh — DEMIURGE NAM verification (run on the Pi). No audio hardware,
# no guitar needed. Catches exactly the failure modes that have bitten NAM:
#
#   TIER 1  symbol guard   (instant)  — the .so must export nam_kperf and NOT
#                                        nam_aperf. This is THE regression that
#                                        muted audio (CSound 6.18 never schedules
#                                        thread=5/aperf -> outputs stay zero).
#   TIER 2  functional      (~10 s)    — render a tone through the REAL plugin and
#                                        assert non-silent output that differs
#                                        from dry. Catches silence + opcode no-op
#                                        + model-not-loading. (nam_signal_test.csd)
#   TIER 2b channel isolation (~1 min) — render L twice (R same / R decorrelated);
#                                        L output must be identical both times.
#                                        Catches the shared-model-instance bug
#                                        (stateful models convolving one channel
#                                        against the other channel's history).
#                                        (nam_channel_isolation.csd)
#   TIER 3  integration     (manual)   — full demiurge chain via the probe harness
#                                        with NAM toggled on. Pointer only.
#
# Usage:  bash testing/nam/test-nam.sh        (NAM_SO=/path/to.so to override)
set -uo pipefail

# The dir csound actually loads; the old /usr/local path was never searched.
SO="${NAM_SO:-/usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/csound-nam.so}"
NAMLIBDIR="${NAMLIBDIR:-$HOME/demiurge/profiles/nam}"
HERE="$(cd "$(dirname "$0")" && pwd)"
CSD="$HERE/nam_signal_test.csd"
fail=0

echo "=== TIER 1: symbol guard ($SO) ==="
if [ ! -f "$SO" ]; then
  echo "  FAIL: .so not installed — build it: cd ~/NEPTR_phase4/csound/nam && cmake --build build -j1 && sudo cmake --install build"
  fail=1
else
  # capture first, match with shell globs — any grep -q in a pipeline here
  # SIGPIPEs the producer under pipefail (bit us on macOS; latent on Pi)
  SYMS="$(nm -C "$SO" 2>/dev/null)"
  case "$SYMS" in *nam_kperf*) echo "  PASS: nam_kperf present";; *) echo "  FAIL: nam_kperf MISSING"; fail=1;; esac
  case "$SYMS" in *nam_aperf*) echo "  FAIL: nam_aperf PRESENT (stale thread=5 silence build)"; fail=1;; *) echo "  PASS: nam_aperf absent";; esac
fi

echo "=== TIER 1b: profiles present ($NAMLIBDIR) ==="
n=$(ls "$NAMLIBDIR"/*.nam 2>/dev/null | wc -l | tr -d ' ')
if [ "$n" -gt 0 ]; then echo "  PASS: $n .nam profiles"; else echo "  FAIL: no .nam files in $NAMLIBDIR"; fail=1; fi

echo "=== TIER 2: functional render (real plugin, tone in) ==="
if command -v csound >/dev/null 2>&1 && [ -f "$CSD" ] && [ -f "$SO" ]; then
  OUT="$(csound -n -m0 -d --opcode-lib="$SO" --omacro:NAMLIBDIR="$NAMLIBDIR" "$CSD" 2>&1)"
  echo "$OUT" | grep -E 'WET_RMS=|DRY_RMS=|NAM_TEST=' | sed 's/^/  /'
  echo "$OUT" | grep -qE 'NAM_TEST=PASS' || { echo "  -> TIER 2 FAIL"; fail=1; }
  # confirm the loader actually loaded a model (catches wrong dir / get_dsp throw)
  echo "$OUT" | grep -qE '\[NAMLoader\] Loaded profile|\[NAMProcess\].*profiles' \
    && echo "  PASS: loader reported model load" \
    || echo "  WARN: no loader/load message seen (check stderr; model may be slow to load)"
else
  echo "  SKIP: need csound + $CSD + $SO"
fi

echo "=== TIER 2b: channel isolation (per-channel model instances) ==="
ISOCSD="$HERE/nam_channel_isolation.csd"
if command -v csound >/dev/null 2>&1 && command -v python3 >/dev/null 2>&1 \
   && [ -f "$ISOCSD" ] && [ -f "$SO" ]; then
  ISOTMP="$(mktemp -d)"
  iso_ok=1
  for R in 0 1; do
    # capture to a file, grep after the render finishes (never grep -q a live
    # csound pipe — SIGPIPE would kill the render mid-way under pipefail)
    ISOLOG="$ISOTMP/render_r$R.log"
    csound -n -m0 -d --opcode-lib="$SO" "--omacro:NAMLIBDIR=$NAMLIBDIR" \
        "--omacro:OUTDIR=$ISOTMP" "--omacro:RSIG=$R" "$ISOCSD" \
        >"$ISOLOG" 2>&1
    if ! grep -q '\[NAMLoader\] Loaded profile' "$ISOLOG"; then
      echo "  FAIL: render RSIG=$R did not load a model"; iso_ok=0
    fi
  done
  if [ "$iso_ok" -eq 1 ]; then
    if python3 "$HERE/analyze_isolation.py" "$ISOTMP" | sed 's/^/  /'; then
      echo "  PASS: L output independent of R content"
    else
      echo "  -> TIER 2b FAIL (channel crosstalk — shared model instance?)"; fail=1
    fi
  else
    fail=1
  fi
  rm -rf "$ISOTMP"
else
  echo "  SKIP: need csound + python3 + $ISOCSD + $SO"
fi

echo "=== TIER 3 (manual integration): full demiurge chain ==="
echo "  Run the probe harness with the NAM stage active, NAM toggled ON via MIDI:"
echo "    1) redem (demiurge up, NAM stage in live.conf or example nam_stage)"
echo "    2) bash testing/probe/probe.sh --fx 15      # inject -> capture -> analyze"
echo "    3) compare runs/<ts>/report.txt with NAM on vs off:"
echo "       working NAM => output non-silent, spectrum shows added harmonics vs dry."

echo ""
if [ "$fail" -eq 0 ]; then echo "RESULT: PASS"; exit 0; else echo "RESULT: FAIL"; exit 1; fi
