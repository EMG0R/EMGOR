#!/usr/bin/env bash
# demiurge-update-swap — BOOT-TIME atomic swap. current -> staged `next`. SYNC bucket.
# SHIPPED DISABLED. Runs before demiurge.service at boot, never at runtime.
# The flip is one rename(2) of a symlink; old tree stays in versions/ (revert = flip back).
set -euo pipefail
ROOT="${DEMIURGE_ROOT:-/demiurge}"
log() { echo "[update-swap] $*"; }

[ -e "$ROOT/local/NO_SWAP" ] && { log "NO_SWAP present; skipping"; exit 0; }
[ -L "$ROOT/next" ] || { log "nothing staged"; exit 0; }
FIRST_INSTALL="${DEMIURGE_FIRST_INSTALL:-0}"   # 1 = sibling instance (COD): create current/ if absent
if [ ! -L "$ROOT/current" ]; then
  { [ "$FIRST_INSTALL" = 1 ] && [ ! -e "$ROOT/current" ]; } || { log "$ROOT/current is a real dir or missing; refusing to touch"; exit 0; }
fi

NEXT="$(readlink -f "$ROOT/next")"; CUR="$(readlink -f "$ROOT/current")"
if [ ! -L "$ROOT/current" ]; then   # first install: nothing to demote
  ln -sfn "versions/$(basename "$NEXT")" "$ROOT/current.tmp" && mv -T "$ROOT/current.tmp" "$ROOT/current"
  rm -f "$ROOT/next"; log "first install: current -> $(basename "$NEXT")"; exit 0
fi
[ -d "$NEXT" ] || { log "next points nowhere; clearing"; rm -f "$ROOT/next"; exit 0; }
[ "$NEXT" = "$CUR" ] && { rm -f "$ROOT/next"; exit 0; }

ln -sfn "versions/$(basename "$CUR")" "$ROOT/previous.tmp" && mv -T "$ROOT/previous.tmp" "$ROOT/previous"
ln -sfn "versions/$(basename "$NEXT")" "$ROOT/current.tmp" && mv -T "$ROOT/current.tmp" "$ROOT/current"
rm -f "$ROOT/next"
log "swapped: $(basename "$CUR") -> $(basename "$NEXT")  (revert: demiurge-update-revert)"

# Post-swap hook (from the NEW tree): self-update the updater, queue a user-level apply.
# Best effort: a failure or hang here never undoes the swap and never blocks boot.
HOOK="$ROOT/current/setup-script/updater/post-swap.sh"
if [ -f "$HOOK" ]; then
  if DEMIURGE_ROOT="$ROOT" timeout "${DEMIURGE_POSTSWAP_TIMEOUT:-60}" bash "$HOOK"; then log "post-swap ok"
  else log "post-swap FAILED (rc=$?); swap kept"; fi
fi
exit 0
