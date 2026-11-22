#!/bin/bash
# flash-ota.sh — compile + push dot2 over the air (ArduinoOTA).
# Usage: ./flash-ota.sh [host]     (default: dot2.local; pass the IP if mDNS is flaky)
# Same pattern the project uses for the strip nodes: espota.py, port 3232,
# password CHANGE_ME_PASSWORD.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
FQBN="esp32:esp32:esp32doit-devkit-v1"
BUILD="/tmp/dot2-build"
HOST="${1:-dot2.local}"
ESPOTA="$HOME/Library/Arduino15/packages/esp32/hardware/esp32/3.3.8/tools/espota.py"

echo "== dot2: compiling for $FQBN =="
arduino-cli compile --fqbn "$FQBN" --build-path "$BUILD" "$DIR"

echo "== OTA push to $HOST (port 3232) =="
if [ -f "$ESPOTA" ]; then
  python3 "$ESPOTA" -i "$HOST" -p 3232 -a CHANGE_ME_PASSWORD -f "$BUILD/dot2.ino.bin"
else
  # fallback: let arduino-cli invoke espota itself
  arduino-cli upload --fqbn "$FQBN" -p "$HOST" --upload-field password=CHANGE_ME_PASSWORD --input-dir "$BUILD" "$DIR"
fi

echo "== done. verify: curl -s http://$HOST/api/state =="
