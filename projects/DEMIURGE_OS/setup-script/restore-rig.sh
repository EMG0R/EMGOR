#!/bin/bash
#
# restore-rig.sh — push a backup-rig.sh copy back to the Pi's USER files.
#
# Copy-only. An existing Pi file is NEVER overwritten unless --force, and with
# --force the old file is kept beside it as <name>.pre-restore. Nothing is ever
# deleted on the Pi. /etc, /var and /boot are NOT touched: the sudo commands
# for those are printed for you to run yourself, one at a time.
#
# Normal order on a rebuilt card: run setup-demiurge.sh on the Pi (it also
# seeds from rig-backup/latest/<host>/ when ~/demiurge/live.conf is absent),
# then this for anything the installer did not seed, then the printed sudo lines.
#
# Usage:
#   setup-script/restore-rig.sh [--dry-run] [--force]
#   PI_USER=x PI_HOST=y setup-script/restore-rig.sh
# Exit status: non-zero if ssh fails or there is nothing to restore.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DRY=0; FORCE=0
for a in "$@"; do
    case "$a" in
        --dry-run|-n) DRY=1 ;;
        --force) FORCE=1 ;;
        -h|--help) sed -n 2,18p "$0"; exit 0 ;;
        *) echo "unknown arg: $a" >&2; exit 2 ;;
    esac
done

[[ -f "$ROOT/pi-rig.conf" ]] && source "$ROOT/pi-rig.conf"
PI_USER="${PI_USER:-${PI_USER_DEFAULT:-gorpi7v2}}"
PI_HOST="${PI_HOST:-${PI_HOST_DEFAULT:-demiurge.local}}"
TARGET="$PI_USER@$PI_HOST"
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=8 -o ServerAliveInterval=5 -o ServerAliveCountMax=3)
SRC="$ROOT/rig-backup/latest/$PI_HOST"

if [[ ! -d "$SRC/home" ]]; then
    echo "FAIL: no backup at rig-backup/latest/$PI_HOST/home (run backup-rig.sh first)" >&2
    exit 1
fi
if ! ssh "${SSH_OPTS[@]}" "$TARGET" true 2>/dev/null; then
    echo "FAIL: cannot ssh to $TARGET. Nothing pushed." >&2
    exit 1
fi

FLAGS=(-a -v -e "ssh ${SSH_OPTS[*]}")
if [[ $FORCE -eq 1 ]]; then
    FLAGS+=(--backup --suffix=.pre-restore)
    echo "mode: --force (existing files replaced, old copy kept as .pre-restore)"
else
    FLAGS+=(--ignore-existing)
    echo "mode: safe (existing Pi files are skipped)"
fi
[[ $DRY -eq 1 ]] && FLAGS+=(-n) && echo "mode: dry run, nothing written"

echo "=== restore  rig-backup/latest/$PI_HOST/home -> $TARGET:~ ==="
rsync "${FLAGS[@]}" "$SRC/home/" "$TARGET:" || echo "WARN: rsync reported errors" >&2

# RAM state is not pushed: demiurge-state-ram seeds /dev/shm from the SD copy
# (~/demiurge/state) on service start, which is restored above.

echo
echo "--- system files: NOT touched. Run these on the Pi yourself, in order ---"
echo "(copy the backup over first:  rsync -a $SRC/{etc,var,boot} $TARGET:/tmp/rig-restore/ )"
[[ -f "$SRC/var/lib/alsa/asound.state" ]] && \
    echo "  sudo install -m 0644 /tmp/rig-restore/var/lib/alsa/asound.state /var/lib/alsa/asound.state   # or: demiurge-mixer restore"
[[ -d "$SRC/etc/wireplumber/wireplumber.conf.d" ]] && \
    echo "  sudo cp -n /tmp/rig-restore/etc/wireplumber/wireplumber.conf.d/* /etc/wireplumber/wireplumber.conf.d/   # -n: no overwrite; the installer owns these"
[[ -d "$SRC/etc/pipewire/pipewire.conf.d" ]] && \
    echo "  sudo cp -n /tmp/rig-restore/etc/pipewire/pipewire.conf.d/* /etc/pipewire/pipewire.conf.d/"
[[ -d "$SRC/etc/systemd/system" ]] && \
    echo "  # systemd units: prefer 'sudo setup-script/setup-demiurge.sh 6' then 7; compare against /tmp/rig-restore/etc/systemd/system"
[[ -d "$SRC/etc/sudoers.d" ]] && \
    echo "  # sudoers.d: review /tmp/rig-restore/etc/sudoers.d, then 'sudo visudo -cf <file>' BEFORE 'sudo install -m 0440 <file> /etc/sudoers.d/'"
[[ -d "$SRC/boot" ]] && \
    echo "  # boot/firmware: config.txt + cmdline.txt are yours (power=, usb_max_current_enable=1). Diff, edit by hand, then 'sync'."
echo "  sudo systemctl daemon-reload   # after any unit change; never restart the engine unasked"
exit 0
