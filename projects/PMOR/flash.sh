#!/usr/bin/env bash
# Build + flash PMOR to the Daisy Seed 3. Usage:  ./flash.sh
#
# WHY THIS SCRIPT EXISTS
# ----------------------
# The obvious one-liner DOES NOT WORK:
#
#     printf 'dfu\n' > /dev/cu.usbmodem*        # <-- silently fails
#
# A shell redirect opens the port WITHOUT asserting DTR. macOS never raises
# the line, the CDC stack never delivers the bytes, PMOR's parser never sees
# "dfu", and no DFU device ever appears. You get an endless "waiting for DFU"
# with no error anywhere. Open the port properly and assert DTR instead.
#
# PMOR's own trigger is fine: it uses BootloaderMode::DAISY_INFINITE_TIMEOUT,
# which exposes QSPI. (BootloaderMode::STM would boot the ST ROM loader, whose
# DFU has ONLY internal flash 0x08000000 and rejects a BOOT_SRAM app with
# "Last page ... is not writeable".)
set -u
APP=PMOR_V3
ADDR=0x90040000        # QSPI — PMOR is APP_TYPE = BOOT_SRAM

cd "$(dirname "$0")"
echo ">>> $(git log --oneline -1 2>/dev/null)"
make -j8 || exit 1

PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
if [ -n "$PORT" ]; then
  echo ">>> triggering DFU on $PORT (DTR asserted)"
  python3 - "$PORT" <<'PY'
import serial, sys, time
s = serial.Serial(sys.argv[1], 115200, timeout=1)
s.dtr = True; s.rts = True          # THE important bit
time.sleep(0.3)
s.write(b'dfu\r\n'); s.flush()
time.sleep(0.3); s.close()
PY
else
  echo ">>> no CDC port — if the board is unresponsive, hold BOOT + tap RESET"
fi

# Catcher: the bootloader window is short, so poll fast and write on sight.
for i in $(seq 1 80); do
  if dfu-util -l 2>&1 | grep -qi 'found dfu'; then
    OUT=$(dfu-util -a 0 -s ${ADDR}:leave -D build/${APP}.bin -d ,0483:df11 2>&1)
    # Success is this exact string. dfu-util then reports a benign
    # "Error during download get_status" as the board resets — ignore it.
    if echo "$OUT" | grep -q "File downloaded successfully"; then
      echo ">>> FLASHED"; exit 0
    fi
    echo "$OUT" | grep -iE "not writeable|error" | head -1
  fi
  sleep 0.25
done
echo ">>> NOT FLASHED — board not on USB?"; exit 1
