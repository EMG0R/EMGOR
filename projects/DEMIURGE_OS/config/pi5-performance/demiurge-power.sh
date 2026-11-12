#!/bin/bash
#
# demiurge-power.sh — apply live.conf `power =` to the CPU. THE one power
# mechanism: replaces demiurge-cpu-governor.{sh,service} and
# neptr/scripts/demiurge-power-restore (both retired).
# Install to: /usr/local/bin/demiurge-power.sh
# Called by:  demiurge-power.service (root oneshot, boot, Before=demiurge.service)
#             and `demiurge-power <level>` (restarts the unit after writing live.conf)
#
# live.conf keys (live.conf is the only config surface):
#   power         = low | medium | high      default MEDIUM (EMGOR 2026-10-21)
#   cpu_max_mhz   = <MHz>                    optional, overrides the table's max
#   cpu_cores     = <N>                      optional, overrides the table's core count
# If there is no `power =` key but the file carries a `# power: <level>` header
# (what neptr/scripts/sets/*.conf ship), that tag is read as a fallback. The key
# always wins.
#
#   level   cores online                      governor      min..max
#   low     cpu0 + isolated core + cpu1 (3)   performance   2000..2000 MHz flat
#   medium  all                               performance   1500..2400 (stock)
#   high    all                               performance   1500..cpuinfo_max_freq
#
# WHY low IS NOT 1.5 GHz ANY MORE: flat pinning avoids di/dt sag on weak
# supplies, but 2.0 GHz is the floor the NAM-loaded engine needs — at 1.5 GHz
# csound measured ~90-100% of a core and xruns. Low saves power by shedding
# cores, not by starving the one that carries the audio.
#
# `high` relies on the overclock block in config/pi5-performance/config.txt.snippet
# (arm_freq=2800 etc., installed by setup Phase 8): cpuinfo_max_freq is whatever
# config.txt allows, so without that block high == stock 2400.
#
# DEFAULT IS `medium` (EMGOR, 2026-10-21): stock 2400 ceiling, no overclock, so
# an unconfigured rig never silently runs overclocked. `high` stays available
# and still works — it is opt-in by writing `power = high` in live.conf, and it
# only exceeds 2400 if the config.txt overclock block was also applied.
#
# Never offlines cpu0 or the isolated core. Core onlining can fail on a weak
# supply (EINVAL / psci) — logged, never fatal. Always exits 0: a power knob
# must not be able to fail a boot or hold up demiurge.service.
#
# Test hooks: DEMIURGE_SYSFS_ROOT (prefix for /sys), DEMIURGE_LIVE_CONF,
# DEMIURGE_BOOT_CONF.

set -u

SYS="${DEMIURGE_SYSFS_ROOT:-}/sys/devices/system/cpu"
BOOT_CONF="${DEMIURGE_BOOT_CONF:-/boot/firmware/demiurge.conf}"

log() { echo "[demiurge-power] $*"; }

# ── find live.conf the way the launcher does ────────────────────────────────
# /boot/firmware/demiurge.conf `launch =` first, else the UID-1000 user's
# ~/demiurge/live.conf. Derived, never hardcoded.
find_live() {
    if [[ -n "${DEMIURGE_LIVE_CONF:-}" ]]; then echo "$DEMIURGE_LIVE_CONF"; return; fi
    local l=""
    if [[ -f "$BOOT_CONF" ]]; then
        l="$(sed -n 's/#.*//; s/^[[:space:]]*launch[[:space:]]*=[[:space:]]*//p' "$BOOT_CONF" \
             | head -1 | sed 's/[[:space:]]*$//')"
    fi
    if [[ -n "$l" && -f "$l" ]]; then echo "$l"; return; fi
    local home; home="$(getent passwd 1000 | cut -d: -f6)"
    [[ -n "$home" ]] && echo "$home/demiurge/live.conf"
}

LIVE="$(find_live)"
read_key() {
    [[ -f "$LIVE" ]] || return 0
    grep -m1 -E "^[[:space:]]*$1[[:space:]]*=" "$LIVE" 2>/dev/null \
        | sed 's/^[^=]*=[[:space:]]*//; s/#.*//; s/[[:space:]]*$//'
}
norm_level() {
    case "$(tr '[:upper:]' '[:lower:]' <<<"$1")" in
        low) echo low ;; med|medium) echo medium ;; high) echo high ;; *) echo "" ;;
    esac
}

raw="$(read_key power)"; src="key"
if [[ -z "$raw" && -f "$LIVE" ]]; then
    raw="$(sed -n 's/^#[[:space:]]*power:[[:space:]]*//p' "$LIVE" | head -1 | tr -d '[:space:]')"
    src="tag"
fi
LEVEL="$(norm_level "$raw")"
if [[ -z "$LEVEL" ]]; then
    [[ -n "$raw" ]] && log "unrecognised power '$raw' in $LIVE — using medium"
    LEVEL=medium; src="default"
fi

# ── topology ────────────────────────────────────────────────────────────────
ISO="$(cat "$SYS/isolated" 2>/dev/null | tr -d '[:space:]')"
ISO="${ISO%%[,-]*}"; [[ "$ISO" =~ ^[0-9]+$ ]] || ISO=3

ncpu=0
for d in "$SYS"/cpu[0-9]*; do [[ -d "$d" ]] && ncpu=$((ncpu+1)); done
[[ $ncpu -gt 0 ]] || { log "no cpus under $SYS — nothing to do"; exit 0; }

case "$LEVEL" in
    low)    CORES=3;      MIN=2000; MAX=2000 ;;
    medium) CORES=$ncpu;  MIN=1500; MAX=2400 ;;
    high)   CORES=$ncpu;  MIN=1500
            MAX=$(( $(cat "$SYS/cpu0/cpufreq/cpuinfo_max_freq" 2>/dev/null || echo 2400000) / 1000 )) ;;
esac

ov="$(read_key cpu_max_mhz)"; [[ "$ov" =~ ^[0-9]+$ ]] && { MAX=$ov; [[ $MIN -gt $MAX ]] && MIN=$MAX; }
ov="$(read_key cpu_cores)";   [[ "$ov" =~ ^[0-9]+$ && $ov -ge 1 ]] && CORES=$ov
[[ $CORES -gt $ncpu ]] && CORES=$ncpu

# ── which cores stay online: cpu0, the isolated core, then lowest-numbered ──
declare -a KEEP=(0)
[[ "$ISO" != 0 && -d "$SYS/cpu$ISO" ]] && KEEP+=("$ISO")
for ((i=1; i<ncpu && ${#KEEP[@]}<CORES; i++)); do
    [[ " ${KEEP[*]} " == *" $i "* ]] || KEEP+=("$i")
done

wr() {   # wr <value> <file> — log and continue on failure
    echo "$1" > "$2" 2>/dev/null || log "could not write $1 to ${2#${DEMIURGE_SYSFS_ROOT:-}}"
}

# Online first (so the freq loop below covers every core that will run),
# offline last. cpu0 has no online file and is never touched.
for ((i=1; i<ncpu; i++)); do
    [[ " ${KEEP[*]} " == *" $i "* ]] && wr 1 "$SYS/cpu$i/online"
done
for ((i=1; i<ncpu; i++)); do
    [[ " ${KEEP[*]} " == *" $i "* ]] || { [[ "$i" == "$ISO" ]] || wr 0 "$SYS/cpu$i/online"; }
done

# Order matters: widen the window first, then close it, so min>max never trips.
for c in "${KEEP[@]}"; do
    f="$SYS/cpu$c/cpufreq"
    [[ -d "$f" ]] || continue
    wr performance "$f/scaling_governor"
    wr "$(cat "$f/cpuinfo_min_freq" 2>/dev/null || echo 1500000)" "$f/scaling_min_freq"
    wr $((MAX*1000)) "$f/scaling_max_freq"
    wr $((MIN*1000)) "$f/scaling_min_freq"
done

log "power=$LEVEL ($src) cores=${KEEP[*]} governor=performance ${MIN}..${MAX} MHz (conf: ${LIVE:-none})"
exit 0
