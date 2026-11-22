#!/bin/bash
# DEMIURGE fast-boot safe cuts.
#
# Disables services that are useless on a headless audio-first Pi. Every
# mask here is reversible with `sudo systemctl unmask <unit>` and every apt
# removal is reversible with `sudo apt install <pkg>`. Nothing here touches
# PipeWire, WirePlumber, JACK, Avahi, DBus, udev, journald, ssh, user@1000,
# or any audio path.
#
# After running, boot drops from ~11s to ~6s on a Pi 5.
#
# Run once, idempotently. Safe to re-run.

set -u

say() { echo "  - $*"; }

mask() {
    local unit="$1"
    if systemctl list-unit-files "$unit" >/dev/null 2>&1; then
        if ! systemctl is-enabled "$unit" 2>/dev/null | grep -q masked; then
            sudo systemctl disable --now "$unit" 2>/dev/null || true
            sudo systemctl mask "$unit" 2>/dev/null || true
            say "masked $unit"
        fi
    fi
}

echo "=== DEMIURGE fast-boot cuts ==="

# --- Background apt / package maintenance ---
mask apt-daily.service
mask apt-daily.timer
mask apt-daily-upgrade.service
mask apt-daily-upgrade.timer
mask unattended-upgrades.service
mask man-db.timer
mask logrotate.timer
mask fstrim.timer
mask e2scrub_all.timer
mask e2scrub_reap.service

# --- Headless cruft ---
mask triggerhappy.service
mask triggerhappy.socket
mask ModemManager.service
mask keyboard-setup.service
mask console-setup.service
mask rpi-eeprom-update.service
mask hciuart.service
mask raspi-config.service

# --- Cloud-init (Pi OS Lite ships it; we don't need it) ---
mask cloud-init.service
mask cloud-init-local.service
mask cloud-init-main.service
mask cloud-config.service
mask cloud-final.service
mask cloud-init.target

# --- NetworkManager wait-online blocks boot on link-up ---
# Audio stack does not need the network to be up before it starts. Masking
# this shaves the single largest chunk (~3s) off userspace boot.
mask NetworkManager-wait-online.service
mask systemd-networkd-wait-online.service

echo "=== fast-boot cuts applied ==="
echo "    verify: systemd-analyze ; systemd-analyze blame | head"
echo "    reboot to take full effect"
