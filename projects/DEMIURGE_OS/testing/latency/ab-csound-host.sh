#!/bin/bash
# ab-csound-host.sh — A/B the STANDALONE csound backend against the EMBEDDED
# host (src/demiurge-csound-host.cpp): same fixture, same quantum, same cable,
# one window, so nothing about the rig can drift between the three numbers.
#
#   floor   rtdelay:out -> DAC -> cable -> ADC -> rtdelay:in
#   std     ...                                -> csound -+rtaudio=jack -> rtdelay:in
#   emb     ...                                -> demiurge-csound-host   -> rtdelay:in
#
# std minus floor is what csound's engine-thread ring (-B) costs. emb minus
# floor is what the embedded host costs, and the entire point of the host is
# that this should be ~0 frames.
#
# Both engines run the SAME passthrough.csd — read input, write output, no DSP
# — at ksmps = the live graph quantum, so the comparison isolates the audio
# backend rather than any patch setting. Method: docs/latency-measurements.md.
#
#   ./ab-csound-host.sh [shots] [gap_ms]
#
# Requires a physical loopback cable, interface out -> interface in.
#
# TWO THINGS THIS SCRIPT LEARNED THE HARD WAY
#
# 1. Run the three passes SEQUENTIALLY, with only one engine alive at a time.
#    Two passthrough engines listening to the same capture and both reachable
#    from the DAC is a recipe for cross-talk that is invisible in the summary.
#
# 2. AUTOCONNECT MUST BE OFF. WirePlumber links a new client's output to the
#    default sink unless told not to. With a loopback cable in place that closes
#    capture -> passthrough -> DAC -> cable -> capture: a howl-round feedback
#    loop. The input is then permanently loud, rtdelay's peak-relative onset
#    detector fires on sample 0 of every window, and the run reports a median of
#    0 frames with a huge spread — which looks like a broken tool and is really
#    a broken graph. demiurge-env.sh sets node.autoconnect=false for exactly
#    this reason, so we source it (it also supplies the pipewire-jack shim on
#    LD_LIBRARY_PATH, without which plain `csound -+rtaudio=jack` looks for a
#    jackd that does not exist and never registers at all). Belt and braces, we
#    also delete any engine->playback link before measuring.
# NOTE: no `set -u`. demiurge-env.sh probes optional variables (DEMIURGE_RT_CORE,
# DEMIURGE_BITS) that are unset outside the launcher, and nounset turns those
# probes into a hard abort partway through the prelude — which silently left the
# RT pin and the bit depth unset the first time this ran.
set -e
cd "$(dirname "$0")"
SHOTS=${1:-40}
GAP=${2:-250}

source /opt/demiurge/lib/demiurge-env.sh

HOST=${HOST_BIN:-$HOME/build-host/demiurge-csound-host}
[ -x "$HOST" ] || { echo "ab: no host binary at $HOST (build it on the Pi)" >&2; exit 1; }
[ -x ./rtdelay ] || make || exit 1

# ksmps MUST track the live quantum. The embedded host refuses to start
# otherwise (by design — a partial control period would need the buffer the
# host exists to remove), and the standalone backend silently rounds -b UP to
# the next ksmps multiple, quietly measuring a different period than the one
# you asked for. Reading it off the running graph keeps both honest.
Q=$(pw-metadata -n settings 2>/dev/null \
    | sed -n "s/.*'clock.force-quantum' value:'\([0-9]\+\)'.*/\1/p" | tail -1)
[ "${Q:-0}" -gt 0 ] 2>/dev/null || Q=$(pw-metadata -n settings 2>/dev/null \
    | sed -n "s/.*'clock.quantum' value:'\([0-9]\+\)'.*/\1/p" | tail -1)

HW_O=$(pw-link -i 2>/dev/null | cut -d: -f1 | sort -u | grep -E '^alsa_output\.' | grep -v snd_aloop | grep -vi hdmi | head -1)
HW_I=$(pw-link -o 2>/dev/null | cut -d: -f1 | sort -u | grep -E '^alsa_input\.'  | grep -v snd_aloop | grep -vi hdmi | head -1)
echo "ab: quantum=$Q  ksmps=$Q  playback=$HW_O  capture=$HW_I"

CS_ARGS=(-d -+ignore_csopts=1 -r 48000 --realtime "--omacro:KSMPS=$Q" passthrough.csd)

# PIPEWIRE_LATENCY must be the quantum under test, for BOTH engines.
# demiurge-env.sh derives it from DEMIURGE_QUANTUM, which only the launcher
# sets, so out-of-band it falls back to 128. Ask csound for -b 64 against a
# 128-frame pipewire-jack period and rtjack refuses outright —
# "*** rtjack: buffer size (-B) is too small" — and exits in 0.06 s, so the
# standalone fixture never registers a port and the whole comparison is missing
# its "before" column. It fails loudly in csound's own log and silently in the
# summary, which is the worst combination.
export PIPEWIRE_LATENCY="$Q/48000"
export DEMIURGE_QUANTUM="$Q"

# Pre-flight: refuse to measure through a dirty graph. An orphaned engine from
# an earlier run is not a cosmetic problem — it is a second full Csound at
# SCHED_FIFO, it perturbs every number here, and enough of them will take the
# board down via the RT watchdog. This exact situation produced a bimodal
# "embedded" result that read BELOW the hardware floor, which is impossible,
# and then hard-reset the Pi.
if pgrep -f 'demiurge-csound-host|jack_client=cspt|nullclient' >/dev/null; then
    echo "ab: orphaned fixture engines are still running — refusing to measure:" >&2
    pgrep -af 'demiurge-csound-host|jack_client=cspt|nullclient' >&2
    exit 1
fi

ENG=""; HOLD=""
# `|| true` on every line, deliberately. Under `set -e`, `wait` on a process we
# just killed returns 143, and that propagated out of the function and aborted
# the script between the standalone and embedded passes — losing the one number
# the whole run exists to produce, with no error message.
stop_engine() {
    [ -n "$HOLD" ] && kill "$HOLD" 2>/dev/null || true
    HOLD=""
    if [ -n "$ENG" ]; then kill "$ENG" 2>/dev/null || true; wait "$ENG" 2>/dev/null || true; fi
    ENG=""
    sleep 2
    return 0
}
trap stop_engine EXIT INT TERM

# Wire an engine's INPUT from capture and keep its OUTPUT off the DAC. Held for
# the run: the launcher scrubs hardware playback links it does not own, and
# WirePlumber may re-assert an autoconnect we did not ask for.
hold_links() {
    local c="$1"
    ( while :; do
        pw-link    "$HW_I:capture_FL" "$c:input1"        2>/dev/null
        pw-link    "$HW_I:capture_FR" "$c:input2"        2>/dev/null
        pw-link -d "$c:output1" "$HW_O:playback_FL"      2>/dev/null
        pw-link -d "$c:output2" "$HW_O:playback_FR"      2>/dev/null
        sleep 1
      done ) & HOLD=$!
}

wait_for() {
    for _ in $(seq 1 300); do
        pw-link -o 2>/dev/null | grep -q "^$1:output1$" && return 0
        sleep 0.2
    done
    echo "ab: $1 never registered a JACK port" >&2
    return 1
}

echo; echo "================ FLOOR (rtdelay.sh: no client in the return path) ================"
./rtdelay.sh "$SHOTS" "$GAP" 2>&1 | tail -8

# The floor above is NOT the baseline for the two rows below, and treating it as
# one produces impossible results (see nullclient.c). rtdelay.sh and
# rtdelay-through.sh measure different graphs; inserting any client at all
# changes rtdelay's scheduling relative to the capture node. So we measure the
# THROUGH graph's own zero point, with a client that provably adds nothing.
echo; echo "================ NULL CLIENT (through-graph baseline, adds nothing) ================"
[ -x ./nullclient ] || gcc -O2 -o nullclient nullclient.c -ljack || exit 1
pw-jack ./nullclient </dev/null >/tmp/ab-null.log 2>&1 &
ENG=$!
if wait_for nullclient; then
    hold_links nullclient
    sleep 2
    RTIN=nullclient:output1 ./rtdelay-through.sh "$SHOTS" "$GAP" 2>&1 | tail -8
else
    tail -5 /tmp/ab-null.log
fi
stop_engine

echo; echo "================ STANDALONE  csound -+rtaudio=jack -B $((Q*2)) -b $Q ================"
chrt --fifo 74 csound -+rtaudio=jack -odac -iadc -+jack_client=cspt_std \
     -b "$Q" -B "$((Q*2))" "${CS_ARGS[@]}" </dev/null >/tmp/ab-std.log 2>&1 &
ENG=$!
if wait_for cspt_std; then
    hold_links cspt_std
    sleep 2
    RTIN=cspt_std:output1 ./rtdelay-through.sh "$SHOTS" "$GAP" 2>&1 | tail -8
else
    tail -5 /tmp/ab-std.log
fi
stop_engine

echo; echo "================ EMBEDDED  demiurge-csound-host (in-callback) ================"
DEMIURGE_JACK_CLIENT=cspt_emb \
    chrt --fifo 74 pw-jack "$HOST" "${CS_ARGS[@]}" </dev/null >/tmp/ab-emb.log 2>&1 &
ENG=$!
if wait_for cspt_emb; then
    hold_links cspt_emb
    sleep 2
    RTIN=cspt_emb:output1 ./rtdelay-through.sh "$SHOTS" "$GAP" 2>&1 | tail -8
else
    tail -5 /tmp/ab-emb.log
fi
stop_engine

echo; echo "=== host banner ==="; grep demiurge-csound-host /tmp/ab-emb.log | head -3

# Final sweep. The trap kills what this script started; this catches anything
# that outlived it. Leaving an embedded host running is the one failure mode
# this whole file is careful about.
pkill -f 'jack_client=cspt' 2>/dev/null || true
pkill -f 'nullclient' 2>/dev/null || true
pkill -f "$(basename "$HOST").*passthrough" 2>/dev/null || true
