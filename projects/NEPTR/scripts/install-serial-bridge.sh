#!/bin/bash
# install-serial-bridge.sh  (NEPTR project — NOT part of DEMIURGE)
# Installs + enables NEPTR's Teensy serial->MIDI bridge. The bridge reads the
# Teensy's 12-bit serial frames, scales on-Pi, emits CCs on its ALSA-seq port,
# routes itself into the Midi Through pool (14:0) that the program reads, and
# removes the raw USB-MIDI feed. DEMIURGE knows nothing about this — it just
# reads the pool. Run via sudo on the Pi.
set -e
# Run via sudo, so $HOME is root's — derive the actual Pi user from whoever
# invoked sudo, falling back to the Pi OS convention of UID 1000.
PI_USER="${PI_USER:-${SUDO_USER:-$(getent passwd 1000 | cut -d: -f1)}}"
PI_USER="${PI_USER:-gorpi7}"
# Template User=/Group=/HOME=/ExecStart= from the actual Pi user — the repo
# ships gorpi7 as a documented placeholder, same convention as
# config/demiurge.service (rendered by setup-demiurge.sh).
sed -e "s/^User=.*/User=$PI_USER/" \
    -e "s/^Group=.*/Group=$PI_USER/" \
    -e "s#^Environment=HOME=.*#Environment=HOME=/home/$PI_USER#" \
    -e "s#^ExecStart=.*#ExecStart=/usr/bin/python3 /home/$PI_USER/NEPTR_phase4/csound/serial_bridge.py /dev/ttyACM0#" \
    "/home/$PI_USER/NEPTR_phase4/csound/neptr-bridge.service" > /etc/systemd/system/neptr-bridge.service
systemctl daemon-reload
systemctl enable --now neptr-bridge.service
echo NEPTR-SERIAL-BRIDGE-INSTALLED
