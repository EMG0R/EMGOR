#!/bin/bash
#
# DEMIURGE — Raise the threaded-IRQ handlers that PACE AUDIO above the engine.
#
# Installed to: /usr/local/bin/demiurge-audio-irq-rt.sh
# Called by:    demiurge-audio-irq-rt.service (at boot, before demiurge.service)
#
# WHY
# ---
# With `threadirqs` on the cmdline, hardware interrupt handlers run in kernel
# threads (`irq/<N>-<action>`) that default to SCHED_FIFO 50. The engine runs
# at FIFO 74 (set by the demiurge-run-* wrappers). Every engine block is paced
# by a blocking ALSA read/write: the hardware signals a period boundary → the
# IRQ thread runs → the engine wakes. If that IRQ thread queues behind other
# FIFO-50 IRQ threads, the whole period slips. At 256 frames (5.3 ms budget)
# nobody notices; at 64 frames (1.33 ms) tens of µs of wake-up jitter eat a
# meaningful slice of the budget.
#
# FIFO 85 puts the pacing handlers above the engine (74) and above every
# default IRQ thread (50) — the rtirq-style placement for sound-card IRQs.
# These handlers do microseconds of work per event, so nothing can starve.
# Affinity is NOT touched here: demiurge-irq-isolate.sh already keeps all
# IRQs (and their threads) off the isolated core.
#
# WHICH IRQ — DERIVED, NEVER HARDCODED
# ------------------------------------
# There is no such thing as "the audio IRQ" in the abstract: it depends
# entirely on which interface is attached, and DEMIURGE must be equally
# optimised for any of them.
#
#   * A USB interface (any brand) is paced by its USB host controller's IRQ
#     (`xhci-hcd:usbN`, `ehci-hcd:usbN`, `dwc2:usbN`, … — the driver prefix
#     varies by controller and kernel version, the bus name does not).
#   * An I2S HAT (any brand) is paced by the DMA engine that moves the
#     samples — on a Pi 5 that is `dw_axi_dmac_platform`, on other SoCs it is
#     something else entirely. The DMA controller is NOT an ancestor of the
#     sound card in sysfs, so no amount of device-tree walking finds it.
#   * An HDMI or on-SoC codec is paced by yet another engine.
#
# An allowlist of driver names is therefore always wrong for the next
# interface. This script derives the set instead, in two passes:
#
#   PASS 1 — STRUCTURAL (runs immediately, works with no audio playing)
#     a. Every USB host controller present: each `/sys/bus/usb/devices/usbN`
#        root hub is matched to the /proc/interrupts line whose action names
#        that bus. Host-controller IRQ threads exist from boot whether or not
#        an interface is plugged in, so this also covers hot-plug: plug any
#        USB interface in later and its pacing IRQ is already at 85.
#     b. Every sound card present: walk `/sys/class/sound/cardN/device` and
#        its ancestors for an `irq` attribute (PCI/PCIe sound cards and some
#        platform cards expose one; on a Pi 5 nothing does — harmless).
#
#   PASS 2 — ACTIVITY (the general answer; catches everything else)
#     Wait for any ALSA sub-device to reach `state: RUNNING`, read its
#     `hw_params` for the real rate and period size, then sample
#     /proc/interrupts over a 2 s window. Any threaded IRQ advancing at or
#     above the plausible audio floor is, by definition, pacing audio — no
#     matter what it is called or which bus it is on. That is what finds the
#     I2S DMA engine, and it will equally find whatever paces the interface
#     nobody has plugged in yet.
#
# FALLBACK (documented, deliberate): if nothing ever plays inside the watch
# window, pass 2 finds nothing and the script exits 0 having applied pass 1
# only. Re-run it at any time — after a hot-plug, after switching interface,
# after changing quantum — and it re-derives from scratch:
#
#     sudo systemctl restart demiurge-audio-irq-rt.service
#
# It is safe with no interface attached, safe with several, and never fails:
# every step is best-effort and the exit status is always 0, because a
# failing boot unit is worse than an unboosted IRQ.
#
# Verify:
#   ps -eLo rtprio,cls,comm | grep '^\s*85'    # every boosted thread
#   journalctl -u demiurge-audio-irq-rt        # what it derived, and why
#
# History: this was `demiurge-usb-irq-rt.sh`, which matched only
# `irq/N-xhci*`. On an I2S HAT it reported success boosting USB controllers
# that carried no audio while the thread that actually paced every block sat
# at FIFO 50 — below the engine's 74, i.e. exactly the priority inversion the
# script exists to prevent.
set -u

PRIO="${DEMIURGE_AUDIO_IRQ_PRIO:-85}"      # above engine (74) and default IRQ threads (50)
WATCH_SECS="${DEMIURGE_AUDIO_IRQ_WATCH:-180}"   # how long pass 2 waits for audio to start
SAMPLE_SECS="${DEMIURGE_AUDIO_IRQ_SAMPLE:-2}"   # length of the /proc/interrupts window
MIN_HZ="${DEMIURGE_AUDIO_IRQ_MIN_HZ:-200}"      # absolute floor for "this paces audio"

TAG="[demiurge-audio-irq-rt]"
boosted_irqs=" "        # space-delimited set of IRQ numbers already handled
boosted_threads=0

log() { echo "$TAG $*"; }

# --- Raise every kernel thread belonging to IRQ $1 to SCHED_FIFO $PRIO.
# Matches on the `irq/<N>-` prefix only: the action suffix is truncated
# differently across kernels, the prefix never is. Secondary handler threads
# (`irq/<N>-s-<action>`) are matched by the same prefix and boosted too.
boost_irq() {
    local n="$1" why="$2" hit=0 pid comm

    case "$boosted_irqs" in *" $n "*) return 0 ;; esac
    boosted_irqs="$boosted_irqs$n "

    while read -r pid comm; do
        case "$comm" in
            "irq/$n-"*) chrt -f -p "$PRIO" "$pid" 2>/dev/null && hit=$((hit+1)) ;;
        esac
    done <<< "$(ps -eo pid=,comm= 2>/dev/null)"

    if [ "$hit" -gt 0 ]; then
        boosted_threads=$((boosted_threads+hit))
        log "IRQ $n -> FIFO $PRIO ($hit thread(s)) [$why]"
    else
        # No thread: either `threadirqs` is off, or this IRQ has no handler
        # thread. Nothing to do, nothing wrong.
        log "IRQ $n has no handler thread — skipped [$why]"
    fi
}

# --- /proc/interrupts helpers -------------------------------------------
# Line shape: "140:  9988059  0  0  0   rp1_irq_chip  40 Level  dw_axi_dmac_platform"
#              ^irq  ^one column per CPU ^chip        ^hwirq ^type ^action (may
# contain spaces). The CPU-column count varies with core count, so it is taken
# from the header line rather than assumed.

IRQ_MAP=""       # cache of "<irq> <action>" lines, refreshed by load_irq_map

load_irq_map() {
    IRQ_MAP=$(awk '
        NR==1 { ncpu = NF; next }
        $1 ~ /^[0-9]+:$/ {
            n = $1; sub(/:$/, "", n)
            out = ""
            for (i = ncpu + 5; i <= NF; i++) out = out (out ? " " : "") $i
            print n, out
        }
    ' /proc/interrupts 2>/dev/null)
}

irq_action() {   # $1 = irq number -> action text (may contain spaces)
    awk -v k="$1" '$1 == k { $1 = ""; sub(/^ /, ""); print; exit }' <<< "$IRQ_MAP"
}

irq_totals() {   # -> "<irq> <summed count>" per line, hardware IRQs only
    awk '
        NR==1 { ncpu = NF; next }
        $1 ~ /^[0-9]+:$/ {
            n = $1; sub(/:$/, "", n)
            s = 0
            for (i = 2; i <= ncpu + 1; i++) s += $i
            print n, s
        }
    ' /proc/interrupts 2>/dev/null
}

# --- PASS 1a: USB host controllers --------------------------------------
# Every root hub is one host controller. The controller's IRQ is the one
# whose action names that bus — bus-derived, so it holds for xhci, ehci,
# dwc2 and anything else, and for a device plugged in an hour from now.
pass_usb_controllers() {
    local hub bus n act
    for hub in /sys/bus/usb/devices/usb[0-9]*; do
        [ -e "$hub" ] || continue
        bus=$(basename "$hub")
        while read -r n act; do
            [ -n "$act" ] || continue
            case "$act" in
                *"$bus")        boost_irq "$n" "USB host controller for $bus" ;;
                *"$bus"[!0-9]*) boost_irq "$n" "USB host controller for $bus" ;;
            esac
        done <<< "$IRQ_MAP"
    done
}

# --- PASS 1b: sound cards that expose their own IRQ ----------------------
# PCI/PCIe cards and some platform cards publish `irq` somewhere up the
# device chain. Nothing on a Pi 5 does; on a machine with a PCIe interface
# this is the exact answer, so it costs nothing to look.
pass_card_sysfs() {
    local card dev d id
    for card in /sys/class/sound/card[0-9]*; do
        [ -e "$card/device" ] || continue
        id=$(cat "$card/id" 2>/dev/null || basename "$card")
        dev=$(readlink -f "$card/device" 2>/dev/null) || continue
        d="$dev"
        while [ -n "$d" ] && [ "$d" != "/sys/devices" ] && [ "$d" != "/" ]; do
            if [ -r "$d/irq" ]; then
                read -r n < "$d/irq" 2>/dev/null || n=""
                case "$n" in
                    ''|*[!0-9]*) ;;
                    *) boost_irq "$n" "sysfs irq of card $id" ;;
                esac
                break
            fi
            d=$(dirname "$d")
        done
    done
}

# --- PASS 2: whatever is actually pacing a running stream ----------------
# Returns the expected period-interrupt rate of the first RUNNING stream, or
# empty if nothing is playing. rate/period_size is the period-boundary rate;
# the real IRQ may fire faster (USB microframes) but never much slower.
running_expected_hz() {
    local st hp rate period
    for st in /proc/asound/card[0-9]*/pcm*/sub*/status; do
        [ -r "$st" ] || continue
        grep -q "state: RUNNING" "$st" 2>/dev/null || continue
        hp="${st%status}hw_params"
        rate=$(awk '/^rate:/ { print $2; exit }' "$hp" 2>/dev/null)
        period=$(awk '/^period_size:/ { print $2; exit }' "$hp" 2>/dev/null)
        case "${rate:-}${period:-}" in ''|*[!0-9]*) continue ;; esac
        [ "${period:-0}" -gt 0 ] || continue
        echo $(( rate / period ))
        return 0
    done
    return 1
}

pass_activity() {
    local waited=0 expected floor before after n a b rate

    while :; do
        if expected=$(running_expected_hz); then break; fi
        [ "$waited" -ge "$WATCH_SECS" ] && {
            log "no stream started within ${WATCH_SECS}s — structural pass only."
            log "re-run after starting audio: sudo systemctl restart demiurge-audio-irq-rt.service"
            return 0
        }
        sleep 1
        waited=$((waited+1))
    done

    # Floor: half the period rate, never below MIN_HZ. Below that it is
    # storage or network chatter, not an audio clock.
    floor=$(( expected / 2 ))
    [ "$floor" -lt "$MIN_HZ" ] && floor="$MIN_HZ"
    log "stream running: ~${expected} periods/s expected; counting IRQs over ${SAMPLE_SECS}s (floor ${floor}/s)"

    load_irq_map          # a device plugged in since boot has new IRQs
    before=$(irq_totals)
    sleep "$SAMPLE_SECS"
    after=$(irq_totals)

    while read -r n b; do
        a=$(awk -v k="$n" '$1 == k { print $2; exit }' <<< "$after")
        [ -n "$a" ] || continue
        rate=$(( (a - b) / SAMPLE_SECS ))
        [ "$rate" -ge "$floor" ] || continue
        boost_irq "$n" "$(irq_action "$n") advancing at ${rate}/s"
    done <<< "$before"
}

# ------------------------------------------------------------------------
if ! command -v chrt >/dev/null 2>&1; then
    log "chrt not available — nothing to do"
    exit 0
fi

grep -qw threadirqs /proc/cmdline 2>/dev/null ||
    log "warning: 'threadirqs' not on the kernel cmdline — most IRQs have no thread to boost"

load_irq_map
pass_usb_controllers
pass_card_sysfs
log "structural pass: $boosted_threads thread(s) at FIFO $PRIO"

pass_activity

log "done — $boosted_threads thread(s) at FIFO $PRIO across IRQ(s):${boosted_irqs% }"
exit 0
