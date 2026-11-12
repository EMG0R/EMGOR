#!/bin/bash
# deploy-clock.sh -- stage sync.conf for role=opgorator (PMOR = clock master).
# Run AFTER the Demiurge install finishes. Starts/restarts NOTHING.
# Does not touch live.conf (sync_layer stays off) or /boot/firmware.
set -u
BIN=/opt/demiurge/bin/demiurge-clock
SYNCBIN=/usr/local/bin/demiurge-sync
SYNC="$HOME/demiurge/sync.conf"
rc=0

[[ -x $BIN ]]      && echo "ok: $BIN" || { echo "MISSING: $BIN (install Phase 6 not done?)"; rc=1; }
[[ -x $SYNCBIN ]]  && echo "ok: $SYNCBIN" || echo "note: $SYNCBIN missing (optional; this script writes sync.conf itself)"
systemctl cat demiurge.service >/dev/null 2>&1 && echo "ok: demiurge.service unit exists (launcher spawns demiurge-clock; there is no separate clock unit)" \
  || { echo "MISSING: demiurge.service unit"; rc=1; }
[[ $rc -ne 0 ]] && { echo "aborting, nothing written"; exit $rc; }

mkdir -p "$(dirname "$SYNC")"
if [[ -f $SYNC ]]; then
  cp -p "$SYNC" "$SYNC.bak.$(date +%Y%m%d-%H%M%S)" && echo "backup: $SYNC.bak.*"
  # edit in place: set role/source lines, keep everything else
  for kv in "role=opgorator" "source=manual"; do
    k=${kv%%=*}; v=${kv#*=}
    if grep -qE "^[[:space:]]*$k[[:space:]]*=" "$SYNC"; then
      sed -i -E "s|^[[:space:]]*$k[[:space:]]*=.*|$k = $v|" "$SYNC"
    else echo "$k = $v" >> "$SYNC"; fi
  done
else
  cat > "$SYNC" <<'CONF'
# DEMIURGE clock role -- same format demiurge-sync writes.
# opgorator = OpGorator (PMOR) is clock master, Demiurge follows.
role = opgorator
# manual = a human chose; launcher auto-switch (--auto) will not override.
source = manual
CONF
fi
sync
echo "--- $SYNC ---"; cat "$SYNC"

cat <<'V'

Verify (daemon reads sync.conf only at startup; live flip: `demiurge-sync opgorator` sends CC116):
  live.conf: grep sync_layer ~/demiurge/live.conf   # must stay: off
  pgrep -a demiurge-clock                           # is it running at all? (see README caveat)
  aconnect -l                                       # PMOR client (name matches OpGorator|Daisy|PMOR) and demiurge_clock;
                                                    # want PMOR "Connecting To: 14:0" and 14:0 -> PMOR
  aseqdump -p "<PMOR client name or id>"            # expect Clock ticks, Start/Stop
  aseqdump -p "Midi Through" | grep -i "controller 116\|controller 119"
  cat ~/demiurge/clock_status.conf                  # ext_clock / pmor_transport
V
# --- RESTART (needs Emory's go-ahead; engine/UI must not be left down) ---
# sudo systemctl restart demiurge.service
