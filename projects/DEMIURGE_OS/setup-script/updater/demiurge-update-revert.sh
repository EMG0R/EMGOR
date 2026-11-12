#!/usr/bin/env bash
# demiurge-update-revert — flip `current` back to `previous` (symlink flip). SYNC bucket.
# Takes effect at next boot/service start; does NOT restart audio. Run as root.
set -euo pipefail
ROOT="${DEMIURGE_ROOT:-/demiurge}"
[ -L "$ROOT/previous" ] && [ -L "$ROOT/current" ] || { echo "no previous version to revert to"; exit 1; }
CUR="$(basename "$(readlink -f "$ROOT/current")")"; PREV="$(basename "$(readlink -f "$ROOT/previous")")"
[ -d "$ROOT/versions/$PREV" ] || { echo "previous tree $PREV is gone"; exit 1; }
ln -sfn "versions/$CUR"  "$ROOT/previous.tmp" && mv -T "$ROOT/previous.tmp" "$ROOT/previous"
ln -sfn "versions/$PREV" "$ROOT/current.tmp"  && mv -T "$ROOT/current.tmp"  "$ROOT/current"
rm -f "$ROOT/next"
touch "$ROOT/local/NO_SWAP"   # don't re-apply the bad version on next boot; rm to resume updates
echo "reverted $CUR -> $PREV. Updates paused (rm $ROOT/local/NO_SWAP to resume). Reboot to run it."
