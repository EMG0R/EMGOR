#!/usr/bin/env bash
# demiurge-update-stage — check EMGOR's git remote; if newer, DOWNLOAD + STAGE only.
# SHIPPED DISABLED (see README.md). SYNC bucket (ships to all Pis).
#
# Never touches `current`, never restarts anything, never runs the installer.
# Result: /demiurge/versions/<hash>/ plus a `next` symlink. The swap happens only
# at next boot (demiurge-update-swap.sh).
set -euo pipefail

ROOT="${DEMIURGE_ROOT:-/demiurge}"
CONF="$ROOT/local/updater.conf"          # LOCAL bucket: remote URL, pause flag
[ -f "$CONF" ] && . "$CONF"
REMOTE="${DEMIURGE_REMOTE:-}"            # public side of EMGOR's repo; set in updater.conf
BRANCH="${DEMIURGE_BRANCH:-main}"
KEEP="${DEMIURGE_KEEP_VERSIONS:-4}"
# Parameterization for a sibling instance (the COD updater reuses this script with
# DEMIURGE_ROOT=<cod root>; see setup-script/cod/). Defaults = the Demiurge behaviour.
VERIFY="${DEMIURGE_VERIFY_FILES:-setup-script/setup-demiurge.sh install.sh MD/DEMIURGE.md}"
NO_LOCAL_LINK="${DEMIURGE_NO_LOCAL_LINK:-0}"   # 1 = no demiurge/local symlink (non-Demiurge tree)
FIRST_INSTALL="${DEMIURGE_FIRST_INSTALL:-0}"   # 1 = allow staging when current/ does not exist yet

log() { echo "[update-stage] $*"; }
[ -n "$REMOTE" ] || { log "no DEMIURGE_REMOTE set in $CONF; nothing to do"; exit 0; }
[ -e "$ROOT/local/UPDATER_PAUSED" ] && { log "paused"; exit 0; }
[ -L "$ROOT/current" ] || [ "$FIRST_INSTALL" = 1 ] || { log "$ROOT/current is not a symlink; layout not migrated, refusing"; exit 0; }

mkdir -p "$ROOT/versions"
exec 9>"$ROOT/.stage.lock"; flock -n 9 || { log "another stage run active"; exit 0; }

# Quiet-network guard: do nothing without a working connection.
timeout 10 git ls-remote --exit-code "$REMOTE" >/dev/null 2>&1 || { log "remote unreachable"; exit 0; }
HASH="$(timeout 30 git ls-remote "$REMOTE" "refs/heads/$BRANCH" | cut -c1-12)"
[ -n "$HASH" ] || { log "branch $BRANCH not found"; exit 0; }

CUR="$(basename "$(readlink -f "$ROOT/current")")"
if [ "$HASH" = "$CUR" ] || [ -d "$ROOT/versions/$HASH" ]; then
  log "up to date or already staged ($HASH)"; exit 0
fi

# LIVE-AUDIO INTERLOCK. A git clone of this tree is memory-heavy, and memory-heavy
# work wrecks the audio engine EVEN ON AN ISOLATED CORE: isolcpus isolates CPU
# scheduling, not RAM bandwidth, and the engine is bandwidth-bound. Measured on
# 2026-10-07: 2,543 underrun warnings in 40 min from one profiling job, zero before
# it and zero in the 60 s after it was killed.
#
# nice/ionice do NOT cover this — they schedule CPU and IO, not bandwidth — so the
# low priority below is necessary and insufficient. Wait for a window instead; the
# timer will come back around, and a skipped update costs nothing while a glitch
# during a set costs the set.
GUARD="$(command -v demiurge-live-guard || echo /opt/demiurge/bin/demiurge-live-guard)"
if [ -x "$GUARD" ]; then
  if ! "$GUARD" --wait "${DEMIURGE_UPDATE_WAIT:-120}" 2>/dev/null; then
    log "instrument is live — deferring the clone to a later run (set DEMIURGE_ALLOW_HEAVY=1 to force)"
    exit 0
  fi
else
  log "WARNING: demiurge-live-guard not installed; cloning without a live-audio interlock"
fi

TMP="$ROOT/versions/.staging-$HASH"
rm -rf "$TMP"
log "staging $HASH (nice/ionice low + live-audio guard passed)"
git clone --quiet --depth 1 --branch "$BRANCH" "$REMOTE" "$TMP"
FULL="$(git -C "$TMP" rev-parse HEAD)"
[ "${FULL:0:12}" = "$HASH" ] || { log "remote moved during clone; retry next run"; rm -rf "$TMP"; exit 0; }

# Verify before it can ever become `next`. Only require files that exist in the
# PUBLIC distro export — NOT demiurge/live.conf (personal/runtime, excluded from public).
for f in $VERIFY; do
  [ -e "$TMP/$f" ] || { log "staged tree missing $f; discarding"; rm -rf "$TMP"; exit 1; }
done
for s in "$TMP"/setup-script/*.sh "$TMP"/install.sh; do
  [ -f "$s" ] || continue
  bash -n "$s" || { log "syntax error in $s; discarding"; rm -rf "$TMP"; exit 1; }
done

# LOCAL bucket stays outside the version: link it in.
rm -rf "$TMP/.git"
if [ "$NO_LOCAL_LINK" != 1 ]; then
  rm -rf "$TMP/demiurge/local"
  # mkdir -p: the PUBLIC export has no demiurge/, so this ln would fail and the
  # updater would discard every staged version. No-op when demiurge/ is present.
  mkdir -p "$TMP/demiurge"
  ln -s "$ROOT/local" "$TMP/demiurge/local"
fi
echo "$FULL" > "$TMP/.version"
mv -T "$TMP" "$ROOT/versions/$HASH"             # atomic rename
ln -sfn "versions/$HASH" "$ROOT/next.tmp" && mv -T "$ROOT/next.tmp" "$ROOT/next"
log "staged $HASH; will swap at next boot"

# Keep the newest $KEEP versions; never delete current/previous/next.
KEEPSET="$(readlink -f "$ROOT/current") $(readlink -f "$ROOT/previous" 2>/dev/null || true) $(readlink -f "$ROOT/next")"
ls -1dt "$ROOT"/versions/*/ 2>/dev/null | tail -n +$((KEEP+1)) | while read -r d; do
  case " $KEEPSET " in *" ${d%/} "*) ;; *) rm -rf "$d";; esac
done
