#!/usr/bin/env bash
# DEMIURGE sweep — MIDI stress driver.
#
# Drives the running NEPTR Phase 4 csound effect engine into maximum
# DSP load by toggling all five FX slots ON, holding them for a
# duration, then toggling OFF on exit. Used during a sweep cell so
# the probe measures the chain *under load*, not idle.
#
# This is a thin wrapper around the existing MIDI injector:
#   neptr/scripts/claude_midi.sh
# It does not implement its own MIDI; the canonical injector is in
# neptr/scripts and is the single source of truth (see
# reference_claude_midi memory).
#
# Usage:
#   stress.sh hold <seconds>     # toggle all slots ON, sleep, OFF
#   stress.sh on                 # toggle all slots ON, return immediately
#   stress.sh off                # toggle all slots OFF, return immediately
#   stress.sh churn <seconds>    # rotate menu params during hold (heavier)
#
# Slot CCs (NEPTR Phase 4 convention, ch1):
#   CC74 = Enc5 = slot 0    (e.g. VOLUME / first menu pick)
#   CC75 = Enc6 = slot 1
#   CC76 = Enc7 = slot 2
#   CC71 = Enc2 = slot 3
#   CC72 = Enc3 = slot 4
#
# The script TAPs each slot button (press + release). Whether the slot
# turns "on" or "off" depends on the chain's current state. Always
# pair `on` with a later `off` on the same boot, or use `hold` to
# bracket both.
#
# Environment:
#   CLAUDE_MIDI   path to claude_midi.sh
#                 default: ~/NEPTR_phase4/scripts/claude_midi.sh on Pi,
#                          falls back to relative ../../neptr/scripts/claude_midi.sh

set -euo pipefail

SLOT_CCS=(74 75 76 71 72)

CLAUDE_MIDI="${CLAUDE_MIDI:-}"
if [[ -z "${CLAUDE_MIDI}" ]]; then
  for candidate in \
    "$HOME/NEPTR_phase4/scripts/claude_midi.sh" \
    "$(dirname "$0")/../../neptr/scripts/claude_midi.sh"
  do
    if [[ -x "${candidate}" ]]; then CLAUDE_MIDI="${candidate}"; break; fi
  done
fi
if [[ ! -x "${CLAUDE_MIDI:-}" ]]; then
  echo "stress: cannot locate claude_midi.sh; set CLAUDE_MIDI=/path/to/it" >&2
  exit 1
fi

toggle_all() {
  for cc in "${SLOT_CCS[@]}"; do
    "${CLAUDE_MIDI}" tap "${cc}" >/dev/null
    sleep 0.05
  done
}

churn_params() {
  # Rotate Enc1 and Enc4 (menu prev/next) plus the slot encoder values
  # to keep DSP recomputing parameter targets — heavier than just hold.
  local until="$1"
  while [[ "$(date +%s)" -lt "${until}" ]]; do
    "${CLAUDE_MIDI}" cc 23 127 >/dev/null  # menu next
    sleep 0.10
    "${CLAUDE_MIDI}" cc 24 96  >/dev/null  # Enc5 rotation, value ~75%
    sleep 0.10
    "${CLAUDE_MIDI}" cc 26 32  >/dev/null  # Enc7 rotation, value ~25%
    sleep 0.10
    "${CLAUDE_MIDI}" cc 20 127 >/dev/null  # menu prev
    sleep 0.10
  done
}

cmd="${1:-}"; shift || true
case "${cmd}" in
  hold)
    dur="${1:?usage: stress.sh hold <seconds>}"
    trap 'toggle_all 2>/dev/null || true' EXIT INT TERM
    echo "[stress] slots ON for ${dur}s"
    toggle_all
    sleep "${dur}"
    ;;
  on)
    echo "[stress] slots ON"
    toggle_all
    ;;
  off)
    echo "[stress] slots OFF"
    toggle_all
    ;;
  churn)
    dur="${1:?usage: stress.sh churn <seconds>}"
    trap 'toggle_all 2>/dev/null || true' EXIT INT TERM
    echo "[stress] slots ON + churn for ${dur}s"
    toggle_all
    churn_params $(( $(date +%s) + dur ))
    ;;
  *)
    echo "usage: stress.sh {hold <sec>|on|off|churn <sec>}" >&2
    exit 1
    ;;
esac
