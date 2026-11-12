#!/bin/bash
#
# demiurge-coremap.sh — pin the Demiurge audio processes to their cores.
# Install to: /usr/local/bin/demiurge-coremap   (NOT installed by anything yet)
#
# Part of the demiurge-power family: demiurge-power.sh sets frequency/governor/
# cores online; this sets process->core placement + SCHED_FIFO priority, using
# the SAME mechanism the wrappers already use (`taskset -c N chrt --fifo P`,
# cf. DEMIURGE_RT_PIN in src/wrappers/demiurge-env.sh). It is not a second
# pinning system: `exec` mode just generalises DEMIURGE_RT_PIN to a per-role core.
#
# Map (config in demiurge-coremap.conf):
#   utility -> core 0     csound -> core 1     cardinal -> core 2     jack -> core 3
#
# INERT BY DEFAULT. Every mode except `plan` and `help` refuses unless
#   DEMIURGE_COREMAP_CONFIRM=bench-not-live
# is set in the environment. `plan` only prints. Nothing runs at boot, no unit
# calls this, and it never restarts a service.
#
# Modes:
#   plan                     print the intended pinning and what is currently running (read-only)
#   exec <role> -- cmd...    start NEW process pinned+RT (for wrappers: csound|cardinal)
#   apply                    re-pin ALREADY RUNNING processes (taskset -pc / chrt -f -p).
#                            Bench only, engine idle. Needs a SECOND confirm:
#                            DEMIURGE_COREMAP_ALLOW_RUNNING=yes
#   verify                   print affinity/prio of each role (read-only)
#
# Must not be applied during a performance. EMGOR owns cmdline.txt/throttling.

set -euo pipefail

CONF="${DEMIURGE_COREMAP_CONF:-/etc/demiurge/coremap.conf}"
# defaults (overridden by conf)
UTIL_CPUS=0; CSOUND_CPU=1; CARDINAL_CPU=2; JACK_CPU=3
CSOUND_PRIO=74; CARDINAL_PRIO=70
CSOUND_COMM="csound demiurge-csound-host"
CARDINAL_COMM="Cardinal CardinalNative"
PIPEWIRE_COMM="pipewire"
# shellcheck disable=SC1090
[[ -f "$CONF" ]] && . "$CONF"

log() { echo "[demiurge-coremap] $*"; }
die() { echo "[demiurge-coremap] REFUSED: $*" >&2; exit 2; }

need_confirm() {
    [[ "${DEMIURGE_COREMAP_CONFIRM:-}" == "bench-not-live" ]] \
        || die "set DEMIURGE_COREMAP_CONFIRM=bench-not-live (bench only, never during a performance)"
}

isolated() { tr -d '[:space:]' < /sys/devices/system/cpu/isolated 2>/dev/null || true; }

pids_of() {   # pids_of "<comm> <comm>"
    local c
    for c in $1; do pgrep -x "$c" 2>/dev/null || true; done
}

role_cpu()  { case "$1" in
    utility) echo "$UTIL_CPUS";; csound) echo "$CSOUND_CPU";;
    cardinal) echo "$CARDINAL_CPU";; jack) echo "$JACK_CPU";;
    *) return 1;; esac; }
role_prio() { case "$1" in
    csound) echo "$CSOUND_PRIO";; cardinal) echo "$CARDINAL_PRIO";; *) return 1;; esac; }

cmd_plan() {
    log "isolated cores now: '$(isolated)' (layout wants 1-3; see cmdline.txt.coremap)"
    log "utility  -> cpu $UTIL_CPUS (launcher/UI/clock/OSC/updater; via systemd CPUAffinity=0)"
    log "csound   -> cpu $CSOUND_CPU  fifo $CSOUND_PRIO   [$CSOUND_COMM]"
    log "cardinal -> cpu $CARDINAL_CPU  fifo $CARDINAL_PRIO   [$CARDINAL_COMM]"
    log "jack     -> cpu $JACK_CPU  (pipewire data-loop.* threads only; prio left at pipewire's own)"
    local r p
    for r in csound cardinal; do
        local comms="$CSOUND_COMM"; [[ "$r" == cardinal ]] && comms="$CARDINAL_COMM"
        for p in $(pids_of "$comms"); do
            log "  running $r pid $p: $(taskset -pc "$p" 2>&1 | sed 's/.*: //')"
        done
    done
}

cmd_verify() {
    local p
    for p in $(pids_of "$CSOUND_COMM $CARDINAL_COMM $PIPEWIRE_COMM"); do
        printf '%s %s ' "$p" "$(cat /proc/$p/comm 2>/dev/null)"
        taskset -pc "$p" 2>&1 | sed 's/.*: //' | tr '\n' ' '
        chrt -p "$p" 2>&1 | sed -n 's/.*policy: //p;s/.*priority: /prio /p' | tr '\n' ' '
        echo
    done
    log "data-loop threads:"
    for p in $(pgrep -x pipewire 2>/dev/null || true); do
        ps -L -o tid=,psr=,comm= -p "$p" | grep -E 'data-loop' || true
    done
}

pin_pid() {   # pin_pid <pid> <cpus> [prio]
    taskset -a -pc "$2" "$1" >/dev/null
    [[ -n "${3:-}" ]] && chrt -f -p "$3" "$1"
    log "pid $1 ($(cat /proc/$1/comm)) -> cpu $2${3:+ fifo $3}"
}

cmd_apply() {
    need_confirm
    [[ "${DEMIURGE_COREMAP_ALLOW_RUNNING:-}" == "yes" ]] \
        || die "apply re-pins LIVE processes; also set DEMIURGE_COREMAP_ALLOW_RUNNING=yes (engine idle, bench)"
    local iso; iso="$(isolated)"
    [[ "$iso" == "1-3" || "$iso" == "1,2,3" ]] \
        || die "isolated cores are '$iso', not 1-3: cmdline.txt change not applied/rebooted yet"
    local p t
    for p in $(pids_of "$CSOUND_COMM"); do pin_pid "$p" "$CSOUND_CPU" "$CSOUND_PRIO"; done
    for p in $(pids_of "$CARDINAL_COMM"); do pin_pid "$p" "$CARDINAL_CPU" "$CARDINAL_PRIO"; done
    # JACK = pipewire: whole process to the management core, ONLY data-loop threads to core 3.
    for p in $(pgrep -x pipewire 2>/dev/null || true); do
        taskset -a -pc "$UTIL_CPUS" "$p" >/dev/null
        for t in $(ps -L -o tid=,comm= -p "$p" | awk '/data-loop/{print $1}'); do
            taskset -pc "$JACK_CPU" "$t" >/dev/null
            log "pipewire tid $t (data-loop) -> cpu $JACK_CPU"
        done
    done
}

cmd_exec() {
    need_confirm
    local role="${1:-}"; shift || true
    [[ "${1:-}" == "--" ]] && shift
    [[ $# -gt 0 ]] || die "usage: exec <csound|cardinal|jack> -- cmd args..."
    local cpu prio
    cpu="$(role_cpu "$role")" || die "unknown role '$role'"
    prio="$(role_prio "$role")" || die "role '$role' has no RT priority via exec"
    exec taskset -c "$cpu" chrt --fifo "$prio" "$@"
}

case "${1:-help}" in
    plan)   cmd_plan ;;
    verify) cmd_verify ;;
    apply)  cmd_apply ;;
    exec)   shift; cmd_exec "$@" ;;
    *)      sed -n '2,32p' "$0" ;;
esac
