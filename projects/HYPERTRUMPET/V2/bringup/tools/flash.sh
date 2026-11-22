#!/bin/zsh
# flash.sh <bin> [timeout_s]: ask the web tab to release the port, put the trumpet Seed in DFU, flash to QSPI 0x90040000, read hello, give the port back.
# Refuses if the spare bench Seed (cu.usbmodem18063371376) is attached.
B=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); cd ~/Documents/_EMGOR_SYNTH/______2026NEW/HYPERTRUMPET_V2; PY=.venv/bin/python; P=/dev/cu.usbmodem327F347733331; T=bringup/tools
end=$((SECONDS+${2:-120})); sent=0
while [ $SECONDS -lt $end ]; do
  [ -e /dev/cu.usbmodem18063371376 ] && { echo "BENCH SEED PRESENT - abort"; exit 3; }
  if dfu-util -l 2>/dev/null | grep -q "0483:df11"; then if ! dfu-util -l 2>/dev/null | grep -q "/0x90"; then echo "ROM DFU (no QSPI region): refusing to flash. Tap RESET on the Seed, then RESET + BOOT during the LED pulse for the Daisy bootloader."; exit 4; fi;
    out=$(dfu-util -a 0 -s 0x90040000:leave -D $B -d ,0483:df11 2>&1 | tr '\r' '\n')
    if echo "$out" | grep -q "File downloaded successfully"; then echo FLASHED; sleep 12; $PY $T/rd.py $P bringup/logs/last_hello.txt; curl -s -X POST -H 'x-ht: 1' --data-binary '__take' http://127.0.0.1:8731/cmd >/dev/null; sleep 10; echo "PORT SETTLED (wait done, safe to open)"; exit 0; fi
    echo "fail: $(echo "$out" | tail -1)"; sleep 1; continue
  fi
  if [ -e $P ]; then
    if lsof $P >/dev/null 2>&1; then [ $sent = 0 ] && { curl -s -X POST -H 'x-ht: 1' --data-binary '__release 90' http://127.0.0.1:8731/cmd >/dev/null; sent=1; }
    else $PY -c "import serial;s=serial.Serial('$P',115200,timeout=0.3);s.dtr=True;s.write(b'dfu\n');s.flush();s.close()" 2>/dev/null; sleep 1.5; fi
  fi
  sleep 0.15
done 2>/dev/null
echo TIMEOUT; exit 1
