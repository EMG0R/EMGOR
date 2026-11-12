#!/bin/bash
# Installs the built OmniplexAudio.driver bundle into /Library/Audio/Plug-Ins/HAL
# and restarts coreaudiod so it picks up the new plug-in. Requires sudo for
# both the copy (system-owned directory) and the coreaudiod restart.
set -euo pipefail

cd "$(dirname "$0")"

BUNDLE="build/OmniplexAudio.driver"
DEST="/Library/Audio/Plug-Ins/HAL/OmniplexAudio.driver"

if [ ! -d "$BUNDLE" ]; then
    echo "Bundle not found at $BUNDLE -- run 'make' first." >&2
    exit 1
fi

echo "Installing $BUNDLE -> $DEST (requires sudo)"
sudo rm -rf "$DEST"
sudo cp -R "$BUNDLE" "$DEST"
sudo chown -R root:wheel "$DEST"
sudo chmod -R go-w "$DEST"

echo "Restarting coreaudiod to load the plug-in (requires sudo)"
sudo killall coreaudiod || true

sleep 2
echo "Done. Verify with:"
echo "  system_profiler SPAudioDataType | grep -A6 'Omniplex Audio'"
