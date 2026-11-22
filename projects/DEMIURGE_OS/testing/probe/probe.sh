#!/usr/bin/env bash
# DEMIURGE probe — measure full-software-latency + detect glitches by
# injecting a known pink-noise + impulse signal in place of the mic,
# then capturing the chain's output as close to the DAC as possible.
#
# Usage:
#   probe.sh [seconds] [--fx] [--play]
#
# Flags:
#   --fx     — engage heavy FX during capture (AMBIENT preset + PHASER
#              + TREMOLO via claude_midi.sh). Required to test the
#              "glitch on FX engage" failure mode — without this the
#              chain runs in whatever preset was active before the probe.
#   --play   — at end of probe, play the captured WAV through the
#              default sink (Scarlett) so you can hear with your own
#              ears what the analyzer saw. Bypasses the chain.
#
# This script does NOT stop demiurge.service. It rewires at the JACK
# layer for the probe window and restores. Mic input is replaced by
# the test signal for the duration; nothing else is disturbed.

set -euo pipefail

# ── arg parse ──────────────────────────────────────────────────────────────────
DURATION=15
WITH_FX=0
PLAYBACK=0
for arg in "$@"; do
  case "$arg" in
    --fx)    WITH_FX=1 ;;
    --play)  PLAYBACK=1 ;;
    *[0-9]*) DURATION="$arg" ;;
    *)       echo "[probe] unknown arg: $arg" >&2; exit 1 ;;
  esac
done

PROBE_DIR="$(cd "$(dirname "$0")" && pwd)"
RUN_DIR="${PROBE_DIR}/runs/$(date +%Y%m%d-%H%M%S)"
mkdir -p "${RUN_DIR}"

INJECT_CSD="${PROBE_DIR}/inject.csd"
XRUN_WATCH="${PROBE_DIR}/../sweep/xrun-watch.sh"
CLAUDE_MIDI="${HOME}/NEPTR_phase4/scripts/claude_midi.sh"
SAVED_LINKS="${RUN_DIR}/saved-links.tsv"
INJECT_LOG="${RUN_DIR}/inject.log"
CAPTURE_LOG="${RUN_DIR}/capture.log"
REPORT="${RUN_DIR}/report.txt"

need() { command -v "$1" >/dev/null 2>&1 || { echo "[probe] missing: $1" >&2; exit 1; }; }
need csound
need pw-jack
need pw-link
need jack_lsp
need python3

CAPTURE_TOOL=""
if command -v jack_capture >/dev/null 2>&1; then CAPTURE_TOOL=jack_capture
elif command -v jack_rec >/dev/null 2>&1;  then CAPTURE_TOOL=jack_rec
else echo "[probe] need jack_capture or jack_rec (apt install jack-tools)" >&2; exit 1
fi

if [[ "${WITH_FX}" -eq 1 && ! -x "${CLAUDE_MIDI}" ]]; then
  echo "[probe] --fx requires ${CLAUDE_MIDI} (not found / not executable)" >&2
  exit 1
fi

# ── parse current JACK links into (src \t dst) lines ────────────────────────
collect_links() {
  if command -v pw-link >/dev/null 2>&1; then
    pw-jack pw-link -l 2>/dev/null | awk '
      /^[^ \t]/ { src = $0; next }
      /\|->/    { sub(/^[ \t]+\|->[ \t]+/, ""); print src "\t" $0 }
    '
  else
    pw-jack jack_lsp -c 2>/dev/null | awk '
      /^[^ \t]/ { p = $0; next }
      /^[ \t]+/ { sub(/^[ \t]+/, ""); print p "\t" $0 }
    '
  fi
}

# ── discover chain head ────────────────────────────────────────────────────
echo "[probe] discovering chain head…"
ALL_LINKS="$(collect_links)"

chain_links_text="$(
  echo "${ALL_LINKS}" \
    | awk -F'\t' '
        $1 ~ /^alsa_input\..*[Ss]carlett.*:capture/ &&
        $2 !~ /^(alsa_|demiurge-|inject)/ { print $1 "\t" $2 }
      '
)"

if [[ -z "${chain_links_text}" ]]; then
  chain_links_text="$(
    echo "${ALL_LINKS}" \
      | awk -F'\t' '
          $1 ~ /^alsa_input\..*:capture/ &&
          $2 !~ /^(alsa_|demiurge-|inject)/ { print $1 "\t" $2 }
        '
  )"
fi

if [[ -z "${chain_links_text}" ]]; then
  echo "[probe] no alsa_input → chain links found." >&2
  echo "${ALL_LINKS}" | head -30 >&2
  exit 1
fi

echo "${chain_links_text}" > "${SAVED_LINKS}"
HEAD_PORT="$(echo "${chain_links_text}" | head -1 | awk -F'\t' '{print $2}')"
HEAD_CLIENT="${HEAD_PORT%%:*}"
N_HEAD_LINKS=$(wc -l < "${SAVED_LINKS}")
echo "[probe] head: ${HEAD_CLIENT}  (${N_HEAD_LINKS} input link(s))"

# ── capture targets ────────────────────────────────────────────────────────
mapfile -t CAPTURE_TARGETS < <(
  pw-jack jack_lsp 2>/dev/null \
    | grep -i 'scarlett' \
    | grep -i 'headphones\|line.*1' \
    | grep -E ':monitor_F[LR]$' \
    | sort -u
)
if [[ ${#CAPTURE_TARGETS[@]} -eq 0 ]]; then
  mapfile -t CAPTURE_TARGETS < <(
    pw-jack jack_lsp 2>/dev/null \
      | grep -E '^demiurge-sink-output:(output|out)_' | sort -u
  )
fi
if [[ ${#CAPTURE_TARGETS[@]} -eq 0 ]]; then
  echo "[probe] no suitable output capture target." >&2; exit 1
fi
echo "[probe] capture targets: ${#CAPTURE_TARGETS[@]} port(s)"
CAPTURE_PORTS=("inject:out1" "${CAPTURE_TARGETS[@]}")

# ── trap: always restore + kill children + turn FX back off ───────────────
INJECT_PID=""
CAPTURE_PID=""
FX_ENGAGED=0

disengage_fx() {
  if [[ "${FX_ENGAGED}" -eq 1 && -x "${CLAUDE_MIDI}" ]]; then
    echo "[probe] disengaging FX (back to CLEAN preset, toggles off)…"
    "${CLAUDE_MIDI}" tap 72 >/dev/null 2>&1 || true; sleep 0.1
    "${CLAUDE_MIDI}" tap 76 >/dev/null 2>&1 || true; sleep 0.1
    "${CLAUDE_MIDI}" tap 75 >/dev/null 2>&1 || true; sleep 0.1
    FX_ENGAGED=0
  fi
}

cleanup() {
  set +e
  echo "[probe] cleanup…"
  if [[ -n "${CAPTURE_PID}" ]] && kill -0 "${CAPTURE_PID}" 2>/dev/null; then
    kill -INT "${CAPTURE_PID}" 2>/dev/null; wait "${CAPTURE_PID}" 2>/dev/null
  fi
  if [[ -n "${INJECT_PID}" ]] && kill -0 "${INJECT_PID}" 2>/dev/null; then
    kill -TERM "${INJECT_PID}" 2>/dev/null; wait "${INJECT_PID}" 2>/dev/null
  fi
  disengage_fx
  while IFS=$'\t' read -r src dst; do
    [[ -z "${dst}" ]] && continue
    pw-jack pw-link -d "inject:out1" "${dst}" 2>/dev/null
    pw-jack pw-link -d "inject:out2" "${dst}" 2>/dev/null
  done < "${SAVED_LINKS}"
  while IFS=$'\t' read -r src dst; do
    [[ -z "${src}" || -z "${dst}" ]] && continue
    pw-jack pw-link "${src}" "${dst}" 2>/dev/null
  done < "${SAVED_LINKS}"
  echo "[probe] graph restored."
}
trap cleanup EXIT INT TERM

# ── start injector ─────────────────────────────────────────────────────────
echo "[probe] launching injector (pink noise + impulse train)…"
pw-jack csound "${INJECT_CSD}" >"${INJECT_LOG}" 2>&1 &
INJECT_PID=$!

appeared=0
for _ in $(seq 1 50); do
  if pw-jack jack_lsp 2>/dev/null | grep -qx 'inject:out1'; then appeared=1; break; fi
  sleep 0.1
done
if [[ "${appeared}" -ne 1 ]]; then
  echo "[probe] inject:out1 never appeared. injector log:" >&2
  cat "${INJECT_LOG}" >&2 || true
  exit 1
fi

# ── swap mic → injector ────────────────────────────────────────────────────
echo "[probe] swapping mic for injector…"
idx=1
while IFS=$'\t' read -r src dst; do
  [[ -z "${src}" || -z "${dst}" ]] && continue
  pw-jack pw-link -d "${src}" "${dst}" 2>/dev/null || true
  pw-jack pw-link "inject:out${idx}" "${dst}" 2>/dev/null || \
    echo "[probe] warn: could not link inject:out${idx} -> ${dst}" >&2
  idx=$(( idx == 2 ? 1 : idx + 1 ))
done < "${SAVED_LINKS}"

sleep 0.3

# ── engage FX if --fx ──────────────────────────────────────────────────────
if [[ "${WITH_FX}" -eq 1 ]]; then
  echo "[probe] engaging heavy FX via MIDI (AMBIENT preset + PHASER + TREMOLO)…"
  "${CLAUDE_MIDI}" tap 74 >/dev/null 2>&1; sleep 0.15  # AMBIENT preset
  "${CLAUDE_MIDI}" tap 76 >/dev/null 2>&1; sleep 0.15  # PHASER toggle on
  "${CLAUDE_MIDI}" tap 75 >/dev/null 2>&1; sleep 0.30  # TREMOLO toggle on
  FX_ENGAGED=1
fi

# ── start xrun-watch ───────────────────────────────────────────────────────
if [[ -x "${XRUN_WATCH}" ]]; then
  "${XRUN_WATCH}" start "${RUN_DIR}" >/dev/null 2>&1 || true
fi

# ── record ────────────────────────────────────────────────────────────────
echo "[probe] recording ${DURATION}s, $(( ${#CAPTURE_PORTS[@]} )) channels$([ "${WITH_FX}" -eq 1 ] && echo ", FX engaged")…"
if [[ "${CAPTURE_TOOL}" == "jack_capture" ]]; then
  PORT_ARGS=()
  for p in "${CAPTURE_PORTS[@]}"; do PORT_ARGS+=(-p "${p}"); done
  pw-jack jack_capture --bitdepth 32 --filename-prefix "${RUN_DIR}/cap-" \
    --duration "${DURATION}" --silent "${PORT_ARGS[@]}" \
    >"${CAPTURE_LOG}" 2>&1 &
  CAPTURE_PID=$!
else
  pw-jack jack_rec -d "${DURATION}" -f "${RUN_DIR}/cap.wav" "${CAPTURE_PORTS[@]}" \
    >"${CAPTURE_LOG}" 2>&1 &
  CAPTURE_PID=$!
fi
wait "${CAPTURE_PID}" || true
CAPTURE_PID=""

# ── xrun-watch stop ────────────────────────────────────────────────────────
if [[ -x "${XRUN_WATCH}" ]]; then
  CSND_PID=$(pgrep -f "csound.*neptr" | head -1)
  CSND_LOG=""
  [[ -n "${CSND_PID}" ]] && CSND_LOG="/proc/${CSND_PID}/fd/2"
  "${XRUN_WATCH}" stop "${RUN_DIR}" "${CSND_LOG:-}" >/dev/null 2>&1 || true
fi

# ── disengage FX so chain returns to user-friendly state ──────────────────
disengage_fx

RECORDED="$(ls -1t "${RUN_DIR}"/cap*.wav 2>/dev/null | head -n1 || true)"
if [[ -z "${RECORDED}" ]]; then
  echo "[probe] no .wav produced. capture log:" >&2
  cat "${CAPTURE_LOG}" >&2 || true
  exit 1
fi

# ── analyze ───────────────────────────────────────────────────────────────
echo "[probe] analyzing ${RECORDED}…"
python3 "${PROBE_DIR}/analyze.py" --capture "${RECORDED}" --report "${REPORT}"

echo
echo "===== probe report ====="
cat "${REPORT}"
[[ -f "${RUN_DIR}/xrun-report.txt" ]] && { echo; cat "${RUN_DIR}/xrun-report.txt"; }
echo "========================"
echo "run dir: ${RUN_DIR}"

# ── playback if --play ────────────────────────────────────────────────────
if [[ "${PLAYBACK}" -eq 1 ]]; then
  echo
  echo "[probe] playing captured WAV through default sink (Scarlett, bypassing chain)…"
  echo "[probe]   ch0 = injector reference, ch1+ = what the chain produced."
  echo "[probe]   Listen for clicks / crackle / dropouts. Ctrl-C to stop early."
  # pw-cat plays through default sink; mix multichannel to stereo so it's audible
  pw-jack pw-cat --playback --channels 2 "${RECORDED}" 2>/dev/null || \
    aplay "${RECORDED}" 2>/dev/null || \
    echo "[probe] no working playback tool found (tried pw-cat, aplay)."
fi
