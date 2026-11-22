#!/bin/bash
# HYPERTRUMPET V2: flash the Bela Gem image to SD, then stage the hyperTrumpet boot/USB stack on it.
# Usage: sudo bash flash_sd.sh disk8
set -euo pipefail
DISK="${1:?usage: sudo bash flash_sd.sh diskN}"
IMG="/Users/emgor/Downloads/bela-gem.img.xz"
HERE="$(cd "$(dirname "$0")" && pwd)"

[ "$(id -u)" = 0 ] || { echo "run with sudo"; exit 1; }
[ -f "$IMG" ] || { echo "missing $IMG"; exit 1; }
INFO="$(diskutil info "/dev/$DISK")"
echo "$INFO" | grep -q "Device Location:.*External" || { echo "/dev/$DISK is not external, refusing"; exit 1; }
echo "$INFO" | grep -q "Removable Media:.*Removable" || { echo "/dev/$DISK is not removable, refusing"; exit 1; }
echo "$INFO" | grep -E "Media Name|Disk Size"
[ "${2:-}" = YES ] || { echo "re-run with YES as 2nd arg to ERASE /dev/$DISK"; exit 1; }

diskutil unmountDisk "/dev/$DISK"
xz -dc "$IMG" | dd of="/dev/r$DISK" bs=4m status=progress
sync
echo "flash done; waiting for partitions to mount..."
diskutil mountDisk "/dev/$DISK" >/dev/null 2>&1 || true
sleep 5

# Stage the stack on the first FAT partition (the ext4 root can't be written from macOS).
FATVOL="$(diskutil list "/dev/$DISK" | awk '/DOS_FAT|FAT_32|Windows_FAT/ {print $NF; exit}')"
MNT=""
[ -n "$FATVOL" ] && MNT="$(diskutil info "/dev/$FATVOL" | awk -F': *' '/Mount Point/ {print $2}')"
if [ -n "$MNT" ] && [ -d "$MNT" ]; then
  rsync -a --exclude flash_sd.sh "$HERE/" "$MNT/hypertrumpet/"
  echo "staged stack at $MNT/hypertrumpet/"
else
  echo "no FAT partition mounted; stage over ssh after first boot instead (README_BOOT_USB.md)"
fi
diskutil eject "/dev/$DISK"
echo "DONE. Insert card in the Gem Stereo, boot, then from the Mac: bash $HERE/install.sh"
