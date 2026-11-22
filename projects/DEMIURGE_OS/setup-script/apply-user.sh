#!/usr/bin/env bash
# apply-user.sh — per-release USER-level installers, run as the rig user by demiurge-apply
# after an update (and idempotent by hand). Add new installers here, one block each.
# Never touches live.conf, ~/.cod-bus.conf, ~/.claude-persist, ~/NEPTR_phase4; never
# starts/restarts any unit (install-cod.sh only enables, dormant).
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
log() { echo "[apply-user] $*"; }
RC=0
COD_ROOT="${COD_ROOT:-$HOME/.local/share/cod-update}"
if [ -f "$HERE/cod/install-cod.sh" ]; then
  if [ -L "$COD_ROOT/current" ]; then MODE=--refresh; else MODE=; fi
  log "cod: install-cod.sh ${MODE:-(first install)}"
  bash "$HERE/cod/install-cod.sh" ${MODE:+$MODE} || { log "cod installer failed"; RC=1; }
fi
exit $RC
