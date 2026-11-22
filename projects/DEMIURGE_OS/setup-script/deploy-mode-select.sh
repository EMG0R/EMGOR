#!/bin/bash
# deploy-mode-select.sh — REMOVE the old mode-select layer and install the
# bypass-aware launcher. sync_layer on/off is now handled INSIDE the launcher
# (it reads live.conf and, when sync_layer=off, runs the chain's program
# directly on the interface). No mode-select service / scripts / polkit needed.
#
# Build the launcher first (as the normal user, no sudo):
#   PATH=$HOME/.cargo/bin:$PATH cargo build --release \
#     --manifest-path $HOME/_______DEMIURGE/src/demiurge-launcher-rs/Cargo.toml
set -e
# Run via sudo, so $HOME is root's — derive the actual Pi user from whoever
# invoked sudo, falling back to the Pi OS convention of UID 1000.
PI_USER="${PI_USER:-${SUDO_USER:-$(getent passwd 1000 | cut -d: -f1)}}"
PI_USER="${PI_USER:-gorpi7}"
REPO="/home/$PI_USER/_______DEMIURGE"
LAUNCHER="$REPO/src/demiurge-launcher-rs/target/release/demiurge-launcher"
test -x "$LAUNCHER" || { echo "build the launcher first: $LAUNCHER missing"; exit 1; }

# 1. remove the old mode-select layer
rm -f /etc/systemd/system/demiurge.service.d/mode-select.conf
rm -f /etc/systemd/system/demiurge.service.d/wait-pipewire.conf
rm -f /usr/local/bin/demiurge-mode-select.sh /usr/local/bin/demiurge-lowlatency-check.sh
rm -f /etc/polkit-1/rules.d/49-demiurge.rules

# 2. install the bypass-aware launcher + the DIRECT-capable csound wrapper
install -m0755 "$LAUNCHER"                              /opt/demiurge/bin/demiurge-launcher
install -m0755 "$REPO/src/wrappers/demiurge-run-csound" /opt/demiurge/bin/demiurge-run-csound
# The live-audio interlock ships beside the wrapper, because everything that
# needs it (the updater, builds, profiling) looks for it at this path.
install -m0755 "$REPO/src/wrappers/demiurge-live-guard"  /opt/demiurge/bin/demiurge-live-guard

# 3. restart onto the clean unit (ExecStart back to the launcher)
systemctl daemon-reload
systemctl reset-failed demiurge 2>/dev/null || true
systemctl restart demiurge
echo BYPASS-LAUNCHER-DEPLOYED
