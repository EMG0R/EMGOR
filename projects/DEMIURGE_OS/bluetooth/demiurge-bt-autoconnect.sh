#!/usr/bin/env bash
# demiurge-bt-autoconnect.sh — connect the saved Bluetooth speaker ON BOOT.
#
# Runs ONCE at boot (systemd --user oneshot), connects the speaker saved in
# ~/demiurge/bluetooth.conf, then EXITS. No long-running process, no ongoing
# CPU use — it just does the boot-time connect and stops.
#
# It retries for a couple of minutes to cover the speaker powering on a little
# after the Pi, then gives up until the next boot. Trust (set by the setup
# script) is what lets the link succeed; this is what initiates it from the Pi.
#
# Reliability: an active BLE discovery scan aborts a classic A2DP connect
# (le-connection-abort-by-local), so we stop scanning before every attempt and
# wait for the adapter to be powered first.

set -u

CONF="${DEMIURGE_BT_CONF:-$HOME/demiurge/bluetooth.conf}"
ATTEMPTS="${DEMIURGE_BT_ATTEMPTS:-24}"   # ~2 min total at 5s spacing
GAP="${DEMIURGE_BT_GAP:-5}"

log() { echo "demiurge-bt-autoconnect: $*"; }

[ -f "$CONF" ] || { log "no saved device ($CONF) — nothing to do"; exit 0; }
# shellcheck disable=SC1090
. "$CONF" 2>/dev/null
[ -n "${MAC:-}" ] || { log "no MAC in $CONF — nothing to do"; exit 0; }
command -v bluetoothctl >/dev/null 2>&1 || { log "bluetoothctl missing"; exit 0; }

# Wait for the adapter to be powered before the first attempt.
for _ in $(seq 1 15); do
    bluetoothctl show 2>/dev/null | grep -q "Powered: yes" && break
    bluetoothctl power on >/dev/null 2>&1
    sleep 2
done

connected() { bluetoothctl info "$MAC" 2>/dev/null | grep -q "Connected: yes"; }

if connected; then log "already connected: ${NAME:-$MAC}"; exit 0; fi

for i in $(seq 1 "$ATTEMPTS"); do
    connected && { log "connected ${NAME:-$MAC}"; exit 0; }
    log "attempt $i/$ATTEMPTS — connecting ${NAME:-$MAC} ($MAC)"
    bluetoothctl scan off >/dev/null 2>&1
    bluetoothctl connect "$MAC" >/dev/null 2>&1
    # PATIENT: wait for this attempt to finish before firing another. Hammering
    # connect collides ("Operation already in progress") and makes the speaker
    # refuse/drop. Poll up to ~12s for success before the next attempt.
    for _ in $(seq 1 12); do connected && { log "connected ${NAME:-$MAC}"; exit 0; }; sleep 1; done
    sleep "$GAP"
done

connected && log "connected ${NAME:-$MAC}" || log "gave up after $ATTEMPTS attempts; will retry next boot."
exit 0
