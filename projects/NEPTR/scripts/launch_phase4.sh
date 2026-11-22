#!/bin/bash
#
# NEPTR Phase 4 — single-entry launcher.
#
# Audio path (everyone routes through the DEMIURGE virtual hub, never
# direct to hardware):
#
#   demiurge-source ─┐
#   chuck out_1/2  ─┴──> csound input1/2
#   csound out 1/2 ────> demiurge-sink:playback_FL/FR ──> Scarlett (via aggregate)
#
# The DEMIURGE virtual hub (`demiurge-sink` / `demiurge-source`) is a
# pair of always-present 4-channel loopback nodes defined in
# /etc/pipewire/pipewire.conf.d/demiurge-virtual.conf. Stages link
# only to the hub. A separate, idempotent "aggregate" pass links the
# hub to whatever physical USB sinks/sources exist right now —
# unplug/replug rewires the hub-to-hardware edges without ever
# touching the chain stages, so audio keeps flowing.
#
# What the launcher does, in order:
#  1. Wait for the virtual hub to be live (60 s budget).
#  2. Re-pin pw-metadata clock.force-rate=48000 + clock.force-quantum=128
#     so a graph reset / wireplumber reload can't drag the rate.
#  3. Run the USB-aggregate pass once (link hub <-> live USB devices).
#  4. Start csound, locked at sr=48000 / ksmps=128 (matches the .csd).
#  5. Start chuck, with --srate:48000 forced (so it never inherits a
#     transient JACK rate that doesn't match the graph).
#  6. Wait for stage ports, wire chain stages to the hub.
#  7. Bridge ALSA seq "Midi Through" → snd-virmidi so csound -Ma and
#     claude_midi.sh share the same MIDI bus as the Teensy.
#  8. Spawn a hot-plug watcher: every 2 s, re-run the USB-aggregate
#     pass (idempotent — no-op if nothing changed). Hot-unplug or
#     replug of the Scarlett re-converges within ~2 s with no audio
#     interruption to the chain stages, which only see the hub.
#
# Ctrl+C / SIGTERM tears everything down cleanly.

set -u

# ---- locate root + load config ---------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CONF="$ROOT/phase4.conf"

if [ ! -f "$CONF" ]; then
    echo "[phase4] FATAL: $CONF not found" >&2
    exit 1
fi

# strip comments and blank lines, parse KEY = VALUE
get_conf() {
    local key="$1"
    grep -E "^[[:space:]]*${key}[[:space:]]*=" "$CONF" \
        | head -1 \
        | sed -E "s/^[[:space:]]*${key}[[:space:]]*=[[:space:]]*//; s/[[:space:]]*$//"
}

CSD_REL="$(get_conf csound_main)"
CSD_JACK_NAME="$(get_conf csound_jack_name)"
CSD_IGNORE_OPTS="$(get_conf csound_ignore_csopts)"
PRESET_BANK="$(get_conf preset_bank)"
PRESET_BANK="${PRESET_BANK:-0}"
CHUCK_FILES="$(get_conf chuck_files)"
CHUCK_JACK_NAME="$(get_conf chuck_jack_name)"
CHUCK_JACK_DEV_MATCH="$(get_conf chuck_jack_device_match)"

# Pinned graph rate / quantum. The static config in
# /etc/pipewire/pipewire.conf.d/99-demiurge-lowlatency.conf already
# sets these defaults, but pw-metadata is the runtime enforcement —
# anything that resets the graph (wireplumber reload, hot-plug at the
# wrong moment) can drop them. We re-assert them every launch.
GRAPH_RATE=48000
GRAPH_QUANTUM=128

# pipewire-jack shim — required so csound and chuck use PipeWire's
# libjack instead of jackd2's libjack (which would hang waiting for
# jackd). Both binaries are linked to libjack.so.0.
PIPEWIRE_JACK_LD="/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack"

CSD_ABS="$ROOT/$CSD_REL"
if [ ! -f "$CSD_ABS" ]; then
    echo "[phase4] FATAL: csound_main not found: $CSD_ABS" >&2
    exit 1
fi

# ---- pidfile + cleanup -----------------------------------------------------
LOG_DIR="$ROOT/.run"
mkdir -p "$LOG_DIR"
PIDFILE_CSD="$LOG_DIR/csound.pid"
PIDFILE_CK="$LOG_DIR/chuck.pid"
LOG_CSD="$LOG_DIR/csound.log"
LOG_CK="$LOG_DIR/chuck.log"
LOG_AGG="$LOG_DIR/aggregate.log"

CHUCK_PIDS=()
CSD_PID=""
WATCHER_PID=""
LINKS_MADE=()

cleanup() {
    echo ""
    echo "[phase4] tearing down…"
    if [ -n "${WATCHER_PID:-}" ] && kill -0 "$WATCHER_PID" 2>/dev/null; then
        kill "$WATCHER_PID" 2>/dev/null || true
    fi
    # remove pw-links the LAUNCHER made (chain stages <-> hub).
    # Hub <-> hardware links are owned by the aggregate pass and stay.
    for L in "${LINKS_MADE[@]}"; do
        # shellcheck disable=SC2086
        pw-link -d $L >/dev/null 2>&1 || true
    done
    if [ -n "${CSD_PID:-}" ] && kill -0 "$CSD_PID" 2>/dev/null; then
        kill "$CSD_PID" 2>/dev/null || true
        sleep 0.3
        kill -9 "$CSD_PID" 2>/dev/null || true
    fi
    for P in "${CHUCK_PIDS[@]}"; do
        if kill -0 "$P" 2>/dev/null; then
            kill "$P" 2>/dev/null || true
            sleep 0.2
            kill -9 "$P" 2>/dev/null || true
        fi
    done
    rm -f "$PIDFILE_CSD" "$PIDFILE_CK"
    echo "[phase4] done."
}
trap cleanup EXIT INT TERM

# ===========================================================================
# 0) wait for hub + pin graph rate/quantum
# ===========================================================================

# The DEMIURGE virtual hub is created by /etc/pipewire/pipewire.conf.d/
# demiurge-virtual.conf and is normally up within ~1s of pipewire
# starting. On cold boot we may briefly run before pipewire has
# instantiated the loopback modules — block here until we see the
# canonical port, with a 60 s budget. Same gate the demiurge.service
# uses (see /etc/systemd/system/demiurge.service.d/wait-pipewire.conf).
echo "[phase4] waiting for demiurge-sink:playback_FL …"
HUB_WAIT=0
while [ "$HUB_WAIT" -lt 120 ]; do
    if pw-link -i 2>/dev/null | grep -q "^demiurge-sink:playback_FL$"; then
        break
    fi
    sleep 0.5
    HUB_WAIT=$((HUB_WAIT + 1))
done
if [ "$HUB_WAIT" -ge 120 ]; then
    echo "[phase4] FATAL: demiurge-sink:playback_FL never appeared (60s)" >&2
    echo "[phase4]   check that pipewire/wireplumber are running and" >&2
    echo "[phase4]   /etc/pipewire/pipewire.conf.d/demiurge-virtual.conf is installed." >&2
    exit 4
fi
echo "[phase4]   hub ready (after ${HUB_WAIT}× 0.5s)"

# Re-assert graph rate + quantum at runtime. The static config sets the
# defaults; pw-metadata is the runtime enforcement. Idempotent.
echo "[phase4] pin graph: rate=${GRAPH_RATE} quantum=${GRAPH_QUANTUM}"
pw-metadata -n settings 0 clock.force-rate    "$GRAPH_RATE"    >/dev/null 2>&1 || true
pw-metadata -n settings 0 clock.force-quantum "$GRAPH_QUANTUM" >/dev/null 2>&1 || true

# Ensure snd-virmidi is loaded (needed for the Midi Through bridge
# below). Cheap to call repeatedly.
sudo modprobe -q snd-virmidi midi_devs=1 2>/dev/null || true

# ===========================================================================
# USB AGGREGATE — hub <-> physical hardware linker
# ===========================================================================
# Bash port of demiurge-launcher-rs/src/aggregate.rs. Enumerates every
# `alsa_output.usb-*` sink and `alsa_input.usb-*` source currently in
# the graph, and links them into the next free pair on the hub.
# Idempotent: pw-link rejects duplicates so re-runs converge silently.
#
# Channel plan (must match the [FL FR RL RR] layout in the
# demiurge-virtual.conf loopback nodes):
SINK_PAIRS=(
    "output_FL output_FR"
    "output_RL output_RR"
)
SOURCE_PAIRS=(
    "input_FL input_FR"
    "input_RL input_RR"
)

aggregate_apply() {
    local outs ins sink src idx pair_l pair_r

    # First pass: nuke every existing edge from a USB device (sink or
    # source) into the hub. Then rebuild the desired set fresh below.
    # Without this, WirePlumber's default-source/sink policy and stale
    # edges from previous runs accumulate cross-links (e.g. Mic2 ending
    # up wired to BOTH input_FL and input_RL), which silently sums
    # channels and breaks the channel layout the chain expects.
    pw-link -l 2>/dev/null | awk '
        /^[A-Za-z]/ {hdr=$0; next}
        /\|<- alsa_input\.usb-.*:capture_(FL|FR|MONO)$/ {
            if (hdr ~ /^demiurge-source-input:/) {
                sub(/^[ \t]*\|<- /, "", $0)
                print $0 "\t" hdr
            }
        }
        /\|-> alsa_output\.usb-.*:playback_(FL|FR)$/ {
            if (hdr ~ /^demiurge-sink-output:/) {
                sub(/^[ \t]*\|-> /, "", $0)
                print hdr "\t" $0
            }
        }
    ' | while IFS=$'\t' read -r src dst; do
        pw-link -d "$src" "$dst" >/dev/null 2>&1 || true
    done

    outs="$(pw-link -i 2>/dev/null || true)"
    ins="$(pw-link -o 2>/dev/null || true)"

    # Sinks: alsa_output.usb-<...>:playback_FL — sorted by client name.
    local sinks=()
    while IFS= read -r line; do
        sinks+=("$line")
    done < <(printf '%s\n' "$outs" \
        | awk -F: '/^alsa_output\.usb-/ && /:playback_FL$/ {print $1}' \
        | sort -u)

    idx=0
    for sink in "${sinks[@]}"; do
        if [ "$idx" -ge "${#SINK_PAIRS[@]}" ]; then
            echo "[phase4] aggregate: extra USB sink ignored ($sink)" >> "$LOG_AGG"
            continue
        fi
        # shellcheck disable=SC2206
        local pair=( ${SINK_PAIRS[$idx]} )
        pw-link "demiurge-sink-output:${pair[0]}" "${sink}:playback_FL" >/dev/null 2>&1 || true
        pw-link "demiurge-sink-output:${pair[1]}" "${sink}:playback_FR" >/dev/null 2>&1 || true
        idx=$((idx + 1))
    done

    # Stereo sources first (alsa_input.usb-<...>:capture_FL+FR — one
    # pair each). Then mono sources, which we group two-at-a-time into
    # one stereo pair (Scarlett Solo: Mic1 -> L, Mic2 -> R), so the
    # chain sees a normal stereo capture instead of silence on one side.
    local stereo_sources=() mono_sources=()
    while IFS= read -r src; do
        [ -z "$src" ] && continue
        stereo_sources+=("$src")
    done < <(printf '%s\n' "$ins" \
        | awk -F: '/^alsa_input\.usb-/ && /:capture_FL$/ {print $1}' \
        | sort -u)
    while IFS= read -r src; do
        [ -z "$src" ] && continue
        mono_sources+=("$src")
    done < <(printf '%s\n' "$ins" \
        | awk -F: '/^alsa_input\.usb-/ && /:capture_MONO$/ {print $1}' \
        | sort -u)

    idx=0
    # 1. Stereo: one source = one pair slot.
    for src in "${stereo_sources[@]}"; do
        if [ "$idx" -ge "${#SOURCE_PAIRS[@]}" ]; then
            echo "[phase4] aggregate: extra stereo source ignored ($src)" >> "$LOG_AGG"
            continue
        fi
        # shellcheck disable=SC2206
        local pair=( ${SOURCE_PAIRS[$idx]} )
        pw-link "${src}:capture_FL" "demiurge-source-input:${pair[0]}" >/dev/null 2>&1 || true
        pw-link "${src}:capture_FR" "demiurge-source-input:${pair[1]}" >/dev/null 2>&1 || true
        idx=$((idx + 1))
    done
    # 2. Mono: pair them up two at a time into the next available
    #    stereo slot. An odd trailing mono takes the L of its slot only.
    local i=0
    while [ "$i" -lt "${#mono_sources[@]}" ]; do
        if [ "$idx" -ge "${#SOURCE_PAIRS[@]}" ]; then
            echo "[phase4] aggregate: extra mono source ignored (${mono_sources[$i]})" >> "$LOG_AGG"
            i=$((i + 1)); continue
        fi
        # shellcheck disable=SC2206
        local pair=( ${SOURCE_PAIRS[$idx]} )
        local m_l="${mono_sources[$i]}"
        pw-link "${m_l}:capture_MONO" "demiurge-source-input:${pair[0]}" >/dev/null 2>&1 || true
        if [ $((i + 1)) -lt "${#mono_sources[@]}" ]; then
            local m_r="${mono_sources[$((i + 1))]}"
            pw-link "${m_r}:capture_MONO" "demiurge-source-input:${pair[1]}" >/dev/null 2>&1 || true
            i=$((i + 2))
        else
            i=$((i + 1))
        fi
        idx=$((idx + 1))
    done
}

# Scrub any direct chain-stage <-> hardware links left over from a
# previous launcher run (or speculatively created by WirePlumber).
# Chain stages must route through the hub ONLY — direct chain->Scarlett
# edges defeat the hot-plug recovery story since csound/chuck would lose
# audio when Scarlett vanished and the watcher (which only restores
# hub<->hardware edges) couldn't help them.
scrub_chain_direct() {
    local listing src dst hdr
    listing="$(pw-link -l 2>/dev/null || true)"
    hdr=""
    while IFS= read -r line; do
        if [[ "$line" =~ ^[A-Za-z] ]]; then
            hdr="$line"
            continue
        fi
        # outgoing edge "  |-> dst" attached to current hdr (= source port)
        if [[ "$line" =~ ^[[:space:]]+\|-\>[[:space:]](.+)$ ]]; then
            dst="${BASH_REMATCH[1]}"
            case "$hdr" in
                "$CSD_JACK_NAME":*|"$CHUCK_JACK_NAME":*)
                    case "$dst" in
                        alsa_output.*|alsa_input.*)
                            pw-link -d "$hdr" "$dst" >/dev/null 2>&1 \
                                && echo "[phase4]   scrub: $hdr -X-> $dst" ;;
                    esac
                    ;;
            esac
        fi
        # incoming edge "  |<- src" attached to current hdr (= destination port)
        if [[ "$line" =~ ^[[:space:]]+\|\<-[[:space:]](.+)$ ]]; then
            src="${BASH_REMATCH[1]}"
            case "$hdr" in
                "$CSD_JACK_NAME":*|"$CHUCK_JACK_NAME":*)
                    case "$src" in
                        alsa_input.*|alsa_output.*)
                            pw-link -d "$src" "$hdr" >/dev/null 2>&1 \
                                && echo "[phase4]   scrub: $src -X-> $hdr" ;;
                    esac
                    ;;
            esac
        fi
    done <<< "$listing"
}

echo "[phase4] scrubbing direct chain<->hardware links"
scrub_chain_direct

echo "[phase4] aggregate: linking hub <-> USB hardware"
aggregate_apply

# ===========================================================================
# 1) start csound  (sr/ksmps already pinned in csound/globals.orc)
# ===========================================================================
echo "[phase4] starting csound: $CSD_REL  (jack client: $CSD_JACK_NAME)"

CSD_OPTS=()
if [ "$CSD_IGNORE_OPTS" = "1" ]; then
    CSD_OPTS+=( "-+ignore_csopts=1" )
fi
CSD_OPTS+=(
    "-+rtaudio=jack"
    "-+jack_client=$CSD_JACK_NAME"
    "-+rtmidi=alsa"
    "-odac" "-iadc"
    "-Ma"
    # explicit sr; ksmps stays authoritative in csound/globals.orc
    "-r" "$GRAPH_RATE"
    "-b$GRAPH_QUANTUM" "-B$((GRAPH_QUANTUM * 2))"
    "--omacro:PRESET_BANK=$PRESET_BANK"
    # The .orc/.csd files ship a literal /home/gorpi7/... default behind an
    # #ifndef guard (state_persist.orc, looper_persist.orc, globals.orc,
    # effects/ir_reverb.orc, neptrPhase4.csd) so a rig running as any other
    # Pi user doesn't silently write state into a directory that doesn't
    # exist. These five always win: they're defined on the command line,
    # before csound ever parses the #ifndef, so the file's literal default
    # is never reached on a normal launch — only a manual, direct `csound`
    # invocation (bypassing this script) would hit it.
    "--omacro:STATE_DIR=$HOME/demiurge/state"
    "--omacro:PRESET_DIR=$HOME/demiurge/presets"
    "--omacro:NAM_DIR=$HOME/demiurge/profiles/nam"
    "--omacro:IR_DIR=$HOME/demiurge/ir"
    "--omacro:DRUMLOOP_DIR=$HOME/demiurge/samples/drumloops"
    "--daemon"
)

LD_LIBRARY_PATH="$PIPEWIRE_JACK_LD" \
    csound "${CSD_OPTS[@]}" "$CSD_ABS" >"$LOG_CSD" 2>&1 &
CSD_PID=$!
echo "$CSD_PID" > "$PIDFILE_CSD"
echo "[phase4]   csound pid = $CSD_PID  (log: $LOG_CSD)"

# ===========================================================================
# 2) discover chuck JACK device index
# ===========================================================================
# RtAudio's JACK backend in chuck wants a device index. Probe the live
# JACK graph and pick the first device whose name contains
# CHUCK_JACK_DEV_MATCH and exposes ≥ 2 output channels.
CK_DAC_INDEX=""
if [ -n "$CHUCK_FILES" ]; then
    PROBE_OUT="$(LD_LIBRARY_PATH="$PIPEWIRE_JACK_LD" chuck --driver:jack --probe 2>&1 || true)"
    CK_DAC_INDEX="$(printf '%s\n' "$PROBE_OUT" | awk -v match_str="$CHUCK_JACK_DEV_MATCH" '
        /audio device:/ { idx = $0; sub(/.*device: /, "", idx); sub(/ \).*/, "", idx); name=""; outs=0 }
        /device name =/ { name = $0; sub(/.*= "/, "", name); sub(/"$/, "", name) }
        /# output channels =/ {
            outs = $NF
            if (outs >= 2 && index(name, match_str) > 0 && picked == "") {
                picked = idx
            }
        }
        END { print picked }
    ')"
    # If the configured match misses (e.g. Scarlett unplugged), fall
    # back to the demiurge-sink — it always exists and has ≥2 channels.
    if [ -z "$CK_DAC_INDEX" ]; then
        CK_DAC_INDEX="$(printf '%s\n' "$PROBE_OUT" | awk '
            /audio device:/ { idx = $0; sub(/.*device: /, "", idx); sub(/ \).*/, "", idx); name=""; outs=0 }
            /device name =/ { name = $0; sub(/.*= "/, "", name); sub(/"$/, "", name) }
            /# output channels =/ {
                outs = $NF
                if (outs >= 2 && index(name, "DEMIURGE") > 0 && picked == "") {
                    picked = idx
                }
            }
            END { print picked }
        ')"
        [ -n "$CK_DAC_INDEX" ] && echo "[phase4] chuck dac fallback to DEMIURGE (Scarlett missing)"
    fi
    if [ -z "$CK_DAC_INDEX" ]; then
        echo "[phase4] FATAL: no chuck JACK device with ≥2 outputs (tried '$CHUCK_JACK_DEV_MATCH' and DEMIURGE)" >&2
        printf '%s\n' "$PROBE_OUT" | grep -E 'audio device|device name|output channels' >&2
        exit 3
    fi
    echo "[phase4] chuck dac device index = $CK_DAC_INDEX"
fi

# ===========================================================================
# 3) start chuck file(s) — sample rate explicitly pinned to GRAPH_RATE
# ===========================================================================
for CK_REL in $CHUCK_FILES; do
    CK_ABS="$ROOT/$CK_REL"
    if [ ! -f "$CK_ABS" ]; then
        echo "[phase4] WARN: chuck file not found: $CK_ABS — skipping" >&2
        continue
    fi
    echo "[phase4] starting chuck: $CK_REL  (srate=$GRAPH_RATE)"
    LD_LIBRARY_PATH="$PIPEWIRE_JACK_LD" \
        chuck --driver:jack --dac:"$CK_DAC_INDEX" --out:2 \
        --srate:"$GRAPH_RATE" \
        "$CK_ABS" >>"$LOG_CK" 2>&1 &
    CK_PID=$!
    CHUCK_PIDS+=("$CK_PID")
    echo "$CK_PID" >> "$PIDFILE_CK"
    echo "[phase4]   chuck pid = $CK_PID  (log: $LOG_CK)"
done

# ===========================================================================
# 4) wait for stage ports to register
# ===========================================================================
echo "[phase4] waiting for chain stage JACK ports…"

find_ports() {
    local listing="$1"   # "i" or "o"
    local pattern="$2"
    pw-link "-${listing}" 2>/dev/null | grep -F "$pattern" || true
}

WAIT=0
CSD_INS=""; CSD_OUTS=""; CK_OUTS=""
while [ "$WAIT" -lt 50 ]; do
    CSD_INS="$(find_ports i "$CSD_JACK_NAME")"
    CSD_OUTS="$(find_ports o "$CSD_JACK_NAME")"
    if [ -n "$CHUCK_FILES" ]; then
        CK_OUTS="$(find_ports o "$CHUCK_JACK_NAME")"
    else
        CK_OUTS="ok"
    fi
    csd_in_count=$(printf "%s\n" "$CSD_INS"  | grep -c .)
    csd_out_count=$(printf "%s\n" "$CSD_OUTS" | grep -c .)
    ck_out_count=$(printf "%s\n" "$CK_OUTS"  | grep -c .)
    if [ "$csd_in_count" -ge 2 ] && [ "$csd_out_count" -ge 2 ] \
       && { [ -z "$CHUCK_FILES" ] || [ "$ck_out_count" -ge 2 ]; }; then
        break
    fi
    sleep 0.2
    WAIT=$((WAIT + 1))
done

if [ "$WAIT" -ge 50 ]; then
    echo "[phase4] FATAL: timed out waiting for chain JACK ports." >&2
    tail -20 "$LOG_CSD" >&2 || true
    tail -20 "$LOG_CK" >&2 || true
    exit 2
fi

CSD_IN1=$(printf "%s\n" "$CSD_INS"  | sed -n '1p')
CSD_IN2=$(printf "%s\n" "$CSD_INS"  | sed -n '2p')
CSD_OUT1=$(printf "%s\n" "$CSD_OUTS" | sed -n '1p')
CSD_OUT2=$(printf "%s\n" "$CSD_OUTS" | sed -n '2p')
CK_OUT1=$(printf "%s\n" "$CK_OUTS"  | sed -n '1p')
CK_OUT2=$(printf "%s\n" "$CK_OUTS"  | sed -n '2p')

echo "[phase4] discovered:"
echo "[phase4]   csound  in1  = $CSD_IN1"
echo "[phase4]   csound  in2  = $CSD_IN2"
echo "[phase4]   csound  out1 = $CSD_OUT1"
echo "[phase4]   csound  out2 = $CSD_OUT2"
echo "[phase4]   chuck   out1 = ${CK_OUT1:-<none>}"
echo "[phase4]   chuck   out2 = ${CK_OUT2:-<none>}"

# ===========================================================================
# 5) wire chain stages -> virtual hub (NOT to physical hardware)
# ===========================================================================
make_link() {
    local src="$1"; local dst="$2"
    [ -z "$src" ] || [ -z "$dst" ] && return 0
    if pw-link "$src" "$dst" >/dev/null 2>&1; then
        echo "[phase4]   linked: $src -> $dst"
        LINKS_MADE+=( "\"$src\" \"$dst\"" )
    else
        echo "[phase4]   (already linked or failed): $src -> $dst"
    fi
}

echo "[phase4] wiring chain stages -> hub…"
# Hub source -> csound input (was: Scarlett mic 1/2 direct)
make_link "demiurge-source:capture_FL" "$CSD_IN1"
make_link "demiurge-source:capture_FR" "$CSD_IN2"
# chuck synth -> csound input (mixed with hub source)
make_link "$CK_OUT1" "$CSD_IN1"
make_link "$CK_OUT2" "$CSD_IN2"
# csound output -> hub sink (was: Scarlett line out direct)
make_link "$CSD_OUT1" "demiurge-sink:playback_FL"
make_link "$CSD_OUT2" "demiurge-sink:playback_FR"

# ===========================================================================
# 5b) bridge ALSA seq Midi Through -> snd-virmidi (so csound -Ma sees it)
# ===========================================================================
echo "[phase4] aconnect Midi Through -> Virtual Raw MIDI"
VIRMIDI_CID="$(aconnect -l 2>/dev/null \
    | awk '/client [0-9]+: .Virtual Raw MIDI/ {
        cid=$2; sub(/:/, "", cid); print cid; exit
    }')"
if [ -n "$VIRMIDI_CID" ]; then
    aconnect "Midi Through":0 "${VIRMIDI_CID}:0" 2>/dev/null \
        || echo "[phase4]   (already connected)"
    echo "[phase4]   Midi Through:0 -> ${VIRMIDI_CID}:0"
else
    echo "[phase4]   WARN: snd-virmidi seq client not found — operator MIDI won't reach csound" >&2
fi

# ===========================================================================
# 6) hot-plug watcher — re-aggregate every 2s while we're up
# ===========================================================================
# Polling instead of pw-mon: aggregate_apply is a few subprocess calls
# and is idempotent, so 2-second cadence is fine. On Scarlett unplug,
# the hub's outgoing links to its playback ports vanish (PipeWire
# garbage-collects them); on replug, the next aggregate tick re-creates
# the demiurge-sink-output -> Scarlett:playback_FL/FR edges. The chain
# stages are wired only to the hub, so they never see the disturbance.
echo "[phase4] starting hot-plug watcher (2s aggregate poll)"
(
    while sleep 2; do
        kill -0 "$CSD_PID" 2>/dev/null || exit 0
        # Re-aggregate (idempotent) AND re-scrub (in case WirePlumber
        # re-created speculative chain<->hardware links).
        aggregate_apply
        scrub_chain_direct >/dev/null 2>&1
    done
) &
WATCHER_PID=$!

# ===========================================================================
# 7) run until csound dies or user hits Ctrl+C
# ===========================================================================
echo "[phase4] up. ctrl+c to stop."
wait "$CSD_PID"
echo "[phase4] csound exited."
