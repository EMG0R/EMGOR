#!/bin/bash
# Removes the OmniplexAudio.driver bundle from /Library/Audio/Plug-Ins/HAL and
# restarts coreaudiod. Requires sudo.
set -euo pipefail

DEST="/Library/Audio/Plug-Ins/HAL/OmniplexAudio.driver"

if [ -d "$DEST" ]; then
    echo "Removing $DEST (requires sudo)"
    sudo rm -rf "$DEST"
else
    echo "$DEST not present, nothing to remove."
fi

echo "Restarting coreaudiod (requires sudo)"
sudo killall coreaudiod || true

sleep 2
echo "Done."
