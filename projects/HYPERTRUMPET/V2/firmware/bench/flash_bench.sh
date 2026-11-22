#!/usr/bin/env bash
# Build + flash the benchmark to the Daisy Seed 3 (BOOT_SRAM: app in QSPI 0x90040000 via the Daisy bootloader). NOT run by the builder agent.
#   ./flash_bench.sh                 app only: needs the Daisy bootloader already installed (PMOR flow)
#   INSTALL_BOOTLOADER=1 ./flash_bench.sh   first install the Daisy bootloader at 0x08000000 (OVERWRITES whatever BOOT_NONE app is in internal flash,
#                                    e.g. the bring-up firmware; it can be re-flashed later with bringup/fw/flash.sh). Seed must be in ROM DFU
#                                    (bring-up fw: this script sends "dfu"; blank/other: hold BOOT, tap RESET, release BOOT).
set -u
cd "$(dirname "$0")"
make -j8 >/dev/null || { echo ">>> build failed"; exit 1; }
BIN=build/hx_bench.bin; BOOTBIN=../../../HYPER_CORE/third_party/libDaisy/core/dsy_bootloader_v6_4-intdfu-2000ms.bin
echo ">>> built $BIN ($(wc -c < "$BIN") bytes)"
PORT=${PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | grep -v 4488BED64B3B | head -1)}   # never the Bela
if ! dfu-util -l 2>&1 | grep -qi 'found dfu' && [ -n "$PORT" ]; then
  echo ">>> sending dfu to $PORT"
  ../../.venv/bin/python - "$PORT" <<'PY'
import serial, sys, time
s = serial.Serial(sys.argv[1], 115200, timeout=1); s.dtr = True; s.rts = True
time.sleep(0.3); s.write(b'dfu\r\n'); s.flush(); time.sleep(0.3); s.close()
PY
fi
wait_dfu() { for i in $(seq 1 80); do dfu-util -l 2>&1 | grep -qi 'found dfu' && return 0; sleep 0.25; done; return 1; }
wait_dfu || { echo ">>> no DFU device (hold BOOT, tap RESET, release BOOT)"; exit 1; }
if [ "${INSTALL_BOOTLOADER:-0}" = 1 ]; then
  dfu-util -a 0 -s 0x08000000:leave -D "$BOOTBIN" -d ,0483:df11 || exit 2
  sleep 1; wait_dfu || { echo ">>> bootloader did not enumerate"; exit 1; }
fi
dfu-util -a 0 -s 0x90040000:leave -D "$BIN" -d ,0483:df11 && echo ">>> FLASHED"
