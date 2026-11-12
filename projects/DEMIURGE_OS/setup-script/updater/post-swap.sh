#!/usr/bin/env bash
# post-swap — run by demiurge-update-swap right after a successful flip, as root, from the NEW tree.
#  (a) self-update the updater: stage/swap/revert/apply scripts -> $DEMIURGE_BIN_DIR,
#      units -> $DEMIURGE_UNIT_DIR, only files that differ (atomic install), daemon-reload on change.
#  (b) queue a user-level apply: $ROOT/local/apply-pending = new hash (demiurge-apply.service consumes it).
# Never restarts demiurge.service or any audio unit. Idempotent. Failures are non-fatal to the swap.
set -uo pipefail
ROOT="${DEMIURGE_ROOT:-/demiurge}"
BIN_DIR="${DEMIURGE_BIN_DIR:-/opt/demiurge/bin}"
UNIT_DIR="${DEMIURGE_UNIT_DIR:-/etc/systemd/system}"
SYSTEMCTL="${SYSTEMCTL:-systemctl}"
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
log() { echo "[post-swap] $*"; }
CHANGED=0 NEWUNIT=0
# put <mode> <src> <dst>: atomic replace only when content differs.
put() {
  [ -f "$2" ] || return 0
  if [ -f "$3" ] && cmp -s "$2" "$3"; then return 0; fi
  install -d "$(dirname "$3")"
  install -m "$1" "$2" "$3.new.$$" && mv -f "$3.new.$$" "$3" || { rm -f "$3.new.$$"; log "could not install $3"; return 1; }
  CHANGED=1; log "updated $3"
}
for n in stage swap revert; do put 0755 "$D/demiurge-update-$n.sh" "$BIN_DIR/demiurge-update-$n"; done
put 0755 "$D/demiurge-apply.sh" "$BIN_DIR/demiurge-apply"
for u in "$D"/demiurge-update-*.service "$D"/demiurge-update-*.timer "$D"/demiurge-apply.service; do
  [ -f "$u" ] || continue
  [ -f "$UNIT_DIR/$(basename "$u")" ] || NEWUNIT=1
  put 0644 "$u" "$UNIT_DIR/$(basename "$u")"
done
if [ "$CHANGED" = 1 ]; then "$SYSTEMCTL" daemon-reload || log "daemon-reload failed"; fi
# enable (never start audio) the apply unit if it is not enabled yet.
if ! "$SYSTEMCTL" is-enabled demiurge-apply.service >/dev/null 2>&1; then
  "$SYSTEMCTL" enable demiurge-apply.service >/dev/null 2>&1 || log "could not enable demiurge-apply"
  # enabled mid-boot is too late for this boot's transaction: queue it (ordering waits for user@/network).
  "$SYSTEMCTL" start --no-block demiurge-apply.service >/dev/null 2>&1 || true
fi
HASH="$(basename "$(readlink -f "$ROOT/current")")"
install -d "$ROOT/local"
printf '%s\n' "$HASH" > "$ROOT/local/apply-pending.tmp.$$" && mv -f "$ROOT/local/apply-pending.tmp.$$" "$ROOT/local/apply-pending"
log "apply-pending=$HASH"
exit 0
