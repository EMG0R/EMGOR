#!/usr/bin/env bash
# seal-image-file.sh <image.img> — seal a RAW IMAGE FILE offline (loop-mount), then shrink + xz + hash + os-list JSON.
# Bucket: sync. CURRENT golden-image path. The master card (gorpi7v3) stays untouched and self-updating:
# dd it to a .img (macOS), then run THIS in a Linux VM (Lima/Colima) or on gorcave. NEVER on a live rig, NEVER on a device.
#
# DESTRUCTIVE to the .img FILE (it edits it in place; work on the dd copy, never your only copy).
# Guards: CONFIRM_SEAL_IMAGE=yes, explicit regular-file path (no /dev/*, no block devices, no symlink into /dev), Linux, root.
# DRY_RUN=1 prints the plan only (no root, nothing touched).
#
# Applies the SAME item list as seal-image.sh (keep the two in step), against the mounted image:
#   ROOT = rootfs (p2 ext4), BOOT = bootfs (p1 vfat, /boot/firmware on the Pi).
# Differences from the in-place seal-image.sh, all because this is offline: no systemctl/journalctl/cloud-init binaries
# (their effects are done by deleting files), firstboot-regen is installed + enabled by hand-made symlink, the demiurge
# password hash is dropped (field set to "!") in the image's /etc/shadow, and sudoers keeps demiurge's own rules
# (only files naming ANOTHER real user are removed; the in-place seal removes demiurge's too).
#
# ORDER (sparse-friendly; a 62 GB card captured sparse must never be densified by a full-fs zero-fill):
#   1 loop-mount, seal (delete files), install+enable firstboot-regen, unmount
#   2 e2fsck -f root   3 resize2fs -M root (PRIMARY free-space/deleted-data removal)
#   4 only then: zero the tiny remaining free space (root+boot), unmount, e2fsck -f again
#   5 shrink the MBR partition to the fs size, truncate the image file   6 build-image.sh: xz -9, sha256 x2, os-list JSON
# Env: OUT_DIR IMG_NAME OS_URL ... are forwarded to build-image.sh. SEAL_KEEP_SUDOERS="file ...". DEMIURGE_USER (default demiurge).
set -euo pipefail

DRY="${DRY_RUN:-0}"
IMG="${1:-}"
DUSER="${DEMIURGE_USER:-demiurge}"
NEUTRAL_HOSTNAME="${NEUTRAL_HOSTNAME:-demiurge-image}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FB_SCRIPT_SRC="$HERE/firstboot-regen.sh"
FB_UNIT_SRC="$HERE/firstboot-regen.service"
BUILD="$HERE/build-image.sh"

say() { echo "seal-image-file: $*"; }
die() { echo "seal-image-file: $*" >&2; exit "${2:-2}"; }

# ---------------------------------------------------------------- guards -----
[ -n "$IMG" ] || die "usage: CONFIRM_SEAL_IMAGE=yes seal-image-file.sh <image.img>   (DRY_RUN=1 to preview)"
case "$IMG" in /dev/*) die "REFUSING: $IMG is under /dev. This script seals an image FILE only." 3;; esac
[ -e "$IMG" ] || die "not found: $IMG"
[ ! -b "$IMG" ] || die "REFUSING: $IMG is a block device." 3
[ -f "$IMG" ] || die "REFUSING: $IMG is not a regular file." 3
case "$(readlink -f -- "$IMG")" in /dev/*) die "REFUSING: $IMG resolves into /dev." 3;; esac
[ "$(uname -s)" = Linux ] || die "Linux only (needs loop devices + resize2fs). Use a Lima/Colima VM or gorcave, see README." 3
[ -f "$FB_SCRIPT_SRC" ] && [ -f "$FB_UNIT_SRC" ] || die "missing firstboot-regen.sh / .service next to this script"

if [ "$DRY" = 1 ]; then
  cat <<PLAN
[dry-run] seal-image-file plan for $IMG (user $DUSER)
  guards   : CONFIRM_SEAL_IMAGE=yes, regular file, Linux, root
  attach   : losetup -fP --show $IMG ; expect p1 vfat (boot) + p2 ext4 (root); mount both rw on temp dirs (ROOT, BOOT)
  scrub    : ROOT/etc/ssh/ssh_host_* ; etc/machine-id -> empty (+ var/lib/dbus/machine-id removed)
             wifi: etc/NetworkManager/system-connections/*, etc/wpa_supplicant/*.conf, DHCP leases
             ~$DUSER/.claude* .cod* .config/gh .ssh EMGOR_SKILLS .gitconfig .git-credentials (+ root's .ssh/.claude*)
             device.md in demiurge/local + node repo (+ node repo .git)
             bash+python history (user + root); var/log truncated + journal removed
             cloud-init: var/lib/cloud/*, instance-specific etc/cloud (base cloud.cfg + cloud.cfg.d kept), var/log/cloud-init*
             apt cache; var/lib/systemd/random-seed; hostname -> $NEUTRAL_HOSTNAME
             sudoers.d files naming a user other than $DUSER; $DUSER password hash dropped in etc/shadow (locked)
             BOOT: user-data + network-config -> stock commented, meta-data -> comment-only,
                   firstrun.sh / userconf* / demiurge-device.conf* removed
  install  : firstboot-regen.sh -> ROOT/usr/local/sbin/demiurge-firstboot-regen.sh ; .service -> ROOT/etc/systemd/system/ ;
             enable symlink in multi-user.target.wants ; ROOT/var/lib/demiurge-firstboot cleared
  assert   : grep mounted tree for users.noreply.github.com / ssh- key lines / \$6\$ \$y\$ hashes -> WARN only
  unmount  : (NO zero-fill yet; it would densify a sparse image)
  package  : e2fsck -f ; resize2fs -M (primary shrink) ; THEN zero tiny remainder on root+boot ; e2fsck -f ; sfdisk shrink p2 ; truncate ;
             build-image.sh (xz -9, sha256 raw+xz, os-list JSON, init_format cloudinit-rpi)
PLAN
  exit 0
fi

[ "${CONFIRM_SEAL_IMAGE:-}" = "yes" ] || die "REFUSING. Edits the image FILE in place and wipes identity/creds. Re-run with CONFIRM_SEAL_IMAGE=yes (DRY_RUN=1 to preview)." 3
[ "$(id -u)" -eq 0 ] || die "run as root (sudo)" 1
for t in losetup mount umount blkid e2fsck resize2fs dd sync truncate awk grep find sfdisk partx; do
  command -v "$t" >/dev/null 2>&1 || die "missing tool: $t"
done

# ------------------------------------------------------- attach + mount ------
LOOP=""; ROOT=""; BOOT=""
cleanup() {
  set +e
  [ -n "$BOOT" ] && { rm -f -- "$BOOT/.ZEROFILL"; mountpoint -q "$BOOT" && umount "$BOOT"; rmdir "$BOOT" 2>/dev/null; }
  [ -n "$ROOT" ] && { rm -f -- "$ROOT/.ZEROFILL"; mountpoint -q "$ROOT" && umount "$ROOT"; rmdir "$ROOT" 2>/dev/null; }
  [ -n "$LOOP" ] && losetup -d "$LOOP" 2>/dev/null
}
trap cleanup EXIT

LOOP="$(losetup -fP --show -- "$IMG")"
say "attached $IMG as $LOOP"
command -v udevadm >/dev/null 2>&1 && udevadm settle 2>/dev/null || true
for _ in 1 2 3 4 5 6 7 8 9 10; do [ -b "${LOOP}p1" ] && [ -b "${LOOP}p2" ] && break; sleep 1; done
[ -b "${LOOP}p1" ] && [ -b "${LOOP}p2" ] || die "partition nodes ${LOOP}p1/p2 did not appear (is this a Pi image with 2 partitions? VM needs loop partition support)"
t1="$(blkid -s TYPE -o value "${LOOP}p1")"; t2="$(blkid -s TYPE -o value "${LOOP}p2")"
[ "$t1" = vfat ] && [ "$t2" = ext4 ] || die "unexpected layout: p1=$t1 p2=$t2 (expected vfat + ext4)"

ROOT="$(mktemp -d /tmp/seal-root.XXXXXX)"; BOOT="$(mktemp -d /tmp/seal-boot.XXXXXX)"
mount -t ext4 "${LOOP}p2" "$ROOT"
mount -t vfat "${LOOP}p1" "$BOOT"
ROOT_REAL="$(readlink -f "$ROOT")"; BOOT_REAL="$(readlink -f "$BOOT")"
say "mounted: root=$ROOT boot=$BOOT"

# ------------------------------------------------------------ helpers --------
CLEARED=(); SKIPPED=()
note_cleared() { CLEARED+=("$1"); }
note_skipped() { SKIPPED+=("$1"); }
# Every destructive path must resolve (parent dir, symlinks followed) INSIDE the image mounts. Absolute symlinks in the
# image would otherwise resolve against the HOST. Refuse hard if anything escapes.
inimg() {
  local p="$1" par
  par="$(readlink -f -- "$(dirname -- "$p")")"
  case "$par/" in "$ROOT_REAL"/*|"$BOOT_REAL"/*) ;; *) die "BUG/ESCAPE: '$p' resolves outside the image mounts ($par)" 4;; esac
  [ "$p" != "$ROOT" ] && [ "$p" != "$BOOT" ] && [ "$p" != "$ROOT/" ] && [ "$p" != "$BOOT/" ] || die "refusing mount root" 4
}
rmf() {   # rmf <label> <paths...>
  local label="$1"; shift; local n=0 f
  for f in "$@"; do
    [ -e "$f" ] || [ -L "$f" ] || continue
    inimg "$f"; rm -f -- "$f"; n=$((n+1))
  done
  if [ "$n" -gt 0 ]; then note_cleared "$label ($n)"; else note_skipped "$label (none present)"; fi
}
rmr() {   # rmr <label> <path>  (rm -rf one explicit path)
  local label="$1" f="$2"
  if [ -e "$f" ] || [ -L "$f" ]; then inimg "$f"; rm -rf -- "$f"; note_cleared "$label"; else note_skipped "$label (absent)"; fi
}
stock() { local f="$1"; shift; inimg "$f"; printf '%s\n' "$@" > "$f"; }
shopt -s nullglob dotglob

# demiurge home from the IMAGE's passwd (not the host's)
H="$(awk -F: -v u="$DUSER" '$1==u{print $6}' "$ROOT/etc/passwd")"
[ -n "$H" ] || die "user '$DUSER' not found in the image's /etc/passwd. Wrong image?"
HI="$ROOT$H"
case "$H" in /|/root|"") die "bad home '$H'";; esac
[ -d "$HI" ] || die "home $H missing in image"
say "sealing image: user=$DUSER home=$H"

# ----------------------------------------------------------- 1. ssh host keys
rmf "ssh host keys" "$ROOT"/etc/ssh/ssh_host_*

# ----------------------------------------------------------- 2. machine-id ---
if [ -e "$ROOT/etc/machine-id" ]; then inimg "$ROOT/etc/machine-id"; : > "$ROOT/etc/machine-id"; note_cleared "etc/machine-id (truncated to empty)"
else : > "$ROOT/etc/machine-id"; note_cleared "etc/machine-id (created empty)"; fi
rmf "var/lib/dbus/machine-id" "$ROOT/var/lib/dbus/machine-id"

# ----------------------------------------------------------- 3. wifi + leases
rmf "NetworkManager connections (wifi PSKs)" "$ROOT"/etc/NetworkManager/system-connections/*
rmf "wpa_supplicant confs (wifi PSKs)" "$ROOT"/etc/wpa_supplicant/*.conf "$ROOT"/etc/wpa_supplicant*.conf
rmf "DHCP/NM leases" "$ROOT"/var/lib/NetworkManager/*.lease "$ROOT"/var/lib/NetworkManager/*.leases "$ROOT"/var/lib/dhcp/*.leases "$ROOT"/var/lib/dhcpcd/*.lease

# ----------------------------------------------------------- 4. user creds ---
n=0; for f in "$HI"/.claude* "$ROOT"/root/.claude*; do [ -e "$f" ] || [ -L "$f" ] || continue; inimg "$f"; rm -rf -- "$f"; n=$((n+1)); done
[ "$n" -gt 0 ] && note_cleared "~/.claude* ($n)" || note_skipped "~/.claude* (none)"
n=0; for f in "$HI"/.cod*; do [ -e "$f" ] || [ -L "$f" ] || continue; inimg "$f"; rm -rf -- "$f"; n=$((n+1)); done
[ "$n" -gt 0 ] && note_cleared "~/.cod* ($n)" || note_skipped "~/.cod* (none)"
rmr "~/.config/gh" "$HI/.config/gh"
rmr "~/EMGOR_SKILLS (private memory repo)" "$HI/EMGOR_SKILLS"
rmr "~/.ssh (all)" "$HI/.ssh"
rmr "/root/.ssh (all)" "$ROOT/root/.ssh"
rmf "git identity/credentials" "$HI/.gitconfig" "$HI/.git-credentials" "$HI/.config/git/config"

# ----------------------------------------------------------- 5. device.md ----
ident=()
[ -d "$ROOT/demiurge/local" ] && [ ! -L "$ROOT/demiurge/local" ] && ident+=("$ROOT/demiurge/local")
[ -d "$HI/_______DEMIURGE/demiurge/local" ] && [ ! -L "$HI/_______DEMIURGE/demiurge/local" ] && ident+=("$HI/_______DEMIURGE/demiurge/local")
for d in "${ident[@]}"; do
  rmf "device.md (${d#$ROOT})" "$d/device.md"
  [ -d "$d/.git" ] && rmr "node repo .git (${d#$ROOT})" "$d/.git"
done
# sweep for stray device.md files in the install trees (find does not follow symlinks)
for tree in "$ROOT/demiurge" "$HI/_______DEMIURGE"; do
  [ -d "$tree" ] && [ ! -L "$tree" ] || continue
  while IFS= read -r -d '' f; do inimg "$f"; rm -f -- "$f"; note_cleared "stray device.md (${f#$ROOT})"; done \
    < <(find "$tree" -xdev -type f -name device.md -print0 2>/dev/null)
done
[ "${#ident[@]}" -gt 0 ] || note_skipped "device.md (no local bucket found)"

# ----------------------------------------------------------- 6. history ------
rmf "bash/python history" "$HI/.bash_history" "$HI/.python_history" "$ROOT/root/.bash_history" "$ROOT/root/.python_history"

# ----------------------------------------------------------- 7. boot secrets -
stock "$BOOT/user-data" '#cloud-config' \
  '# Sealed golden image: stock empty cloud-config. Raspberry Pi Imager / the DEMIURGE FLASHER rewrites this file' \
  '# at flash time (user, password, ssh keys, hostname). No secrets live here.' \
  '#' '# Example (commented):' '# hostname: example' '# manage_etc_hosts: true'
note_cleared "boot user-data (stock empty cloud-config; no hash/keys)"
stock "$BOOT/network-config" \
  '# Sealed golden image: stock network-config. No wifi credentials.' \
  '# network:' '#   version: 2' '#   wifis:' '#     renderer: NetworkManager' '#     wlan0:' '#       dhcp4: true' '#       optional: true'
note_cleared "boot network-config (stock; no SSID/PSK)"
stock "$BOOT/meta-data" '# Sealed golden image: no instance-id. cloud-init treats the next flash as a FRESH instance.'
note_cleared "boot meta-data (instance-id cleared)"
rmf "boot firstrun.sh / userconf / device conf" "$BOOT/firstrun.sh" "$BOOT"/userconf* "$BOOT/demiurge-device.conf" "$BOOT/demiurge-device.conf.done"

# ----------------------------------------------------------- 8. cloud-init ---
# Base config (etc/cloud/cloud.cfg, cloud.cfg.d/*) is KEPT. Only per-instance state goes.
n=0; for f in "$ROOT"/var/lib/cloud/*; do [ -e "$f" ] || [ -L "$f" ] || continue; inimg "$f"; rm -rf -- "$f"; n=$((n+1)); done
[ "$n" -gt 0 ] && note_cleared "var/lib/cloud/* ($n)" || note_skipped "var/lib/cloud/* (empty)"
rmf "instance-specific etc/cloud artifacts" "$ROOT/etc/cloud/cloud.cfg.d/99-demiurge-hostname.cfg" "$ROOT"/etc/cloud/user-data* "$ROOT"/etc/cloud/*.user-data "$ROOT/etc/cloud/ds-identify.cfg"
rmf "cloud-init logs" "$ROOT"/var/log/cloud-init*

# ----------------------------------------------------------- 9. apt cache ---
rmf "apt cache" "$ROOT"/var/cache/apt/archives/*.deb "$ROOT"/var/cache/apt/pkgcache.bin "$ROOT"/var/cache/apt/srcpkgcache.bin

# ----------------------------------------------------------- 10. hostname ----
echo "$NEUTRAL_HOSTNAME" > "$ROOT/etc/hostname"
if grep -qE '^127\.0\.1\.1' "$ROOT/etc/hosts" 2>/dev/null; then sed -i -E "s/^127\.0\.1\.1.*/127.0.1.1\t$NEUTRAL_HOSTNAME/" "$ROOT/etc/hosts"
else printf '127.0.1.1\t%s\n' "$NEUTRAL_HOSTNAME" >> "$ROOT/etc/hosts"; fi
note_cleared "hostname -> $NEUTRAL_HOSTNAME"

# ----------------------------------------------------------- 11. sudoers ------
# Remove files whose rules name a real user OTHER than $DUSER (e.g. pi, a personal user). $DUSER's own rules stay.
for f in "$ROOT"/etc/sudoers.d/*; do
  [ -f "$f" ] || continue
  b="$(basename "$f")"; [ "$b" = README ] && continue
  keep=0; for k in ${SEAL_KEEP_SUDOERS:-}; do [ "$k" = "$b" ] && keep=1; done
  [ "$keep" = 1 ] && { note_skipped "sudoers.d/$b (kept via SEAL_KEEP_SUDOERS)"; continue; }
  if awk -v u="$DUSER" '{sub(/#.*/,"")} NF && $1 !~ /^%/ && $1 != u && $1 !~ /^(Defaults|Cmnd_Alias|User_Alias|Host_Alias|Runas_Alias|Cmd_Alias|@includedir|@include)/ {f=1} END{exit !f}' "$f" 2>/dev/null; then
    inimg "$f"; rm -f -- "$f"; note_cleared "sudoers.d/$b (names another user)"
  else note_skipped "sudoers.d/$b (kept: $DUSER/group/no user rule)"; fi
done

# ----------------------------------------------------------- 12. lock password
if [ -f "$ROOT/etc/shadow" ]; then
  tmp="$(mktemp)"; awk -F: -v OFS=: -v u="$DUSER" '$1==u{$2="!"}1' "$ROOT/etc/shadow" > "$tmp"
  cat "$tmp" > "$ROOT/etc/shadow"; rm -f "$tmp"      # cat > preserves the original file's owner and mode
  note_cleared "$DUSER password hash dropped (shadow field '!', locked; Imager sets it)"
fi

# ----------------------------------------------------------- 13. misc --------
rmf "systemd random-seed" "$ROOT/var/lib/systemd/random-seed"

# ----------------------------------------------------------- 14. logs --------
# Offline: no journalctl. Delete the journal directories and truncate/remove logs directly.
n=0; for d in "$ROOT"/var/log/journal/*; do [ -e "$d" ] || continue; inimg "$d"; rm -rf -- "$d"; n=$((n+1)); done
find "$ROOT/var/log" -xdev -type f \( -name '*.gz' -o -name '*.xz' -o -name '*.[0-9]' -o -name '*.old' \) -delete 2>/dev/null || true
find "$ROOT/var/log" -xdev -type f -exec truncate -s 0 {} + 2>/dev/null || true
note_cleared "var/log truncated, rotated + journal removed ($n journal dirs)"

# ----------------------------------------------------------- 15. firstboot-regen INSIDE the image
install -m 0755 "$FB_SCRIPT_SRC" "$ROOT/usr/local/sbin/demiurge-firstboot-regen.sh"
install -m 0644 "$FB_UNIT_SRC"   "$ROOT/etc/systemd/system/demiurge-firstboot-regen.service"
mkdir -p "$ROOT/etc/systemd/system/multi-user.target.wants"
ln -sfn /etc/systemd/system/demiurge-firstboot-regen.service "$ROOT/etc/systemd/system/multi-user.target.wants/demiurge-firstboot-regen.service"
rm -rf -- "$ROOT/var/lib/demiurge-firstboot"
note_cleared "firstboot-regen installed + ENABLED in the image (stamp cleared); master card untouched"

# ----------------------------------------------------------- 16. secret assert (WARN only)
SECRET_RE='users\.noreply\.github\.com|^[[:space:]-]*ssh-(rsa|ed25519|dss)|ecdsa-sha2-|\$(6|y)\$'
SECRETS_FOUND=0
scan_targets=("$BOOT")
for d in etc home root var/lib var/spool demiurge opt srv usr/local; do [ -d "$ROOT/$d" ] && scan_targets+=("$ROOT/$d"); done
for t in "${scan_targets[@]}"; do
  hits="$(grep -rIlE --exclude='*.so*' --exclude='*.pyc' -e "$SECRET_RE" "$t" 2>/dev/null || true)"
  # /etc/shadow legitimately holds system hashes; list it but it is the operator's call.
  if [ -n "$hits" ]; then
    SECRETS_FOUND=1
    echo "WARNING: secret-looking content under ${t#$ROOT} :" >&2; echo "$hits" | sed "s#^$ROOT#  - #; s#^$BOOT#  - (boot)#" >&2
  fi
done
[ "$SECRETS_FOUND" = 0 ] && note_cleared "secret assert: clean" || note_skipped "!!! secret-looking content STILL PRESENT (see WARNING above; review before shipping) !!!"

# ----------------------------------------------------------- unmount (seal done)
# NO zero-fill here: on a sparse image that would densify the whole (e.g. 62 GB) fs. Free space is removed by SHRINKING.
sync
umount "$BOOT"; umount "$ROOT"; rmdir "$BOOT" "$ROOT"; BOOT=""; ROOT=""
say "seal applied, unmounted; image still attached as $LOOP"

echo
echo "================ seal-image-file summary ================"
echo "CLEARED:"; for i in "${CLEARED[@]}"; do echo "  - $i"; done
echo "ALREADY ABSENT / SKIPPED:"; for i in "${SKIPPED[@]:-}"; do [ -n "$i" ] && echo "  - $i"; done

# ============================================ PACKAGING (sparse-friendly order) =
command -v dumpe2fs >/dev/null 2>&1 || die "dumpe2fs needed"
[ -f "$BUILD" ] || die "build-image.sh not found next to this script"
[ "$(sfdisk -l "$IMG" 2>/dev/null | awk '/Disklabel type/{print $3}')" = dos ] || die "partition shrink supports MBR (dos) images only (Pi OS default)"

# 2. fsck the root (required before resize2fs -M)
say "2. e2fsck -f root"
e2fsck -fy "${LOOP}p2" || [ $? -le 1 ]         # exit 0/1 = clean/corrected

# 3. resize2fs -M: shrink the fs to its minimum. This is the PRIMARY free-space + deleted-data removal.
say "3. resize2fs -M root (primary free-space removal)"
resize2fs -M "${LOOP}p2"

# 4. OPTIONAL small hardening, ONLY NOW (fs is near-minimal, so this stays cheap and does not densify a big sparse file):
#    zero the tiny remaining free space on root, and on the (small) boot fs. NEVER do this before the shrink above.
say "4. zero remaining free space (root is now minimal)"
ROOT="$(mktemp -d /tmp/seal-root.XXXXXX)"; BOOT="$(mktemp -d /tmp/seal-boot.XXXXXX)"
mount -t ext4 "${LOOP}p2" "$ROOT"; mount -t vfat "${LOOP}p1" "$BOOT"
for m in "$ROOT" "$BOOT"; do
  dd if=/dev/zero of="$m/.ZEROFILL" bs=4M status=none 2>/dev/null || true   # ENOSPC is the expected, successful end
  sync; rm -f -- "$m/.ZEROFILL" || true; sync
done
umount "$BOOT"; umount "$ROOT"; rmdir "$BOOT" "$ROOT"; BOOT=""; ROOT=""
e2fsck -fy "${LOOP}p2" || [ $? -le 1 ]

# 5. shrink the MBR partition to the fs size, then truncate the image FILE to the end of that partition.
blocks="$(dumpe2fs -h "${LOOP}p2" 2>/dev/null | awk -F: '/^Block count/{gsub(/ /,"",$2);print $2}')"
bsize="$(dumpe2fs -h "${LOOP}p2" 2>/dev/null | awk -F: '/^Block size/{gsub(/ /,"",$2);print $2}')"
start="$(partx -g -o START -n 2 "$LOOP" | tr -d ' ')"
sync; losetup -d "$LOOP"; LOOP=""
[ -n "$blocks" ] && [ -n "$bsize" ] && [ -n "$start" ] || die "could not read fs size / partition start"
new=$(( (blocks*bsize + 4*1024*1024 + 511) / 512 ))       # fs bytes + 4 MiB slack, in 512 B sectors
say "5. partition 2 -> $new sectors (start $start), truncating image"
echo ", $new" | sfdisk --no-reread --no-tell-kernel -N 2 "$IMG" >/dev/null
truncate -s $(( (start + new) * 512 )) "$IMG"
say "image now $(stat -c %s "$IMG") bytes"

# 6. xz -9 + sha256 (uncompressed + xz) + os-list JSON via build-image.sh.
#    The shrink is already done above. If pishrink exists, run it too ONLY to add its first-boot auto-expand
#    (on an already-minimal fs it is quick); otherwise skip it (then NO auto-expand: install pishrink for the real build).
PISHRINK="${PISHRINK:-$(command -v pishrink.sh || command -v pishrink || true)}"
if [ -n "$PISHRINK" ] && [ -x "$PISHRINK" ]; then
  say "6. build-image.sh (pishrink adds auto-expand)"; PISHRINK="$PISHRINK" bash "$BUILD" "$IMG"
else
  say "6. build-image.sh with SKIP_PISHRINK=1 (no auto-expand; install pishrink for the real build)"; SKIP_PISHRINK=1 bash "$BUILD" "$IMG"
fi
