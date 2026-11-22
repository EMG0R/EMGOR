#!/bin/bash
# DEMIURGE environment prelude.
# Sourced by every demiurge-run-* wrapper. Single source of truth for
# the env-var landmines every audio language needs to avoid on DEMIURGE.

# pipewire-jack shim: forces apps that dynamically link libjack.so.0 to
# load PipeWire's drop-in replacement instead of real jackd2. Without
# this, any app that pulled in libjack-jackd2-dev at build time silently
# hangs waiting for a jackd server that does not exist on DEMIURGE.
export LD_LIBRARY_PATH="/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Kill PipeWire/WirePlumber auto-connect heuristics for every node we
# launch. Without this, WirePlumber links each new client straight to
# the default sink in parallel with whatever the launcher's patch graph
# wants, producing double-audio and phantom links.
export PIPEWIRE_PROPS="node.autoconnect=false node.dont-reconnect=true${PIPEWIRE_PROPS:+ $PIPEWIRE_PROPS}"

# Headless Qt for any Qt-based audio tool (SuperCollider sclang uses Qt
# for its IDE GUI even when running a script). offscreen lets sclang
# boot without a display.
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"

# Audio defaults. PIPEWIRE_LATENCY AUTO-FOLLOWS the graph: it derives from
# DEMIURGE_QUANTUM / DEMIURGE_RATE, which the launcher injects into every wrapper
# from live.conf (`quantum =` / `rate =`). So the quantum lives in ONE place —
# live.conf — and this matches it automatically (no second number to keep in
# sync). A client buffer smaller than the graph quantum makes pipewire-jack
# silently produce no audio, which is exactly what this derivation prevents.
export PIPEWIRE_LATENCY="${PIPEWIRE_LATENCY:-${DEMIURGE_QUANTUM:-128}/${DEMIURGE_RATE:-48000}}"

# OSC base port. Unlike MIDI, OSC has no kernel merge bus — there is no
# "Midi Through" for OSC, because OSC is point-to-point and *addressed*,
# not broadcast. So DEMIURGE does not run an OSC pool/router; each stage
# that wants OSC opens its own socket. What DEMIURGE provides is a single
# reserved, collision-free port range so stages and tooling agree on
# where to listen without hardcoding.
#
#   Reserved range: DEMIURGE_OSC_BASE .. DEMIURGE_OSC_BASE+99  (9000–9099)
#   Convention:     a chain stage at index N listens on BASE + N.
#   Override:       export DEMIURGE_OSC_BASE before launch to relocate it.
#
# 9000 is clear of SuperCollider (scsynth 57110 / sclang 57120) and the
# RNBO runner's own control port (UDP 1234, its app default — documented
# in demiurge/docs/osc.md, not part of this range). See osc.md for the
# full model and per-language usage.
export DEMIURGE_OSC_BASE="${DEMIURGE_OSC_BASE:-9000}"

# ---------------------------------------------------------------------------
# Realtime CPU pin: DEMIURGE_RT_PIN
#
# The Pi 5 tuning boots with `isolcpus=<N>` — the general scheduler then
# never puts anything ON that core, but nothing moves TO it either. It is
# reserved, not used, unless something explicitly asks for it.
#
# Measured 2026-08-10 on the HiFiBerry: with the engine floating on the
# shared cores alongside the UI it ran SCHED_OTHER at ~90-100% of a core
# and produced ~10 buffer underruns per minute. Moving ONLY the engine
# process onto the isolated core (still SCHED_FIFO 74) gave 0 xruns.
#
# ONLY the engine belongs there. Pinning the whole service — launcher,
# wrappers, pw-link helpers — onto the same core (systemd AllowedCPUs=3)
# measured WORSE than not pinning at all: those helpers then preempt the
# very engine the core was reserved for. See config/systemd/
# demiurge.service.d/cpu-affinity.conf.
#
# Core selection, first match wins:
#   1. DEMIURGE_RT_CORE in the environment
#   2. `rt_core =` in live.conf   (a number, or `off` to disable pinning)
#   3. the last core listed in the kernel's isolcpus= token
#   4. no pin at all (empty DEMIURGE_RT_PIN — every wrapper still runs
#      chrt, so an untuned box degrades to plain SCHED_FIFO)
_dem_rt_core() {
    [[ -n "$DEMIURGE_RT_CORE" ]] && { echo "$DEMIURGE_RT_CORE"; return; }
    local live="${DEMIURGE_LIVE_CONF:-$HOME/demiurge/live.conf}" v
    v="$(grep -m1 -E '^[[:space:]]*rt_core[[:space:]]*=' "$live" 2>/dev/null \
         | sed 's/^[^=]*=[[:space:]]*//; s/[[:space:]]*$//; s/#.*//')"
    [[ -n "$v" ]] && { echo "$v"; return; }
    sed -n 's/.*isolcpus=\([0-9,-]*\).*/\1/p' /proc/cmdline 2>/dev/null \
        | tr ',' '\n' | tail -1 | sed 's/.*-//'
}
_DEM_RT_CORE="$(_dem_rt_core)"
if [[ "$_DEM_RT_CORE" =~ ^[0-9]+$ ]] && command -v taskset >/dev/null 2>&1; then
    export DEMIURGE_RT_PIN="taskset -c $_DEM_RT_CORE"
else
    export DEMIURGE_RT_PIN=""
fi
unset _DEM_RT_CORE

# ---------------------------------------------------------------------------
# Sample format: DEMIURGE_BITS
#
# What the engine hands ALSA. Nothing negotiates this for you — measured
# 2026-08-10, csound's ALSA backend defaults to S16_LE and `plughw` silently
# accepts it, so a 24-bit DAC was running with its bottom 8 bits discarded
# (raised noise floor; no latency cost either way).
#
#   bits = 32      32-bit int  (default — S32_LE; plughw feeds the card's
#                  native S24_3LE/S32_LE with no conversion cost)
#   bits = float   32-bit float
#   bits = 16      16-bit int  (only if a device genuinely refuses more)
#
# Set in live.conf (`bits =`) or via DEMIURGE_BITS in the environment.
# Platform-independent by design: it names the FORMAT, and each wrapper maps
# it to its own engine's flag — so the same live.conf key means the same
# thing on an I2S HAT, a USB interface or HDMI. csound: -l / -f / -s.
# NOTE 24-bit is deliberately absent: csound's realtime ALSA rejects `-3`
# ("Unknown sample format" — 16/32-bit int and 32-bit float only). 32-bit
# int is the correct way to feed a 24-bit converter.
_dem_bits() {
    [[ -n "$DEMIURGE_BITS" ]] && { echo "$DEMIURGE_BITS"; return; }
    local live="${DEMIURGE_LIVE_CONF:-$HOME/demiurge/live.conf}" v
    v="$(grep -m1 -E '^[[:space:]]*bits[[:space:]]*=' "$live" 2>/dev/null \
         | sed 's/^[^=]*=[[:space:]]*//; s/[[:space:]]*$//; s/#.*//')"
    echo "${v:-32}"
}
export DEMIURGE_BITS="$(_dem_bits)"
