#!/bin/bash
# Installs omniplex onto this Raspberry Pi. Always removes any prior install
# first, so re-running this after a rebuild never leaves a stale binary
# or a stale/duplicate systemd unit behind.
#
# Usage: ./install.sh [--service]
#   --service   also install and enable a systemd unit (omniplex.service) that
#               runs `omniplex run --midi --audio --video-test` at boot.
#               Without this flag, only the /usr/local/bin/omniplex binary is
#               installed and you run it manually.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> Removing any previous install"
sudo systemctl stop omniplex.service 2>/dev/null || true
sudo systemctl disable omniplex.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/omniplex.service
sudo rm -f /usr/local/bin/omniplex

echo "==> Installing /usr/local/bin/omniplex"
sudo cp "$SCRIPT_DIR/bin/omniplex" /usr/local/bin/omniplex
sudo chmod 755 /usr/local/bin/omniplex

# Audio access needs the invoking user in the 'audio' group; MIDI/serial
# device nodes are typically 'dialout'/'plugdev' -- add both defensively.
for grp in audio dialout plugdev; do
    if getent group "$grp" >/dev/null 2>&1; then
        sudo usermod -aG "$grp" "$USER" || true
    fi
done

if [[ "${1:-}" == "--service" ]]; then
    echo "==> Installing systemd service (running as user: $USER)"
    sed "s/__INSTALL_USER__/$USER/" "$SCRIPT_DIR/omniplex.service" \
        | sudo tee /etc/systemd/system/omniplex.service >/dev/null
    sudo systemctl daemon-reload
    sudo systemctl enable --now omniplex.service
    echo "omniplex.service enabled and started. Check with: systemctl status omniplex"
fi

echo ""
echo "==> Done. Run 'omniplex info' to confirm."
echo ""
echo "If you just got added to a new group (audio/dialout/plugdev), log out"
echo "and back in (or reboot) for it to take effect."
echo ""
echo "Discovery interface: this Pi's Ethernet port (eth0) and, if you set"
echo "up USB gadget networking, usb0 are both auto-detected. Override with"
echo "OMNIPLEX_IFACE=<name> if it doesn't find the right one automatically."
