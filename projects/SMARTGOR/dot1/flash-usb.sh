#!/bin/bash
# flash-usb.sh — compile + upload dot1 over USB serial.
# Usage: ./flash-usb.sh [port]     (port autodetected if omitted)
#
# Upload runs at 921600 baud (set below via UploadSpeed). If the upload fails
# or the board drops mid-transfer, retry at 115200:
#   change UploadSpeed=921600 to UploadSpeed=115200 on the compile/upload lines.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
FQBN="esp32:esp32:esp32doit-devkit-v1"   # same board target as the strip nodes (NodeMCU/DOIT WROOM-32)
BUILD="/tmp/dot1-build"

# ---- pick the USB serial port: $1 wins, else newest matching /dev/cu.* ----
PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT=$(ls -t /dev/cu.usbserial* /dev/cu.SLAB* /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
fi
if [ -z "$PORT" ]; then
  echo "ERROR: no USB serial port found (looked for /dev/cu.usbserial*, cu.SLAB*, cu.wchusbserial*, cu.usbmodem*)." >&2
  echo "Plug the board in, or pass the port explicitly: ./flash-usb.sh /dev/cu.usbserial-0001" >&2
  exit 1
fi

echo "!! REMINDER: the 7-px dot's LED wire must be DISCONNECTED before USB flashing !!"
echo "   (a connected strip/dot makes SPI flash reads look like a dead board — OTA is unaffected)"

echo "== dot1: compiling for $FQBN =="
arduino-cli compile --fqbn "$FQBN" --board-options UploadSpeed=921600 --build-path "$BUILD" "$DIR"

echo "== uploading via $PORT at 921600 baud =="
arduino-cli upload --fqbn "$FQBN" --board-options UploadSpeed=921600 -p "$PORT" --input-dir "$BUILD" "$DIR"

cat <<EOF

== DONE. Next steps ==
1) Watch the boot log for the IP address (ctrl-C to exit):
     arduino-cli monitor -p $PORT -c baudrate=115200
   Look for:  wifi: connected, IP 192.168.8.xxx
2) Sanity check the API:
     curl -s http://<ip>/api/state
3) Future updates go over the air: ./flash-ota.sh <ip>   (or dot1.local)
EOF
