#!/usr/bin/env bash
# flash_pi.sh BIN [note] : flash PMOR from the Pi (the Linux flash.sh). Only flash images that passed ./check.sh.
# Opens the CDC port (open() asserts DTR on Linux and we hold it open while writing), sends "dfu", catches the
# bootloader with dfu-util at 0x90040000, logs to docs/FLASH_LOG.md. Never unbinds USB.
set -u
BIN="$1"; NOTE="${2:-}"; ADDR=0x90040000
here="$(cd "$(dirname "$0")/.." && pwd)"
[ -s "$BIN" ] || { echo "no such image $BIN"; exit 1; }
# ---- gate: clean tree, HEAD pushed, ./check.sh passed for this exact HEAD and this exact image
cd "$here" || exit 1
if [ "${PMOR_FLASH_ROLLBACK:-0}" != 1 ]; then   # rollback to a known-good image is the only bypass (explicit env)
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: working tree not clean"; exit 1; }
HEADSHA=$(git rev-parse HEAD)
git fetch -q origin 2>/dev/null
git branch -r --contains "$HEADSHA" 2>/dev/null | grep -q . || { echo "REFUSED: HEAD $HEADSHA is not pushed to origin"; exit 1; }
[ -s .check_stamp ] || { echo "REFUSED: no check stamp - run ./check.sh (full) on a clean tree"; exit 1; }
read -r STAMP_COMMIT STAMP_BIN < .check_stamp
[ "$STAMP_COMMIT" = "$HEADSHA" ] || { echo "REFUSED: check.sh passed for $STAMP_COMMIT, HEAD is $HEADSHA"; exit 1; }
[ "$(sha256sum "$BIN" | cut -d' ' -f1)" = "$STAMP_BIN" ] || { echo "REFUSED: $BIN is not the image check.sh built and passed"; exit 1; }
fi
HASH=$(sha256sum "$BIN" | cut -c1-16); REV=$(git -C "$here" rev-parse --short HEAD)
PORT=$(ls /dev/ttyACM* 2>/dev/null | head -1)
if [ -n "$PORT" ]; then
  echo ">>> dfu via $PORT"
  stty -F "$PORT" raw -echo 2>/dev/null
  exec 3<>"$PORT"; sleep 0.3; printf 'dfu\r\n' >&3; sleep 0.4; exec 3>&-
else echo ">>> no CDC port (board unresponsive? hold BOOT + tap RESET)"; fi
RES="NOT FLASHED"
for i in $(seq 1 80); do
  if dfu-util -l 2>&1 | grep -qi 'found dfu'; then
    OUT=$(dfu-util -a 0 -s ${ADDR}:leave -D "$BIN" -d ,0483:df11 2>&1)
    if echo "$OUT" | grep -q "File downloaded successfully"; then RES="FLASHED"; break; fi
    echo "$OUT" | grep -iE "not writeable|error" | head -1
  fi
  sleep 0.25
done
echo ">>> $RES"
printf '| %s | %s | %s | sha256 %s | %s | %s |\n' "$(date '+%F %T')" "$REV" "$(basename "$BIN")" "$HASH" "$RES" "$NOTE" >> "$here/docs/FLASH_LOG.md"
[ "$RES" = FLASHED ]
