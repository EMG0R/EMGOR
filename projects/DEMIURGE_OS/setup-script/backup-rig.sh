#!/bin/bash
#
# backup-rig.sh — PULL the Pi rig's user-owned state to the Mac. Copy-only.
#
# WHY: the 2026-10-01 SD card death lost every file that was not in the repo
# (live.conf tuning, sets/, presets, state, the codec mixer, generated audio
# fragments). Nothing backed any of it up. Run this after the rig sounds right,
# and the NEXT card death is a ~10 minute restore (restore-rig.sh + the
# installer, which seeds from rig-backup/latest/<host>/).
#
# SAFE: reads only. It never writes, moves or deletes anything on the Pi, uses
# no rsync --delete, and is fine to run while the engine is playing. The only
# thing it removes is old local tarballs beyond the newest 5, on the Mac.
#
# Layout written (mirrors Pi paths):
#   rig-backup/latest/<host>/home/...   ~/demiurge/*, ~/.config/{wireplumber,pipewire}
#   rig-backup/latest/<host>/dev/shm/neptr-state   RAM state (flushed copy)
#   rig-backup/latest/<host>/etc|var|boot/...      system files (via ssh sudo tar)
#   rig-backup/snapshots/<host>-<YYYYmmdd-HHMM>.tar.gz   dated, last 5 kept
#
# Usage:
#   setup-script/backup-rig.sh                 # target from pi-rig.conf
#   PI_USER=x PI_HOST=y setup-script/backup-rig.sh   # one-off override
#
# Exit status: non-zero only if ssh to the Pi fails.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

[[ -f "$ROOT/pi-rig.conf" ]] && source "$ROOT/pi-rig.conf"
PI_USER="${PI_USER:-${PI_USER_DEFAULT:-gorpi7v2}}"
PI_HOST="${PI_HOST:-${PI_HOST_DEFAULT:-demiurge.local}}"
TARGET="$PI_USER@$PI_HOST"

SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=8 -o ServerAliveInterval=5 -o ServerAliveCountMax=3)
SSH=(ssh "${SSH_OPTS[@]}" "$TARGET")

DEST="$ROOT/rig-backup/latest/$PI_HOST"
SNAPS="$ROOT/rig-backup/snapshots"
STAMP="$(date +%Y%m%d-%H%M)"

echo "=== rig backup  $TARGET -> rig-backup/latest/$PI_HOST ==="

# Preflight: the one hard failure.
if ! "${SSH[@]}" true 2>/dev/null; then
    echo "FAIL: cannot ssh to $TARGET (offline, wrong name, or no key). Nothing copied." >&2
    exit 1
fi

mkdir -p "$DEST/home" "$DEST/dev/shm" "$SNAPS"
WARN=0
note_warn() { echo "  WARN: $*" >&2; WARN=$((WARN + 1)); }

# --- user files, relative to the Pi home -----------------------------------
USER_PATHS=(
    demiurge/live.conf demiurge/sets demiurge/presets demiurge/state
    demiurge/interface.conf demiurge/graph.conf demiurge/touch-cal.conf
    demiurge/ui-state.conf demiurge/asound.state demiurge/backups
    .config/wireplumber .config/pipewire
)
# Ask the Pi which exist, so a missing one is not an error (portable: no
# --ignore-missing-args, the Mac rsync may be openrsync).
CAND="${USER_PATHS[*]}"
EXIST="$("${SSH[@]}" "cd \"\$HOME\" && for p in $CAND; do [ -e \"\$p\" ] && echo \"\$p\"; done" 2>/dev/null)"

N_USER=0
while IFS= read -r p; do
    [[ -z "$p" ]] && continue
    # -R keeps the path relative to ~ so it lands under home/.
    if rsync -aR -e "ssh ${SSH_OPTS[*]}" "$TARGET:$p" "$DEST/home/" >/dev/null 2>&1; then
        N_USER=$((N_USER + 1))
    else
        note_warn "rsync failed for ~/$p"
    fi
done <<< "$EXIST"
for p in "${USER_PATHS[@]}"; do
    grep -qx "$p" <<< "$EXIST" || echo "  absent on Pi: ~/$p"
done

# --- RAM state (flushed copy of the tmpfs state dir) ------------------------
N_SHM=0
if "${SSH[@]}" '[ -d /dev/shm/neptr-state ]' 2>/dev/null; then
    mkdir -p "$DEST/dev/shm"
    if rsync -a -e "ssh ${SSH_OPTS[*]}" "$TARGET:/dev/shm/neptr-state" "$DEST/dev/shm/" >/dev/null 2>&1; then
        N_SHM=1
    else
        note_warn "rsync failed for /dev/shm/neptr-state"
    fi
else
    echo "  absent on Pi: /dev/shm/neptr-state"
fi

# --- system files (root-readable) via ssh sudo tar --------------------------
# Globs expand under sudo so unreadable dirs (sudoers.d) still list. Missing
# paths are simply not named. tar only reads.
SYS_LIST='etc/wireplumber/wireplumber.conf.d etc/pipewire/pipewire.conf.d
 etc/systemd/system/demiurge*.service* etc/systemd/system/demiurge*.timer
 etc/systemd/system/demiurge.service.d etc/sudoers.d
 var/lib/alsa/asound.state boot/firmware/config.txt boot/firmware/cmdline.txt
 boot/firmware/demiurge.conf'
if "${SSH[@]}" "sudo -n sh -c 'cd / && tar -cf - \$(ls -d ${SYS_LIST//$'\n'/ } 2>/dev/null)'" 2>/dev/null \
        | tar -xf - -C "$DEST" 2>/dev/null; then
    :
else
    note_warn "system-file tar reported an error (sudo -n needs NOPASSWD); user files are still saved"
fi
N_SYS="$(find "$DEST/etc" "$DEST/var" "$DEST/boot" -type f 2>/dev/null | wc -l | tr -d ' ')"

# --- dated tarball, keep last 5 --------------------------------------------
SNAP="$SNAPS/$PI_HOST-$STAMP.tar.gz"
if tar -czf "$SNAP" -C "$ROOT/rig-backup/latest" "$PI_HOST" 2>/dev/null; then
    # prune only OUR local tarballs beyond the newest 5
    ls -1t "$SNAPS"/"$PI_HOST"-*.tar.gz 2>/dev/null | tail -n +6 | while IFS= read -r old; do rm -f -- "$old"; done
else
    note_warn "could not write $SNAP"
fi

echo
echo "--- summary ---"
echo "  user paths pulled : $N_USER / ${#USER_PATHS[@]}"
echo "  ram state         : $([[ $N_SHM -eq 1 ]] && echo copied || echo none)"
echo "  system files      : $N_SYS"
echo "  live.conf         : $([[ -f $DEST/home/demiurge/live.conf ]] && echo saved || echo MISSING)"
echo "  asound.state      : $([[ -f $DEST/home/demiurge/asound.state ]] && echo 'saved (user copy)' || echo 'no user copy: run demiurge-mixer store on the Pi')"
echo "  snapshot          : ${SNAP#$ROOT/}  ($(ls -1 "$SNAPS"/"$PI_HOST"-*.tar.gz 2>/dev/null | wc -l | tr -d ' ') kept, max 5)"
echo "  warnings          : $WARN"
echo "Review, then commit rig-backup/latest/ yourself."
exit 0
