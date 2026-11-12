#!/bin/sh
# backup_presets.sh — snapshot the preset banks, on the Pi AND to the Mac.
# Run it after every few presets. Cheap, idempotent, never overwrites.
set -e
PI="${PI:-gorpi7@demiurge.local}"
TS=$(date +%Y%m%d-%H%M%S)
HERE=$(cd "$(dirname "$0")/.." && pwd)
DEST="$HERE/state_backups/gorcore"
mkdir -p "$DEST/$TS"

ssh -o ConnectTimeout=20 "$PI" "mkdir -p ~/demiurge/presets/gorcore/backups && \
  cp ~/demiurge/presets/gorcore/bank.ftab   ~/demiurge/presets/gorcore/backups/bank.ftab.$TS && \
  cp ~/demiurge/presets/gorcore/bank.b.ftab ~/demiurge/presets/gorcore/backups/bank.b.ftab.$TS && sync"
scp -q -o ConnectTimeout=20 "$PI:~/demiurge/presets/gorcore/bank.ftab"   "$DEST/$TS/"
scp -q -o ConnectTimeout=20 "$PI:~/demiurge/presets/gorcore/bank.b.ftab" "$DEST/$TS/"
# readable copy too, so a bank is reviewable/diffable without the rig
python3 "$HERE/tools/preset_bank.py" export "$DEST/$TS/presets.json" --bank "$DEST/$TS" >/dev/null
echo "backed up -> $DEST/$TS  (and on the Pi as *.$TS)"
python3 "$HERE/tools/preset_bank.py" dump --bank "$DEST/$TS" | head -2
