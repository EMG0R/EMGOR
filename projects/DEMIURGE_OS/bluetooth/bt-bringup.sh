#!/usr/bin/env bash
# bt-bringup.sh — one-shot reliable Bluetooth bring-up for DEMIURGE.
#
# Run this ONCE (it asks for sudo to restart bluetooth). It performs the full
# documented fix sequence for "A2DP connects then drops / no audio node":
#   1. restart bluetooth.service   (re-syncs BlueZ with PipeWire — bluez#1545)
#   2. restart wireplumber         (re-registers the A2DP media endpoints)
#   3. patient connect             (one unhurried attempt, no hammering)
#   4. report connection + audio node
#
#   usage:  ssh -t gorpi7@demiurge.local '~/demiurge/bt-bringup.sh'
set -u
MAC="${1:-$(. ~/demiurge/bluetooth.conf 2>/dev/null; echo "${MAC:-}")}"
[ -n "$MAC" ] || { echo "No saved device. Pair first with: ~/demiurge/demiurge-bt"; exit 1; }

if ! bluetoothctl info "$MAC" >/dev/null 2>&1; then
    echo "Device $MAC not paired yet. Pair first with: ~/demiurge/demiurge-bt"; exit 1
fi

echo "[1/4] restarting bluetooth.service (sudo)…"
sudo systemctl restart bluetooth || { echo "sudo failed — skipping bluetooth restart"; }
sleep 3

echo "[2/4] restarting wireplumber…"
systemctl --user restart wireplumber
sleep 5

echo "[3/4] patient connect to $MAC…"
bluetoothctl power on  >/dev/null 2>&1
bluetoothctl scan off  >/dev/null 2>&1
bluetoothctl trust "$MAC" >/dev/null 2>&1
bluetoothctl disconnect "$MAC" >/dev/null 2>&1; sleep 2
connected() { bluetoothctl info "$MAC" 2>/dev/null | grep -q "Connected: yes"; }
for a in 1 2 3; do
    connected && break
    bluetoothctl connect "$MAC" >/dev/null 2>&1
    for _ in $(seq 1 15); do connected && break; sleep 1; done
done

echo "[4/4] result:"
bluetoothctl info "$MAC" 2>/dev/null | grep -E "Connected|Paired|Trusted" | sed 's/^/    /'
if timeout 6 pw-link -i 2>/dev/null | grep -q bluez_output; then
    echo "    AUDIO NODE: YES  ✅  (bluez_output present — sound can route)"
else
    echo "    AUDIO NODE: NO   ❌  (connected but no transport)"
fi
