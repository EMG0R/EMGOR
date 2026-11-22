#!/bin/bash
# flash_bela.sh — push hyperGuitar6chPan to the Bela, rebuild, restart, and
# reset the persisted mix gains to the code defaults (the on-Bela
# hyperguitar-mix.conf otherwise overrides them at boot).
# Usage: tools/flash_bela.sh [bela-host]   (default 192.168.7.2; via the Pi: -J gorpi7@demiurge.local)
set -e
H="${1:-192.168.7.2}"; J="${BELA_JUMP:+-J $BELA_JUMP}"
SRC="$(cd "$(dirname "$0")/../bela/hyperGuitar6chPan" && pwd)"
rsync -a --exclude build --exclude hosttest --exclude '*.o' --exclude hyperguitar-mix.conf -e "ssh -o ConnectTimeout=10 $J" "$SRC/" "root@$H:/root/Bela/projects/hyperGuitar6chPan/"
ssh -o ConnectTimeout=10 $J "root@$H" 'set -e
  rm -rf /root/Bela/projects/hyperGuitar6chPan/hosttest /root/Bela/projects/hyperGuitar6chPan/build/hosttest
  make -C /root/Bela PROJECT=hyperGuitar6chPan 2>&1 | grep -E "error|Error" && exit 1
  rm -f /root/Bela/projects/hyperGuitar6chPan/hyperguitar-mix.conf
  make -C /root/Bela PROJECT=hyperGuitar6chPan stop run >/dev/null 2>&1 || true
  sleep 3; pgrep -a hyperGuitar6chPan | head -1'
echo "flashed $H; mix gains = code defaults (piezo 2.5, trs 1.0, outA 1.0, outB 1.0)"
