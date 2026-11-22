#!/usr/bin/env bash
# Build + flash a bring-up test to the Daisy Seed 3.   usage:  ./flash.sh [TEST]     (default 01_faders)
# Default method: BOOT_NONE, app in internal flash 0x08000000, written through the ST ROM DFU (no Daisy bootloader needed).
# bootloader (BootloaderMode::DAISY_INFINITE_TIMEOUT). The ST ROM DFU cannot write QSPI.
# Never writes option bytes, never passes any :mass-erase, never touches 0x08000000 unless you set
# INTERNAL=1 (BOOT_NONE build into internal flash; overwrites a Daisy bootloader if one is installed).
#
# First flash ever on a blank Seed: hold BOOT, tap RESET, release BOOT (Seed enters DFU), then run this.
# After that the app has buttonless DFU: this script (or the web UI "Enter DFU" button) sends "dfu".
set -u
cd "$(dirname "$0")"
TEST=${1:-01_faders}
INTERNAL=${INTERNAL:-1}   # default: internal flash (BOOT_NONE). INTERNAL=0 -> QSPI via Daisy bootloader
if [ "$INTERNAL" = 1 ]; then APPT=BOOT_NONE; ADDR=0x08000000; else APPT=BOOT_SRAM; ADDR=0x90040000; fi
make -j8 TEST="$TEST" APP_TYPE="$APPT" >/dev/null || { echo ">>> build failed (run make to see why)"; exit 1; }
BIN=build/bringup_${TEST}.bin
echo ">>> built $BIN ($(wc -c < "$BIN") bytes) target $ADDR"

# Buttonless trigger. Only talk to a port that is OUR firmware (libDaisy CDC, serial ...050B...)
# or one you name with PORT=. The Bela also shows up as /dev/cu.usbmodem*: never auto-pick that.
PORT=${PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | grep -v 4488BED64B3B | head -1)}   # never the Bela (4488BED64B3B)
if ! dfu-util -l 2>&1 | grep -qi 'found dfu'; then
  if [ -n "$PORT" ]; then
    echo ">>> sending dfu to $PORT"
    PY=../../.venv/bin/python
    "$PY" - "$PORT" <<'PY'
import serial, sys, time
s = serial.Serial(sys.argv[1], 115200, timeout=1); s.dtr = True; s.rts = True
time.sleep(0.3); s.write(b'dfu\n'); s.flush(); time.sleep(0.3); s.close()
PY
  else
    echo ">>> no DFU device and no bring-up CDC port. Put the Seed in DFU: hold BOOT, tap RESET, release BOOT."
  fi
fi
# INTERNAL=0 (BOOT_SRAM): the Daisy bootloader exposes the QSPI target only inside its window (buttonless "dfu" uses
# DAISY_INFINITE_TIMEOUT so it stays open; a power-up entry has about 2 s). Catch the device, then retry the download for a few
# seconds until the 0x90040000 alt accepts it. venv python only (../../.venv), never system python3.
for i in $(seq 1 80); do                       # 20 s catcher
  if dfu-util -l 2>&1 | grep -qi 'found dfu'; then
    dfu-util -l 2>&1 | grep -i 'alt=' | head -3
    for t in 1 2 3 4 5 6 7 8; do
      OUT=$(dfu-util -a 0 -s ${ADDR}:leave -D "$BIN" -d ,0483:df11 2>&1)
      if echo "$OUT" | grep -q "File downloaded successfully"; then echo ">>> FLASHED"; exit 0; fi
      echo "$OUT" | grep -iE "not writeable|error|cannot|No DFU" | head -1
      sleep 0.25
    done
    echo ">>> flash refused. INTERNAL=0 needs the Daisy bootloader on the Seed; the ST ROM loader cannot write QSPI."; exit 2
  fi
  sleep 0.25
done
echo ">>> NOT FLASHED: no DFU device appeared"; exit 1
