#!/usr/bin/env bash
# demiurge-apply — runs (root, via demiurge-apply.service) the per-release USER-level installers
# when $ROOT/local/apply-pending exists. Never restarts demiurge.service or any audio unit.
# Marker is cleared only on success (a failed/offline apply retries next boot).
set -uo pipefail
ROOT="${DEMIURGE_ROOT:-/demiurge}"
RUNUSER="${RUNUSER:-runuser}"
LINGER_DIR="${DEMIURGE_LINGER_DIR:-/var/lib/systemd/linger}"
log() { echo "[apply] $*"; }
M="$ROOT/local/apply-pending"
[ -f "$M" ] || { log "nothing pending"; exit 0; }
SCRIPT="$ROOT/current/setup-script/apply-user.sh"
[ -f "$SCRIPT" ] || { log "no apply-user.sh in current; clearing marker"; rm -f "$M"; exit 0; }

conf() { # conf <key> <file>: KEY=value or KEY: value
  [ -f "$2" ] && sed -n "s/^[[:space:]]*$1[[:space:]]*[=:][[:space:]]*//p" "$2" | head -1 | tr -d "\"' \r"; }
U="${DEMIURGE_USER:-}"
[ -n "$U" ] || U="$(conf DEMIURGE_USER "$ROOT/local/updater.conf")"
[ -n "$U" ] || U="$(conf DEMIURGE_USER "$ROOT/local/device.md")"
if [ -z "$U" ] && [ -d "$LINGER_DIR" ]; then
  for f in "$LINGER_DIR"/*; do [ -e "$f" ] && id -u "$(basename "$f")" >/dev/null 2>&1 && { U="$(basename "$f")"; break; }; done
fi
[ -n "$U" ] && id -u "$U" >/dev/null 2>&1 || { log "cannot determine rig user; leaving marker"; exit 1; }
UID_="$(id -u "$U")"
log "applying $(cat "$M") as $U"
if "$RUNUSER" -u "$U" -- env XDG_RUNTIME_DIR="/run/user/$UID_" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$UID_/bus" \
     nice -n 19 bash "$SCRIPT"; then
  cp "$M" "$ROOT/local/apply-done" 2>/dev/null; rm -f "$M"; log "done"
else
  log "apply-user FAILED (rc=$?); marker kept for next boot"; exit 1
fi
