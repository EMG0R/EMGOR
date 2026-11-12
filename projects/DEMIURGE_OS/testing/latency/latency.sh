#!/usr/bin/env bash
# DEMIURGE latency — orchestrator for the end-to-end audio test system.
#
#   latency.sh internal   DEFAULT. Self-generated sweep pushed through the
#                         sync-layer sink, captured at its pre-DAC monitor:
#                         measures INTERNAL software latency with no mic and
#                         no audio interface required, then adds the connected
#                         interface's offset (calibrated, else spec estimate).
#                         Publishes state.json for the UIs.
#   latency.sh shot       fire a sweep through a live INPUT chain (needs a
#                         mic/instrument patch up), measuring the full path.
#   latency.sh calibrate  one-time per interface: with a physical loopback
#                         cable (an output patched back to an input), measure
#                         the interface's hardware (ADC+DAC) latency and store
#                         it so the spec estimate is replaced by a real number.
#   latency.sh table      print the per-interface calibration table + last shot.
#   latency.sh selftest   run the no-hardware testing-ground self-test.
#
# Like probe/, this is OUT-OF-BAND: it rewires at the JACK layer for the
# measurement window and ALWAYS restores the graph on exit (signal-handled),
# even on Ctrl-C or failure. It does not stop demiurge.service.

set -euo pipefail

LAT_DIR="$(cd "$(dirname "$0")" && pwd)"
SWEEP_WAV="${LAT_DIR}/sweep.wav"
INJECT_CSD="${LAT_DIR}/inject_sweep.csd"
CALIB_STORE="${LAT_DIR}/calib.json"
STATE="${LAT_DIR}/state.json"
LIVE_CONF="${HOME}/demiurge/live.conf"

CMD="${1:-internal}"; shift || true
DURATION=6

# Globals the EXIT traps reference (must outlive the function scope).
INJECT_PID=""; CAPTURE_PID=""
SINK=""; PL_FL=""; PL_FR=""; MON_FL=""; MON_FR=""
ENT_FL=""; ENT_FR=""; SRC_FL=""; SRC_FR=""
SAVED=""; OUT_PORTS=()

# ── deps ────────────────────────────────────────────────────────────────────
need() { command -v "$1" >/dev/null 2>&1 || { echo "[latency] missing: $1" >&2; exit 1; }; }

capture_tool() {
  if command -v jack_capture >/dev/null 2>&1; then echo jack_capture
  elif command -v jack_rec >/dev/null 2>&1;  then echo jack_rec
  else echo "[latency] need jack_capture or jack_rec (apt install jack-tools)" >&2; exit 1
  fi
}

# ── graph helpers (mirrors probe.sh) ────────────────────────────────────────
collect_links() {
  pw-jack pw-link -l 2>/dev/null | awk '
    /^[^ \t]/ { src = $0; next }
    /\|->/    { sub(/^[ \t]+\|->[ \t]+/, ""); print src "\t" $0 }'
}

ensure_sweep() {
  [[ -f "${SWEEP_WAV}" ]] || python3 "${LAT_DIR}/cli.py" gensweep \
    --out "${SWEEP_WAV}" --dur 2.0 --loops 1 >/dev/null
}

start_injector() {  # $1 = run dir
  # Injector self-generates the sweep (no file dependency).
  pw-jack csound "${INJECT_CSD}" \
    >"${1}/inject.log" 2>&1 &
  INJECT_PID=$!
  for _ in $(seq 1 50); do
    pw-jack jack_lsp 2>/dev/null | grep -qx 'inject:out1' && return 0
    sleep 0.1
  done
  echo "[latency] inject:out1 never appeared; see ${1}/inject.log" >&2
  cat "${1}/inject.log" >&2 || true
  return 1
}

# Detect which physical interface is the active output, for labelling +
# choosing its stored hardware offset / spec estimate.
# JACK CLIENT NAME FOR THE HARDWARE CARD.
#
# Everything here talks to the graph through pw-jack, and JACK names a client
# by node.DESCRIPTION, not node.name: the card shows up as
# `Built-in Audio Pro:playback_FL`, never `alsa_output.platform-soc...`.
# Matching `alsa_*` against jack_lsp therefore finds nothing, whatever the
# profile — which is why calibrate reported "could not find physical
# playback/capture ports" with the cable correctly patched and the card
# working. Resolve the description from PipeWire (where node.name IS visible)
# and match on that. `pw-cli ls Node` prints description before name in each
# block, so the description is carried forward to the name that follows it.
jack_client_for_hw() {
  pw-cli ls Node 2>/dev/null | awk '
    /node\.description[[:space:]]*=/ {
      d = $0; sub(/^[^"]*"/, "", d); sub(/".*$/, "", d)
    }
    /node\.name[[:space:]]*=/ {
      n = $0; sub(/^[^"]*"/, "", n); sub(/".*$/, "", n)
      if (n ~ /^alsa_(output|input)\.(usb-|platform-soc)/ && d != "") { print d; exit }
    }'
}

detect_interface() {
  # Look only at actual output nodes that are present in the graph.
  local n
  n="$(pw-cli ls Node 2>/dev/null | grep -iE 'node.name|node.description' || true)"
  # Real interfaces win first (usb / bluetooth). If there is NO real interface
  # and the only output is the snd-aloop virtual fallback sink, report a generic
  # virtual interface (~0 ms offset): there is no physical ADC/DAC converter —
  # only the loopback's own buffer, already counted in the internal/quantum
  # latency. Only then fall back to the hdmi/onboard guesses.
  if echo "$n" | grep -qiE 'bluez_output|a2dp_sink'; then echo bluetooth
  elif echo "$n" | grep -qiE 'alsa_output.*usb'; then echo usb
  # The onboard I2S card is a REAL converter and must outrank the virtual
  # fallback below. It has to be matched as `platform-soc` and not `platform`:
  # the snd-aloop clock publishes alsa_output.platform-snd_aloop..., so a
  # looser pattern claims the aloop node as the interface. Before this branch
  # existed the I2S card fell through to the `[Ll]oop` test, matched the
  # always-present aloop node, and reported `virtual` — an interface with no
  # converter and ~0 ms offset, which is exactly wrong for a card that has both
  # an ADC and a DAC, and which made `calibrate` refuse to find any physical
  # port to measure.
  elif echo "$n" | grep -qiE 'alsa_output\.platform-soc'; then echo platform
  elif echo "$n" | grep -qiE 'alsa_output.*snd_aloop|[Ll]oop'; then echo virtual
  elif echo "$n" | grep -qiE 'alsa_output.*hdmi'; then echo hdmi
  else echo onboard
  fi
}

# bypass (csound straight to ALSA) vs sync layer. In sync layer, distinguish:
#   synclayer-direct = exactly ONE wired USB interface — the launcher wires the
#                      chain straight to the device, bypassing the demiurge-sink
#                      loopback (one quantum less). Mirrors the launcher's
#                      aggregate::direct_output_device rule (single alsa_output.usb-*).
#   synclayer        = 0 or 2+ USB interfaces — chain routed via the loopback
#                      (the aggregate mixing point; also the no-device fallback).
detect_mode() {
  local in_sync=0
  if pw-jack jack_lsp 2>/dev/null | grep -qi 'demiurge-sink'; then in_sync=1
  elif [[ -f "${LIVE_CONF}" ]] && grep -qiE '^[[:space:]]*sync_layer[[:space:]]*=[[:space:]]*on' "${LIVE_CONF}"; then in_sync=1
  fi
  if [[ "$in_sync" == "0" ]]; then echo bypass; return; fi
  local usb_sinks
  usb_sinks="$(pw-link -i 2>/dev/null | grep -cE '^alsa_output\.usb-.*:playback_FL$')"
  if [[ "$usb_sinks" == "1" ]]; then echo synclayer-direct; else echo synclayer; fi
}

current_quantum_ms() {
  local q
  q="$(pw-metadata -n settings 2>/dev/null | sed -n 's/.*clock.force-quantum.*value:.\([0-9]*\).*/\1/p' | head -1)"
  [[ -z "$q" ]] && q="$(pw-metadata -n settings 2>/dev/null | sed -n 's/.*clock.quantum.*value:.\([0-9]*\).*/\1/p' | head -1)"
  [[ -z "$q" ]] && q=256
  echo "$(python3 -c "print(round(1000.0*${q}/48000.0,3))")"
}

# ── SHOT: measure through the live chain ────────────────────────────────────
do_shot() {
  need csound; need pw-jack; need pw-link; need jack_lsp; need python3
  local TOOL; TOOL="$(capture_tool)"
  ensure_sweep
  local RUN_DIR="${LAT_DIR}/runs/$(date +%Y%m%d-%H%M%S)"
  mkdir -p "${RUN_DIR}"
  SAVED="${RUN_DIR}/saved-links.tsv"

  echo "[latency] discovering chain head…"
  local ALL; ALL="$(collect_links)"
  local CHAIN
  CHAIN="$(echo "${ALL}" | awk -F'\t' '
      $1 ~ /^alsa_input\..*[Uu]sb.*:capture/ && $2 !~ /^(alsa_|demiurge-|inject)/ {print $1"\t"$2}')"
  [[ -z "${CHAIN}" ]] && CHAIN="$(echo "${ALL}" | awk -F'\t' '
      $1 ~ /^alsa_input\..*:capture/ && $2 !~ /^(alsa_|demiurge-|inject)/ {print $1"\t"$2}')"
  if [[ -z "${CHAIN}" ]]; then
    echo "[latency] no mic->chain links found; is the chain up?" >&2
    echo "${ALL}" | head -30 >&2; exit 1
  fi
  echo "${CHAIN}" > "${SAVED}"
  local HEAD; HEAD="$(echo "${CHAIN}" | head -1 | awk -F'\t' '{print $2}')"
  echo "[latency] head: ${HEAD%%:*}"

  # output capture targets (pre-DAC monitor, like probe)
  local TARGETS=()
  mapfile -t TARGETS < <(pw-jack jack_lsp 2>/dev/null \
    | grep -iE 'usb' | grep -E ':monitor_F[LR]$' | sort -u)
  [[ ${#TARGETS[@]} -eq 0 ]] && mapfile -t TARGETS < <(pw-jack jack_lsp 2>/dev/null \
    | grep -E '^demiurge-sink-output:(output|out)_' | sort -u)
  if [[ ${#TARGETS[@]} -eq 0 ]]; then
    echo "[latency] no output capture target found." >&2; exit 1
  fi
  local PORTS=("inject:out1" "${TARGETS[@]}")

  INJECT_PID=""; CAPTURE_PID=""
  cleanup_shot() {
    set +e
    [[ -n "${CAPTURE_PID}" ]] && kill -INT "${CAPTURE_PID}" 2>/dev/null && wait "${CAPTURE_PID}" 2>/dev/null
    [[ -n "${INJECT_PID}" ]] && kill -TERM "${INJECT_PID}" 2>/dev/null && wait "${INJECT_PID}" 2>/dev/null
    while IFS=$'\t' read -r src dst; do
      [[ -z "${dst}" ]] && continue
      pw-jack pw-link -d "inject:out1" "${dst}" 2>/dev/null
      pw-jack pw-link -d "inject:out2" "${dst}" 2>/dev/null
    done < "${SAVED}"
    while IFS=$'\t' read -r src dst; do
      [[ -z "${src}" || -z "${dst}" ]] && continue
      pw-jack pw-link "${src}" "${dst}" 2>/dev/null
    done < "${SAVED}"
    echo "[latency] graph restored."
  }
  trap cleanup_shot EXIT INT TERM

  echo "[latency] launching sweep injector…"
  start_injector "${RUN_DIR}"

  echo "[latency] swapping mic -> injector…"
  local idx=1
  while IFS=$'\t' read -r src dst; do
    [[ -z "${src}" || -z "${dst}" ]] && continue
    pw-jack pw-link -d "${src}" "${dst}" 2>/dev/null || true
    pw-jack pw-link "inject:out${idx}" "${dst}" 2>/dev/null || \
      echo "[latency] warn: link inject:out${idx} -> ${dst} failed" >&2
    idx=$(( idx == 2 ? 1 : idx + 1 ))
  done < "${SAVED}"
  sleep 0.3

  echo "[latency] recording ${DURATION}s, ${#PORTS[@]} channels…"
  if [[ "${TOOL}" == "jack_capture" ]]; then
    local PA=(); for p in "${PORTS[@]}"; do PA+=(-p "${p}"); done
    pw-jack jack_capture --bitdepth 32 --filename-prefix "${RUN_DIR}/cap-" \
      --duration "${DURATION}" --silent "${PA[@]}" >"${RUN_DIR}/capture.log" 2>&1 &
  else
    pw-jack jack_rec -d "${DURATION}" -f "${RUN_DIR}/cap.wav" "${PORTS[@]}" \
      >"${RUN_DIR}/capture.log" 2>&1 &
  fi
  CAPTURE_PID=$!
  wait "${CAPTURE_PID}" || true; CAPTURE_PID=""

  local CAP; CAP="$(ls -1t "${RUN_DIR}"/cap*.wav 2>/dev/null | head -1 || true)"
  [[ -z "${CAP}" ]] && { echo "[latency] no capture produced." >&2; cat "${RUN_DIR}/capture.log" >&2; exit 1; }

  local IFACE MODE OFFSET
  IFACE="$(detect_interface)"; MODE="$(detect_mode)"
  OFFSET="$(python3 "${LAT_DIR}/cli.py" calib get --store "${CALIB_STORE}" \
            --interface "${IFACE}" --mode "${MODE}" 2>/dev/null || echo 0)"
  echo "[latency] interface=${IFACE} mode=${MODE} hw-offset=${OFFSET} ms"

  echo
  python3 "${LAT_DIR}/cli.py" analyze --capture "${CAP}" \
    --interface "${IFACE}" --hw-offset-ms "${OFFSET}" \
    --segments 6 --state "${STATE}"
  echo "[latency] run dir: ${RUN_DIR}"
}

# ── INTERNAL: self-generated test through the sync layer (no mic/interface) ──
# Measures the REAL sync-layer ENTRANCE→EXIT latency with zero hardware:
#   inject -> demiurge-source INPUT (entrance) ─► [bridge source→sink, since the
#   chain is blank] ─► demiurge-sink monitor (exit, captured).
# That traverses both virtual-device buffers = the true internal latency a
# signal sees, even with no interface plugged in. The connected interface's
# offset (calibrated, else spec) is then ADDED automatically.
# Falls back to a sink-tap (computed latency) if the source ports aren't found.
do_internal() {
  need csound; need pw-jack; need pw-link; need jack_lsp; need python3
  local TOOL; TOOL="$(capture_tool)"
  local RUN_DIR="${LAT_DIR}/runs/internal-$(date +%Y%m%d-%H%M%S)"
  mkdir -p "${RUN_DIR}"

  # Discover sync-layer nodes (names carry spaces, e.g. "DEMIURGE Input").
  local lsp; lsp="$(pw-jack jack_lsp 2>/dev/null)"
  SINK="$(echo "${lsp}" | sed -n 's/:playback_FL$//p' | grep -i 'demiurge' | head -1)"
  [[ -z "${SINK}" ]] && SINK="$(echo "${lsp}" | sed -n 's/:playback_FL$//p' | head -1)"
  if [[ -z "${SINK}" ]]; then
    echo "[latency] no sync-layer sink found — sync layer must be UP (sync_layer=on)." >&2
    echo "${lsp}" | head -20 >&2; exit 1
  fi
  PL_FL="${SINK}:playback_FL"; PL_FR="${SINK}:playback_FR"
  MON_FL="${SINK}:monitor_FL"; MON_FR="${SINK}:monitor_FR"

  local ENT SRC MEASURED=0
  ENT="$(echo "${lsp}" | sed -n 's/:input_FL$//p'   | grep -i 'demiurge' | head -1)"
  SRC="$(echo "${lsp}" | sed -n 's/:capture_FL$//p' | grep -i 'demiurge' | head -1)"
  if [[ -n "${ENT}" && -n "${SRC}" ]]; then
    MEASURED=1
    ENT_FL="${ENT}:input_FL";  ENT_FR="${ENT}:input_FR"
    SRC_FL="${SRC}:capture_FL"; SRC_FR="${SRC}:capture_FR"
    echo "[latency] entrance: ${ENT}  →  exit: ${SINK}  (measured sync-layer path)"
  else
    echo "[latency] source ports not found; sink-tap only (computed latency)."
  fi

  cleanup_internal() {
    set +e
    [[ -n "${CAPTURE_PID}" ]] && kill -INT "${CAPTURE_PID}" 2>/dev/null && wait "${CAPTURE_PID}" 2>/dev/null
    [[ -n "${INJECT_PID}" ]] && kill -TERM "${INJECT_PID}" 2>/dev/null && wait "${INJECT_PID}" 2>/dev/null
    [[ -n "${ENT_FL}" ]] && pw-jack pw-link -d "inject:out1" "${ENT_FL}" 2>/dev/null
    [[ -n "${ENT_FR}" ]] && pw-jack pw-link -d "inject:out2" "${ENT_FR}" 2>/dev/null
    [[ -n "${SRC_FL}" ]] && pw-jack pw-link -d "${SRC_FL}" "${PL_FL}" 2>/dev/null
    [[ -n "${SRC_FR}" ]] && pw-jack pw-link -d "${SRC_FR}" "${PL_FR}" 2>/dev/null
    pw-jack pw-link -d "inject:out1" "${PL_FL}" 2>/dev/null
    pw-jack pw-link -d "inject:out2" "${PL_FR}" 2>/dev/null
    echo "[latency] graph restored."
  }
  trap cleanup_internal EXIT INT TERM

  echo "[latency] launching sweep injector…"
  start_injector "${RUN_DIR}"
  if [[ "${MEASURED}" -eq 1 ]]; then
    pw-jack pw-link "inject:out1" "${ENT_FL}" 2>/dev/null || true
    pw-jack pw-link "inject:out2" "${ENT_FR}" 2>/dev/null || true
    pw-jack pw-link "${SRC_FL}" "${PL_FL}" 2>/dev/null || true   # bridge source→sink
    pw-jack pw-link "${SRC_FR}" "${PL_FR}" 2>/dev/null || true
  else
    pw-jack pw-link "inject:out1" "${PL_FL}" 2>/dev/null || true
    pw-jack pw-link "inject:out2" "${PL_FR}" 2>/dev/null || true
  fi
  sleep 0.3

  local PORTS=("inject:out1" "${MON_FL}" "${MON_FR}")
  echo "[latency] recording ${DURATION}s through sync layer…"
  if [[ "${TOOL}" == "jack_capture" ]]; then
    local PA=(); for p in "${PORTS[@]}"; do PA+=(-p "${p}"); done
    pw-jack jack_capture --bitdepth 32 --filename-prefix "${RUN_DIR}/cap-" \
      --duration "${DURATION}" --silent "${PA[@]}" >"${RUN_DIR}/capture.log" 2>&1 &
  else
    pw-jack jack_rec -d "${DURATION}" -f "${RUN_DIR}/cap.wav" "${PORTS[@]}" \
      >"${RUN_DIR}/capture.log" 2>&1 &
  fi
  CAPTURE_PID=$!; wait "${CAPTURE_PID}" || true; CAPTURE_PID=""

  local CAP; CAP="$(ls -1t "${RUN_DIR}"/cap*.wav 2>/dev/null | head -1 || true)"
  [[ -z "${CAP}" ]] && { echo "[latency] no capture produced." >&2; cat "${RUN_DIR}/capture.log" >&2; exit 1; }

  local IFACE MODE OFF SRC QMS
  IFACE="$(detect_interface)"; MODE="$(detect_mode)"; QMS="$(current_quantum_ms)"
  IFS=$'\t' read -r OFF SRC < <(python3 "${LAT_DIR}/cli.py" calib offset \
      --store "${CALIB_STORE}" --interface "${IFACE}" --mode "${MODE}" 2>/dev/null || echo $'0\tspec')
  echo "[latency] interface=${IFACE} mode=${MODE} quantum=${QMS} ms offset=${OFF} ms (${SRC})"

  echo
  python3 "${LAT_DIR}/cli.py" analyze --capture "${CAP}" \
    --interface "${IFACE}" --hw-offset-ms "${OFF}" --offset-source "${SRC}" \
    --mode "${MODE}" --quantum-ms "${QMS}" --segments 6 --state "${STATE}"
  echo "[latency] run dir: ${RUN_DIR}"
}

# ── CALIBRATE: physical loopback, one-time per interface ────────────────────
do_calibrate() {
  need csound; need pw-jack; need pw-link; need jack_lsp; need python3
  local TOOL; TOOL="$(capture_tool)"
  ensure_sweep
  local IFACE; IFACE="$(detect_interface)"
  local MODE; MODE="$(detect_mode)"
  local RUN_DIR="${LAT_DIR}/runs/calib-$(date +%Y%m%d-%H%M%S)"
  mkdir -p "${RUN_DIR}"

  cat >&2 <<EOF
[latency] CALIBRATION — interface '${IFACE}', mode '${MODE}'.
  Patch a PHYSICAL CABLE from this interface's OUTPUT back to its INPUT now.
  (e.g. interface output 1 -> input 1). This measures the real ADC+DAC time.
EOF
  read -r -p "  Cable connected? [enter to measure, Ctrl-C to abort] " _ || true

  local IN_PORTS=(); OUT_PORTS=()
  # Port names differ by profile (playback_FL/FR under ACP and Pro Audio,
  # playback_AUX0.. on a raw Pro Audio card), so only the client name and the
  # `playback_`/`capture_` prefix are anchored.
  local HWC; HWC="$(jack_client_for_hw)"
  if [[ -z "${HWC}" ]]; then
    echo "[latency] no hardware card found in the graph — nothing to calibrate." >&2
    return 1
  fi
  echo "[latency] hardware JACK client: ${HWC}" >&2
  mapfile -t OUT_PORTS < <(pw-jack jack_lsp 2>/dev/null | grep -F "${HWC}:playback_" | sort -u)
  mapfile -t IN_PORTS  < <(pw-jack jack_lsp 2>/dev/null | grep -F "${HWC}:capture_"  | sort -u)
  if [[ ${#OUT_PORTS[@]} -eq 0 || ${#IN_PORTS[@]} -eq 0 ]]; then
    echo "[latency] could not find physical playback/capture ports for ${IFACE}." >&2
    pw-jack jack_lsp 2>/dev/null | grep -iE 'alsa_(in|out)' >&2; exit 1
  fi

  INJECT_PID=""; CAPTURE_PID=""
  cleanup_cal() {
    set +e
    [[ -n "${CAPTURE_PID}" ]] && kill -INT "${CAPTURE_PID}" 2>/dev/null && wait "${CAPTURE_PID}" 2>/dev/null
    [[ -n "${INJECT_PID}" ]] && kill -TERM "${INJECT_PID}" 2>/dev/null && wait "${INJECT_PID}" 2>/dev/null
    pw-jack pw-link -d "inject:out1" "${OUT_PORTS[0]}" 2>/dev/null
    [[ ${#OUT_PORTS[@]} -ge 2 ]] && pw-jack pw-link -d "inject:out2" "${OUT_PORTS[1]}" 2>/dev/null
  }
  trap cleanup_cal EXIT INT TERM

  start_injector "${RUN_DIR}"
  pw-jack pw-link "inject:out1" "${OUT_PORTS[0]}" 2>/dev/null || true
  [[ ${#OUT_PORTS[@]} -ge 2 ]] && pw-jack pw-link "inject:out2" "${OUT_PORTS[1]}" 2>/dev/null || true
  sleep 0.3

  local PORTS=("inject:out1" "${IN_PORTS[0]}")
  echo "[latency] measuring physical loopback round-trip (${DURATION}s)…"
  if [[ "${TOOL}" == "jack_capture" ]]; then
    local PA=(); for p in "${PORTS[@]}"; do PA+=(-p "${p}"); done
    pw-jack jack_capture --bitdepth 32 --filename-prefix "${RUN_DIR}/cap-" \
      --duration "${DURATION}" --silent "${PA[@]}" >"${RUN_DIR}/capture.log" 2>&1 &
  else
    pw-jack jack_rec -d "${DURATION}" -f "${RUN_DIR}/cap.wav" "${PORTS[@]}" \
      >"${RUN_DIR}/capture.log" 2>&1 &
  fi
  CAPTURE_PID=$!; wait "${CAPTURE_PID}" || true; CAPTURE_PID=""

  local CAP; CAP="$(ls -1t "${RUN_DIR}"/cap*.wav 2>/dev/null | head -1 || true)"
  [[ -z "${CAP}" ]] && { echo "[latency] no capture produced." >&2; exit 1; }

  # Physical round-trip (digital buffering + analog converters), then subtract
  # the digital playback+capture buffering (≈ 2 quanta) to isolate the
  # hardware (ADC+DAC) contribution the chain `shot` is missing.
  local PHYS DIGI HW
  # Scrape the analyzer's MEASURED round-trip line. It is labelled `internal`;
  # this used to grep for `software`, a label cli.py no longer prints, so PHYS
  # came back empty on every run. `${PHYS:-0}` then made HW
  # max(0 - digital, 0) = 0.000, and the tool stored that as a calibrated
  # hardware offset — a confident zero for a card that has a real ADC and DAC,
  # silently replacing the spec estimate with a worse number. Empty is now a
  # hard failure instead of a default zero.
  PHYS="$(python3 "${LAT_DIR}/cli.py" analyze --capture "${CAP}" --interface "${IFACE}" \
          --segments 4 2>/dev/null | sed -n 's/.*internal[[:space:]]*:[[:space:]]*\([0-9.]*\)[[:space:]]*ms.*/\1/p' | head -1)"
  if [[ -z "${PHYS}" ]]; then
    echo "[latency] could not read a round-trip from the capture — NOT storing a calibration." >&2
    echo "[latency] run dir: ${RUN_DIR}" >&2
    return 1
  fi
  DIGI="$(python3 -c "print(round(2*$(current_quantum_ms),3))")"
  HW="$(python3 -c "p=${PHYS}; d=${DIGI}; print(round(max(p-d,0.0),3))")"
  echo "[latency] physical round-trip ${PHYS:-?} ms − digital ${DIGI} ms = hardware ${HW} ms"
  python3 "${LAT_DIR}/cli.py" calib set --store "${CALIB_STORE}" \
    --interface "${IFACE}" --mode "${MODE}" --ms "${HW}"
  echo "[latency] you can disconnect the loopback cable now."
}

do_table() {
  echo "== calibration store (${CALIB_STORE}) =="
  python3 "${LAT_DIR}/cli.py" calib list --store "${CALIB_STORE}" 2>/dev/null || echo "  (none yet)"
  echo
  echo "== last shot (${STATE}) =="
  [[ -f "${STATE}" ]] && python3 -m json.tool "${STATE}" || echo "  (no shot yet)"
}

case "${CMD}" in
  internal)  do_internal "$@" ;;
  shot)      do_shot "$@" ;;
  calibrate) do_calibrate "$@" ;;
  table)     do_table ;;
  selftest)  python3 "${LAT_DIR}/cli.py" selftest ;;
  *) echo "usage: latency.sh {internal|shot|calibrate|table|selftest}" >&2; exit 2 ;;
esac
