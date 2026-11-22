#!/bin/bash
# rollback-neptr-pool.sh — undo deploy-neptr-pool.sh from its backup. ONE COMMAND:
#
#   APPLY=yes CONFIRM_LIVE_RESTART=yes bash setup-script/rollback-neptr-pool.sh        # undo everything
#   bash setup-script/rollback-neptr-pool.sh                                           # dry run: prints the plan
#   APPLY=yes CONFIRM_LIVE_RESTART=yes bash setup-script/rollback-neptr-pool.sh engine # only the engine phase
#   ... io | stage | all (default all, order: engine -> io -> stage)
#   BACKUP=<dir> picks a specific backup (default: ~/demiurge/backups/neptr-pool/LATEST)
#
# What each phase undoes (read from the backup MANIFEST, never guessed):
#   engine  old demiurge-launcher binary back; --omacro:PEDAL_VIA_POOL=1 removed from live.conf
#           (surgically: the rest of live.conf is NOT overwritten, so edits made since survive);
#           the USS slot emptied (`demiurge-uss off`); restarts demiurge.service  -> RESTARTS CSOUND.
#   io      old demiurge-io package + unit back, daemon-reload; demiurge-io restored to its prior
#           state (running/stopped); neptr-bridge started again if it was running before.
#   stage   resources/, wrappers, demiurge-uss, ~/demiurge/uss, default patch maps, midi_handler.orc
#           restored from backup; files that did not exist before are removed (only those listed NEW).
# Restarting needs CONFIRM_LIVE_RESTART=yes. Dry run unless APPLY=yes. Never deletes anything it did
# not create. After `engine`, the engine funnel (ch7 CC14/15 -> CC29 inside csound) is live again
# and the pedal works exactly as it did before the migration, with or without demiurge-io.
set -euo pipefail

PHASE="${1:-all}"
APPLY="${APPLY:-no}"
CONFIRM="${CONFIRM_LIVE_RESTART:-no}"
DEM="${DEMIURGE_HOME:-$HOME/demiurge}"
OPT="${OPT:-/opt/demiurge}"
BACKUP_ROOT="${BACKUP_ROOT:-$DEM/backups/neptr-pool}"
BK="${BACKUP:-$BACKUP_ROOT/LATEST}"
SUDO=""; [ "$(id -u)" -eq 0 ] || SUDO="sudo"
SANDBOX="${SANDBOX:-0}"; [ "$SANDBOX" = 1 ] && SUDO=""      # tests only, see deploy-neptr-pool.sh
SYSTEMD_DIR="${SYSTEMD_DIR:-/etc/systemd/system}"

say()  { printf '%s\n' "$*"; }
die()  { printf 'REFUSING: %s\n' "$*" >&2; exit 1; }
act()  { say "+ $*"; if [ "$APPLY" = yes ]; then "$@"; fi; }
sact() { say "+ $SUDO $*"; if [ "$APPLY" = yes ]; then $SUDO "$@"; fi; }
sysd() { say "+ $SUDO systemctl $*"; if [ "$APPLY" = yes ] && [ "$SANDBOX" != 1 ]; then $SUDO systemctl "$@"; fi; }

[ -f "$BK/MANIFEST" ] || die "no backup manifest at $BK/MANIFEST (nothing was staged, or BACKUP= is wrong)"
BK="$(cd "$BK" && pwd -P)"
say "rolling back from: $BK"
state() { sed -n "s/^STATE $1=//p" "$BK/MANIFEST" | head -1; }

restart_gate() {
  [ "$APPLY" = yes ] || return 0
  [ "$CONFIRM" = yes ] || die "this phase restarts live services; re-run with CONFIRM_LIVE_RESTART=yes"
}

restore_path() {   # path -> BACKUP: put the copy back; NEW: remove what we created
  local p="$1"
  if grep -qxF "BACKUP $p" "$BK/MANIFEST"; then
    [ -e "$BK/files$p" ] || [ -L "$BK/files$p" ] || die "manifest lists $p but the copy is missing"
    sact rm -rf "$p"
    sact cp -a "$BK/files$p" "$p"
  elif grep -qxF "NEW $p" "$BK/MANIFEST"; then
    sact rm -rf "$p"
  fi
}
listed() { sed -n 's/^\(BACKUP\|NEW\) //p' "$BK/MANIFEST"; }

phase_done() { grep -qxF "PHASE $1" "$BK/MANIFEST"; }

rb_engine() {
  say "== rollback engine =="
  phase_done engine || { say "(engine phase was never applied in this backup: nothing to undo, no restart)"; return 0; }
  restart_gate
  # empty the USS slot first (removes the live.conf block, the uss-* manifests and links)
  if [ -x "$OPT/bin/demiurge-uss" ]; then act env DEMIURGE_ROOT="$DEM" "$OPT/bin/demiurge-uss" off || true; fi
  restore_path "$OPT/bin/demiurge-launcher"
  if grep -q 'PEDAL_VIA_POOL' "$DEM/live.conf" 2>/dev/null; then
    act cp -a "$DEM/live.conf" "$DEM/live.conf.bak-rollback-$(date +%Y%m%d-%H%M%S)"
    act sed -i 's/[[:space:]]*--omacro:PEDAL_VIA_POOL=1//' "$DEM/live.conf"
    # a csound_extra line we created from nothing is now empty: drop it
    act sed -i '/^csound_extra[[:space:]]*=[[:space:]]*$/d' "$DEM/live.conf"
    act sync
  fi
  if [ "$(state engine_active)" = active ] || [ "$APPLY" != yes ]; then
    sysd restart demiurge.service
    if [ "$APPLY" = yes ] && [ "$SANDBOX" != 1 ]; then
      for i in $(seq 1 40); do pgrep -x csound >/dev/null && break; sleep 1; done
      pgrep -x csound >/dev/null && say "csound is back (pid $(pgrep -x csound | head -1))" || { journalctl -u demiurge -n 30 --no-pager; die "csound did not return: check the journal, then restore $BK/files$DEM/live.conf by hand"; }
    fi
  fi
}

rb_io() {
  say "== rollback io =="
  phase_done io || { say "(io phase was never applied in this backup: demiurge-io untouched, no restart)"; return 0; }
  restart_gate
  restore_path "$OPT/src/demiurge-io"
  restore_path "$SYSTEMD_DIR/demiurge-io.service"
  sysd daemon-reload
  if [ "$(state io_active)" = active ]; then sysd restart demiurge-io
  else sysd stop demiurge-io || true; fi
  [ "$(state bridge_active)" = active ] && sysd start neptr-bridge || true
}

rb_stage() {
  say "== rollback stage (files) =="
  phase_done stage || { say "(stage phase was never applied in this backup)"; return 0; }
  while IFS= read -r p; do
    case "$p" in
      "$OPT/bin/demiurge-launcher"|"$OPT/src/demiurge-io"|"$SYSTEMD_DIR/demiurge-io.service") continue ;;
      "$DEM/live.conf") continue ;;     # never restored wholesale: the engine rollback edits it surgically
    esac
    restore_path "$p"
  done < <(listed)
  # directories the deploy had to create (only removed when now empty)
  for d in "$DEM/patches" "$DEM/params"; do
    [ "$APPLY" = yes ] && rmdir "$d" 2>/dev/null || true
  done
}

case "$PHASE" in
  engine) rb_engine ;;
  io)     rb_io ;;
  stage)  rb_stage ;;
  all)    rb_engine; rb_io; rb_stage ;;
  *)      die "unknown phase '$PHASE' (engine | io | stage | all)" ;;
esac
[ "$APPLY" = yes ] && { : > "$BK/ROLLED_BACK"; say "rollback done (backup closed: the next deploy starts a fresh one)."; } || say "
(DRY RUN — nothing was changed. APPLY=yes CONFIRM_LIVE_RESTART=yes to apply.)"
