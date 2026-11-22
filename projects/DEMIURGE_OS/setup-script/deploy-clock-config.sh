#!/bin/bash
# deploy-clock-config.sh — install the clock config so QUANTUM is a single knob
# (set only in ~/demiurge/live.conf `quantum =`). Installs: pipewire low-latency
# conf (max-quantum ceiling 2048 so any live.conf quantum fits), the bypass-aware
# wait-pipewire drop-in (256 boot seed), and demiurge-env.sh (PIPEWIRE_LATENCY
# derives from DEMIURGE_QUANTUM/DEMIURGE_RATE). Non-disruptive: nothing restarts;
# max-quantum applies on next pipewire start, env.sh on next wrapper launch.
#
# This script is CONFIG ONLY. To rebuild + install the clock DAEMON or the
# LAUNCHER on the Pi, use the commands in demiurge/docs/clock.md
# ("Rebuilding the daemon and launcher on the Pi") -- the same flags as
# setup-demiurge.sh Phase 6.
set -e
# Run via sudo, so $HOME is root's — derive the actual Pi user from whoever
# invoked sudo, falling back to the Pi OS convention of UID 1000.
PI_USER="${PI_USER:-${SUDO_USER:-$(getent passwd 1000 | cut -d: -f1)}}"
PI_USER="${PI_USER:-gorpi7}"
REPO="/home/$PI_USER/_______DEMIURGE"
install -m0644 "$REPO/config/pi5-performance/demiurge-pipewire-lowlatency.conf" /etc/pipewire/pipewire.conf.d/99-demiurge-lowlatency.conf
install -m0644 "$REPO/config/systemd/demiurge.service.d/wait-pipewire.conf"     /etc/systemd/system/demiurge.service.d/wait-pipewire.conf
install -m0755 "$REPO/src/wrappers/demiurge-env.sh"                              /opt/demiurge/lib/demiurge-env.sh
systemctl daemon-reload
echo CLOCK-CONFIG-INSTALLED
