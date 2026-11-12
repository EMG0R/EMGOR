#!/bin/bash
# deploy-launcher.sh — install a freshly-built demiurge-launcher and restart
# the system service. Run via sudo ON THE PI:
#
#   ssh -t <pi rig> 'sudo /home/<pi user>/deploy-launcher.sh'
#
# (Wrap in `ssh -t … 'sudo …'` so it runs on the Pi and sudo can prompt — a
# bare `! sudo …` from the Mac shell runs on the MAC, not the Pi.)
#
# Build first, as the normal user:
#   PATH=$HOME/.cargo/bin:$PATH cargo build --release \
#     --manifest-path $HOME/_______DEMIURGE/src/demiurge-launcher-rs/Cargo.toml
set -e
# Run via sudo, so $HOME is root's — derive the actual Pi user from whoever
# invoked sudo, falling back to the Pi OS convention of UID 1000.
PI_USER="${PI_USER:-${SUDO_USER:-$(getent passwd 1000 | cut -d: -f1)}}"
PI_USER="${PI_USER:-gorpi7}"
SRC="/home/$PI_USER/_______DEMIURGE/src/demiurge-launcher-rs/target/release/demiurge-launcher"
test -x "$SRC" || { echo "build missing: $SRC"; exit 1; }
install -m0755 "$SRC" /opt/demiurge/bin/demiurge-launcher
systemctl restart demiurge.service
echo DEPLOYED
