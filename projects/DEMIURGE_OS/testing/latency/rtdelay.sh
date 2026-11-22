#!/bin/bash
# demiurge-rtdelay runner — TRUE round-trip latency with a physical loopback cable.
#
# Requires: the selected interface's OUTPUT patched to its INPUT with a cable.
#
# Out-of-band, like the rest of testing/latency: it parks the chain's output
# link for the measurement window and ALWAYS restores it on exit (signal
# handled), never edits live.conf, never stops demiurge.service. The chain
# keeps running throughout, so the CPU load under measurement is realistic.
#
#   ./rtdelay.sh [shots] [gap_ms]
#
# Env overrides:
#   HW_O / HW_I   force the playback / capture node names
#   KEEP_CHAIN=1  leave the chain's output on the DAC (measures with the
#                 instrument audible — noisier, but proves the number holds
#                 under real signal rather than under silence)

set -u
cd "$(dirname "$0")"

SHOTS=${1:-30}
GAP=${2:-400}

pick() { pw-link "$1" 2>/dev/null | cut -d: -f1 | sort -u | grep -E "^alsa_$2\." | grep -v snd_aloop | grep -vi hdmi | head -1; }
HW_O=${HW_O:-$(pick -i output)}
HW_I=${HW_I:-$(pick -o input)}

if [ -z "$HW_O" ] || [ -z "$HW_I" ]; then
    echo "rtdelay: could not find a hardware playback+capture pair." >&2
    echo "  playback: ${HW_O:-<none>}   capture: ${HW_I:-<none>}" >&2
    exit 1
fi
echo "rtdelay: playback $HW_O"
echo "rtdelay: capture  $HW_I"

# Snapshot whatever is currently driving the DAC so we can put it back exactly.
PARKED=$(pw-link -l | awk -v hw="$HW_O:playback_FL" '
    /^[^ \t]/ { port=$0; next }
    /\|<- /   { if (port == hw) { sub(/.*\|<- /, ""); print } }')

restore() {
    [ -n "${PARKED:-}" ] && while read -r src; do
        [ -n "$src" ] || continue
        pw-link "$src" "$HW_O:playback_FL" 2>/dev/null
        echo "$src" | grep -q "output1" && pw-link "${src%1}2" "$HW_O:playback_FR" 2>/dev/null
    done <<< "$PARKED"
    echo "rtdelay: graph restored"
}
trap restore EXIT INT TERM

if [ "${KEEP_CHAIN:-0}" != "1" ]; then
    while read -r src; do
        [ -n "$src" ] || continue
        pw-link -d "$src" "$HW_O:playback_FL" 2>/dev/null
        echo "$src" | grep -q "output1" && pw-link -d "${src%1}2" "$HW_O:playback_FR" 2>/dev/null
    done <<< "$PARKED"
    echo "rtdelay: chain output parked for the measurement window"
fi

[ -x ./rtdelay ] || gcc -O2 -o rtdelay rtdelay.c -ljack -lm || exit 1

pw-jack ./rtdelay "$SHOTS" "$GAP" &
RT=$!
# Wait for the client's ports to appear, then wire it to the hardware.
for _ in $(seq 1 50); do
    pw-link -o 2>/dev/null | grep -q '^rtdelay:out$' && break
    sleep 0.1
done
pw-link rtdelay:out "$HW_O:playback_FL" 2>/dev/null
pw-link "$HW_I:capture_FL" rtdelay:in    2>/dev/null

# HOLD THE LINKS. The launcher owns every hardware playback port: its
# scrub_hw_playback pass deletes any incoming link that is neither
# demiurge-sink-output nor in its own `desired` set, and rtdelay:out is by
# definition neither. So on the launcher's next graph event — a pw-mon tick, a
# heartbeat, a live.conf touch — our measurement link vanishes mid-run and
# every subsequent shot reports MISS. That is not a hardware fault and it cost
# a 40-shot run (3 ok, 37 missed) before it was understood.
#
# Re-asserting once a second is enough: pw-link on an existing link is a
# harmless no-op, and a link that was scrubbed is back before the next shot.
# Cheaper and far less invasive than teaching the launcher about a test client.
( while kill -0 $RT 2>/dev/null; do
      pw-link rtdelay:out "$HW_O:playback_FL" 2>/dev/null
      pw-link "$HW_I:capture_FL" rtdelay:in  2>/dev/null
      sleep 1
  done ) &
HOLD=$!
trap 'kill $HOLD 2>/dev/null; restore' EXIT INT TERM

wait $RT
