#!/usr/bin/env bash
# DEMIURGE bypass — run csound DIRECT-to-ALSA, no PipeWire/JACK between.
#
# Reads live.conf for:
#   bypass         = on   (must be 'on' or this script refuses)
#   audio_interface = hw:CARD=USB,DEV=0
#   rate           = 48000   (optional, default 48000)
#   chain          =        (the .csd file to run — first .csd in the list)
#
# Stops demiurge.service, launches csound with raw ALSA on the named
# device, holds until you Ctrl-C, then restarts demiurge.service.
#
# Usage:
#   run-bypass.sh                          # period 128
#   run-bypass.sh 64                       # period 64 (lower latency, risk of xrun)
#   run-bypass.sh --live-conf path/to/conf # explicit config file
#
# What you should hear when csound is up:
#   ~7 ms RTT @ period=128, ~3.5 ms @ period=64. Talk into the mic,
#   listen to output. If the glitch is gone, the sync layer was the
#   culprit. If the glitch is still there, it's deeper than software.

set -euo pipefail

LIVE_CONF="${HOME}/NEPTR_phase4/live.conf"
PERIOD=128
while [[ $# -gt 0 ]]; do
  case "$1" in
    --live-conf) LIVE_CONF="$2"; shift 2 ;;
    *[0-9]*)     PERIOD="$1"; shift ;;
    *)           echo "[bypass] unknown arg: $1" >&2; exit 1 ;;
  esac
done

[[ -f "${LIVE_CONF}" ]] || { echo "[bypass] missing: ${LIVE_CONF}" >&2; exit 1; }

read_field() {
  awk -v k="$1" '
    $0 ~ "^[[:space:]]*"k"[[:space:]]*=" {
      sub(/^[^=]*=[[:space:]]*/, ""); sub(/[[:space:]]*#.*$/, ""); sub(/[[:space:]]+$/, "");
      print; exit
    }
  ' "${LIVE_CONF}"
}

read_chain_csd() {
  awk '
    /^chain[[:space:]]*=/ { in_chain = 1; next }
    /^[a-z]+[[:space:]]*=/ { in_chain = 0 }
    in_chain && /\.csd[[:space:]]*$/ {
      sub(/^[[:space:]]+/, ""); sub(/[[:space:]]+$/, "")
      # expand ~
      sub(/^~/, ENVIRON["HOME"])
      print; exit
    }
  ' "${LIVE_CONF}"
}

BYPASS_FLAG="$(read_field bypass)"
AUDIO_IF="$(read_field audio_interface)"
RATE="$(read_field rate)"
CSD="$(read_chain_csd)"

[[ "${BYPASS_FLAG}" == "on" ]] || {
  echo "[bypass] live.conf has bypass = '${BYPASS_FLAG:-(unset)}', refusing." >&2
  echo "[bypass] set 'bypass = on' in ${LIVE_CONF} first." >&2
  exit 1
}
[[ -n "${AUDIO_IF}" ]] || { echo "[bypass] live.conf missing audio_interface" >&2; exit 1; }
[[ -n "${CSD}" && -f "${CSD}" ]] || { echo "[bypass] no .csd found in chain: '${CSD}'" >&2; exit 1; }
[[ -n "${RATE}" ]] || RATE=48000

HW_BUF=$(( PERIOD * 8 ))    # software ring sized 8× period for headroom

echo "[bypass] config:"
echo "  live.conf       : ${LIVE_CONF}"
echo "  csd             : ${CSD}"
echo "  audio_interface : ${AUDIO_IF}"
echo "  rate            : ${RATE} Hz"
echo "  period (-b)     : ${PERIOD} samples"
echo "  hw buf (-B)     : ${HW_BUF} samples"
echo "  expected RTT    : ~$(( PERIOD * 2 * 1000 / RATE ))-$(( PERIOD * 2 * 1000 / RATE + 2 )) ms (period × 2 + IO)"

DEMIURGE_WAS_ACTIVE=0
if sudo systemctl is-active demiurge >/dev/null 2>&1; then
  DEMIURGE_WAS_ACTIVE=1
fi

# PipeWire's ALSA monitor keeps a handle on the device even when nothing
# is routed — so demiurge.service stop alone leaves the device "busy" to
# raw-ALSA csound. We stop pipewire + wireplumber (user services) too.
PW_WAS_ACTIVE=0
if systemctl --user is-active pipewire >/dev/null 2>&1; then
  PW_WAS_ACTIVE=1
fi

cleanup() {
  set +e
  if [[ -n "${CSND_PID:-}" ]] && kill -0 "${CSND_PID}" 2>/dev/null; then
    echo "[bypass] stopping csound (pid ${CSND_PID})…"
    kill -INT "${CSND_PID}" 2>/dev/null
    wait "${CSND_PID}" 2>/dev/null
  fi
  if [[ "${PW_WAS_ACTIVE}" -eq 1 ]]; then
    echo "[bypass] starting pipewire + wireplumber…"
    systemctl --user start pipewire wireplumber 2>/dev/null
    # Wait a moment for graph to come up before demiurge connects
    for i in $(seq 1 15); do
      if pw-cli ls Core >/dev/null 2>&1; then break; fi
      sleep 0.5
    done
  fi
  if [[ "${DEMIURGE_WAS_ACTIVE}" -eq 1 ]]; then
    echo "[bypass] starting demiurge.service…"
    sudo systemctl start demiurge
  fi
  echo "[bypass] done."
}
trap cleanup EXIT INT TERM

if [[ "${DEMIURGE_WAS_ACTIVE}" -eq 1 ]]; then
  echo "[bypass] stopping demiurge.service…"
  sudo systemctl stop demiurge
fi
if [[ "${PW_WAS_ACTIVE}" -eq 1 ]]; then
  echo "[bypass] stopping pipewire + wireplumber so Scarlett is free…"
  systemctl --user stop pipewire wireplumber pipewire.socket pipewire-pulse.socket 2>/dev/null
fi
# Give the kernel a moment to release the device
sleep 1

echo "[bypass] launching csound DIRECT to ${AUDIO_IF}…"
echo "  Ctrl-C to stop and return to normal chain."
echo

# RT priority same as the regular wrapper. NO --sched (gotchas Q7b).
# -+ignore_csopts so the .csd's hardcoded JACK/hw flags don't fight us.
#
# Device is prefixed with `plug:` so ALSA's plug layer handles the
# format conversion. Csound's ALSA backend requests FLOAT_LE by default;
# the Scarlett USB Audio Class device wants S32_LE. Without `plug:`,
# csound errors out with "Unable to set requested sample format on
# soundcard". The plug layer does a cheap in-kernel conversion (one
# memcpy per sample, no resampling, no buffering — negligible latency
# cost). hw:* directly avoids that conversion but requires csound to
# request a format the device supports natively, which we can't easily
# control from csound flags.
# Map hw:CARD=USB,DEV=0 → plughw:CARD=USB,DEV=0 (ALSA syntax: plug+hw
# is "plughw" as one token, not "plug:hw").
DEV="$(echo "${AUDIO_IF}" | sed -E 's/^hw:/plughw:/')"

exec chrt --fifo 74 csound \
  -d \
  -+ignore_csopts=1 \
  -+rtaudio=alsa \
  -odac:"${DEV}" \
  -iadc:"${DEV}" \
  -r "${RATE}" \
  -B "${HW_BUF}" \
  -b "${PERIOD}" \
  -+rtmidi=alsaseq \
  -M 14:0 \
  --realtime \
  "${CSD}" &
CSND_PID=$!

# Wait for csound to either exit or for the user to Ctrl-C us.
wait "${CSND_PID}" || true
CSND_PID=""
