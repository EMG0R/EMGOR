#!/bin/bash
#
# DEMIURGE — On-demand re-enumeration of the attached audio interface.
#
# Installed to: /usr/local/bin/demiurge-audio-bounce.sh
# Called by:    the Rust launcher's ensure_usb_rates_stable() (util.rs), and
#               by hand when an interface comes up at the wrong rate.
#               NOT a boot service — see "History" below.
#
# WHAT
# ----
# Performs a programmatic unplug+replug of every hot-pluggable audio
# interface currently attached: sysfs unbind, pause, bind. ALSA closes the
# device, WirePlumber sees device-remove + device-add, and every node is
# recreated from scratch with monitor.alsa.rules re-applied — which is what
# clears a node that negotiated the wrong sample rate and would otherwise
# resample silently (the "reports 48000, sounds bit-crushed" failure).
#
# WHICH DEVICE — DERIVED, NEVER HARDCODED
# ---------------------------------------
# The set of devices to bounce is derived from the sound cards actually
# present: for each `/sys/class/sound/cardN`, walk its device chain upward
# looking for a USB device node (`<bus>-<port>[.<port>]`, e.g. `1-2`). If one
# is found, that card lives on a hot-pluggable bus and can be bounced. No
# vendor or product ID appears anywhere in this script: any class-compliant
# USB interface, any brand, gets the same treatment.
#
# Cards with no such ancestor — an I2S HAT, HDMI, an on-SoC codec, the
# always-on loopback — are permanently powered and cannot be re-enumerated.
# For those this script does nothing and exits 0. That is the correct
# behaviour, not a degraded one: those cards do not have the firmware
# clock-state problem this works around, because they never lose power.
#
# Root hubs (`usbN`) are deliberately never bounced: unbinding a host
# controller takes down every device on the bus, including the MIDI
# controller and the boot medium on some layouts.
#
# Always exits 0. Nothing attached, nothing bounceable, no permission — all
# are silent no-ops.
#
# History: this replaces demiurge-scarlett-bounce.sh, which matched USB
# vendor ID 1235 and was therefore a no-op for every other interface, and
# the demiurge-scarlett-rebounce.{service,timer} pair that ran it at boot.
# See config/pi5-performance/README.md for why the boot units were retired.
set -u

TAG="[demiurge-audio-bounce]"
SETTLE_DOWN="${DEMIURGE_BOUNCE_DOWN:-1}"    # seconds unbound
SETTLE_UP="${DEMIURGE_BOUNCE_UP:-2}"        # seconds for WirePlumber to re-enumerate

log() { echo "$TAG $*"; }

# Walk a sysfs device path upward; print the first ancestor that is a
# top-level USB device (bus-port[.port…]). Interfaces carry a colon
# (`1-2:1.0`) and root hubs are named `usbN` — neither is what we want.
usb_parent_of() {
    local d="$1" base
    while [ -n "$d" ] && [ "$d" != "/sys/devices" ] && [ "$d" != "/" ]; do
        base=$(basename "$d")
        case "$base" in
            *:*)            ;;                       # interface, keep walking
            usb[0-9]*)      ;;                       # root hub, keep walking
            [0-9]*-[0-9]*)
                if [ -e "/sys/bus/usb/devices/$base" ]; then
                    echo "$base"
                    return 0
                fi
                ;;
        esac
        d=$(dirname "$d")
    done
    return 1
}

devs=""
for card in /sys/class/sound/card[0-9]*; do
    [ -e "$card/device" ] || continue
    id=$(cat "$card/id" 2>/dev/null || basename "$card")
    path=$(readlink -f "$card/device" 2>/dev/null) || continue
    if dev=$(usb_parent_of "$path"); then
        case " $devs " in *" $dev "*) continue ;; esac
        devs="$devs $dev"
        log "card $id is on hot-pluggable device $dev — will bounce"
    else
        log "card $id is not hot-pluggable (permanently powered) — skipping"
    fi
done

if [ -z "${devs// /}" ]; then
    log "nothing bounceable attached — nothing to do"
    exit 0
fi

for dev in $devs; do
    echo "$dev" > /sys/bus/usb/drivers/usb/unbind 2>/dev/null ||
        log "warning: could not unbind $dev (need root?)"
done

sleep "$SETTLE_DOWN"

for dev in $devs; do
    echo "$dev" > /sys/bus/usb/drivers/usb/bind 2>/dev/null ||
        log "warning: could not bind $dev back"
done

# Let ALSA re-enumerate and WirePlumber re-apply monitor.alsa.rules to the
# fresh nodes before anything tries to open them.
sleep "$SETTLE_UP"

log "done"
exit 0
