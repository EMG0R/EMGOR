#!/bin/bash
# rtdelay-through.sh — rtdelay, but reading its input from $RTIN instead of
# from the hardware capture port.
#
# This is what measures WHAT A CLIENT ADDS to the round trip. rtdelay.sh
# measures the rig floor:
#
#   rtdelay:out -> DAC -> cable -> ADC -> rtdelay:in
#
# Point RTIN at a client's output instead and the click has to pass through
# that client on the way back:
#
#   rtdelay:out -> DAC -> cable -> ADC -> <client> -> rtdelay:in
#
# The difference between the two is the client's own latency, isolated from
# the converters and from the graph. That is how csound's -B ring was measured
# at +23.6 ms (see docs/latency-measurements.md):
#
#   RTIN=csound_pt:output1 ./rtdelay-through.sh 30 250
#
# NOTE it will report MISS for every shot against a client that does not pass
# its input through to its output. An instrument patch that is silent until
# triggered (NEPTR) is exactly that case, and the misses are correct
# behaviour rather than a broken cable.
set -u; cd "$(dirname "$0")"
SHOTS=${1:-30}; GAP=${2:-250}
HW_O=alsa_output.platform-soc_107c000000_sound.stereo-fallback
PARKED=$(pw-link -l | awk -v hw="$HW_O:playback_FL" '/^[^ \t]/{port=$0;next} /\|<- /{if(port==hw){sub(/.*\|<- /,"");print}}')
restore(){ while read -r s; do [ -n "$s" ] || continue; pw-link "$s" "$HW_O:playback_FL" 2>/dev/null; echo "$s"|grep -q output1 && pw-link "${s%1}2" "$HW_O:playback_FR" 2>/dev/null; done <<< "$PARKED"; }
while read -r s; do [ -n "$s" ] || continue; pw-link -d "$s" "$HW_O:playback_FL" 2>/dev/null; echo "$s"|grep -q output1 && pw-link -d "${s%1}2" "$HW_O:playback_FR" 2>/dev/null; done <<< "$PARKED"
pw-jack ./rtdelay $SHOTS $GAP & RT=$!
for _ in $(seq 1 50); do pw-link -o 2>/dev/null | grep -q "^rtdelay:out$" && break; sleep 0.1; done
( while kill -0 $RT 2>/dev/null; do pw-link rtdelay:out "$HW_O:playback_FL" 2>/dev/null; pw-link "$RTIN" rtdelay:in 2>/dev/null; sleep 1; done ) & HOLD=$!
trap "kill $HOLD 2>/dev/null; restore" EXIT INT TERM
wait $RT
