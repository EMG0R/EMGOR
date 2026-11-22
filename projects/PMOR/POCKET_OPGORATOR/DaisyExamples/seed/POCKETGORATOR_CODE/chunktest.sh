#!/bin/bash
# chunktest.sh <chunk>  — build a staged firmware, self-DFU over CDC, flash it,
# and report whether USB still enumerates.
#
# Stages (init order):
#   0 usb only   1 +OLED   2 +sensors(I2C)   3 +SD   4 +encoders/ADC
#   5 +patches   6 +audio  7 +NeoTrellis     full = omit arg
#
# Chunk 0 is PROVEN to enumerate from the Daisy bootloader, so BOOT_SRAM is
# not the problem. This walks the instrument up until USB dies; the first
# failing stage is the culprit.
#
# Self-DFU only works while the CURRENT firmware still enumerates (CDC is a
# USB interface). Once a chunk breaks USB, the next flash needs a physical
# BOOT+RESET — which is why we go upward from a known-good stage.
set -u
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

CHUNK="${1:-}"
if [ -n "$CHUNK" ]; then BUILD_ARGS="CHUNK=$CHUNK"; LABEL="chunk $CHUNK"
else BUILD_ARGS=""; LABEL="FULL instrument"; fi

echo "=== building $LABEL ==="
make clean >/dev/null 2>&1
if ! make $BUILD_ARGS 2>&1 | grep -qE "\.bin$|objcopy -O binary"; then
  make $BUILD_ARGS 2>&1 | grep -iE "(^|[^-])error:" | head -5
fi
[ -f build/POCKETGORATOR_CODE.bin ] || { echo "BUILD FAILED"; exit 1; }
echo "built $(stat -f%z build/POCKETGORATOR_CODE.bin) bytes"

# --- get into DFU: prefer the CDC trigger, fall back to waiting ---
if ! dfu-util -l 2>/dev/null | grep -q "Found DFU"; then
  PORT="$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)"
  if [ -n "$PORT" ]; then
    echo "triggering DFU over $PORT"
    python3 - "$PORT" <<'PY'
import sys, os, termios
fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd); a[0]=a[1]=a[3]=0
a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
termios.tcsetattr(fd, termios.TCSANOW, a)
os.write(fd, b'dfu\r\n'); os.close(fd)
PY
  else
    echo "!! no CDC port and not in DFU — waiting for a physical BOOT+RESET"
    echo "   (press it whenever; this waits up to 10 minutes)"
  fi
  for i in $(seq 1 1200); do
    dfu-util -l 2>/dev/null | grep -q "Found DFU" && break
    python3 -c "import time;time.sleep(0.5)"
  done
fi
dfu-util -l 2>/dev/null | grep -q "Found DFU" || { echo "NO DFU — aborting"; exit 2; }

# --- flash bootloader + app ---
make program-boot 2>&1 | grep -E "File downloaded successfully" >/dev/null
python3 -c "import time;time.sleep(1.2)"
for j in $(seq 1 8); do
  if dfu-util -l 2>/dev/null | grep -q "Found DFU"; then
    dfu-util -a 0 -s 0x90040000:leave -D build/POCKETGORATOR_CODE.bin 2>&1 \
      | grep -E "File downloaded successfully" >/dev/null && break
  fi
  python3 -c "import time;time.sleep(0.4)"
done

python3 -c "import time;time.sleep(12)"

# --- verdict ---
USB=$(ioreg -p IOUSB -l -w 0 2>/dev/null | grep -c '"USB Product Name"')
AUD=$(ffmpeg -hide_banner -f avfoundation -list_devices true -i "" 2>&1 | grep -ciE "opgorator|pmor")
SER=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
echo "--------------------------------------------"
if [ "$AUD" -ge 1 ] && [ -n "$SER" ]; then
  echo "RESULT: $LABEL -> USB OK  (audio=$AUD serial=$SER)"
  exit 0
else
  echo "RESULT: $LABEL -> USB DEAD  (usbnodes=$USB audio=$AUD serial='${SER:-none}')"
  echo ">>> this stage is the culprit (or earlier-and-cumulative)"
  exit 1
fi
