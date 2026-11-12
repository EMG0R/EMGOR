#!/usr/bin/env bash
# seal-image.sh — prepare a fully-installed Demiurge node to be captured as the GOLDEN IMAGE.
# Bucket: sync. Run ON the golden node (gorpi7v3) as root, right before capture. NEVER on a live rig.
#
# DESTRUCTIVE: wipes node identity + credentials (ssh host keys, machine-id, wifi, Claude/gh creds,
# device.md, history, logs). Refuses to run unless CONFIRM_SEAL_THIS_NODE=yes.
# DRY_RUN=1 prints every action and changes nothing (no confirm needed, no root needed).
# Idempotent: re-running on an already-sealed node is safe.
#
# After it finishes: `sudo poweroff` and capture the card. DO NOT BOOT the node again before capture,
# or firstboot-regen fires and consumes itself.
set -euo pipefail

DRY="${DRY_RUN:-0}"
DUSER="${DEMIURGE_USER:-demiurge}"
NEUTRAL_HOSTNAME="${NEUTRAL_HOSTNAME:-demiurge-image}"
REFUSE_HOSTS="${SEAL_REFUSE_HOSTS:-gorpi7v2 demiurge twink}"   # never seal a rig we know is live
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FB_SCRIPT_SRC="$HERE/firstboot-regen.sh"
FB_UNIT_SRC="$HERE/firstboot-regen.service"
FB_SCRIPT_DST=/usr/local/sbin/demiurge-firstboot-regen.sh
FB_UNIT_DST=/etc/systemd/system/demiurge-firstboot-regen.service
FB_STATE=/var/lib/demiurge-firstboot

CLEARED=(); SKIPPED=()
say()  { echo "seal: $*"; }
act()  { if [ "$DRY" = 1 ]; then echo "[dry-run] $*"; else "$@"; fi; }
note_cleared() { CLEARED+=("$1"); }
note_skipped() { SKIPPED+=("$1"); }

# ------------------------------------------------------------------ guards ---
if [ "$DRY" != 1 ]; then
  [ "${CONFIRM_SEAL_THIS_NODE:-}" = "yes" ] || {
    echo "REFUSING. This wipes this node's identity and credentials (ssh host keys, machine-id, wifi," >&2
    echo "Claude/gh creds, device.md, history, logs). Run on the golden node only, as:" >&2
    echo "  sudo CONFIRM_SEAL_THIS_NODE=yes bash $0     (DRY_RUN=1 to preview)" >&2
    exit 3
  }
  [ "$(id -u)" -eq 0 ] || { echo "run as root (sudo)" >&2; exit 1; }
  cur_host="$(hostname)"
  for h in $REFUSE_HOSTS; do
    [ "$cur_host" = "$h" ] && { echo "REFUSING: hostname '$cur_host' is on the never-seal list ($REFUSE_HOSTS). Override list with SEAL_REFUSE_HOSTS=" >&2; exit 3; }
  done
fi
id "$DUSER" >/dev/null 2>&1 || { [ "$DRY" = 1 ] || { echo "user '$DUSER' not found. The golden image bakes a fixed user (default demiurge)." >&2; exit 2; }; }
H="$(getent passwd "$DUSER" 2>/dev/null | cut -d: -f6 || true)"; H="${H:-/home/$DUSER}"
case "$H" in ""|"/"|"/root") echo "bad home dir '$H' for $DUSER" >&2; exit 2;; esac
if [ ! -f "$FB_SCRIPT_SRC" ] || [ ! -f "$FB_UNIT_SRC" ]; then echo "missing $FB_SCRIPT_SRC or $FB_UNIT_SRC next to this script" >&2; exit 2; fi

say "sealing node '$(hostname)' (user $DUSER, home $H) dry=$DRY"
shopt -s nullglob

# rm_files <label> <path...> : remove existing files (globs already expanded by caller)
rm_files() {
  local label="$1"; shift; local n=0 f
  for f in "$@"; do
    [ -e "$f" ] || [ -L "$f" ] || continue
    act rm -f -- "$f"; n=$((n+1))
  done
  if [ "$n" -gt 0 ]; then note_cleared "$label ($n)"; else note_skipped "$label (none present)"; fi
}
rm_tree() {   # explicit directory only, never a glob
  local label="$1" d="$2"
  case "$d" in ""|"/"|"/home"|"/etc"|"/var"|"/root"|"$H") echo "refusing rm_tree '$d'" >&2; exit 2;; esac
  if [ -d "$d" ]; then act rm -rf -- "$d"; note_cleared "$label ($d)"; else note_skipped "$label (absent)"; fi
}

# ------------------------------------------------------- 1. ssh host keys ----
rm_files "ssh host keys /etc/ssh/ssh_host_*" /etc/ssh/ssh_host_*

# ------------------------------------------- 2. machine-id (truncate, keep) ---
if [ -e /etc/machine-id ]; then
  act truncate -s 0 /etc/machine-id; note_cleared "/etc/machine-id (truncated to empty)"
else
  note_skipped "/etc/machine-id (absent)"
fi
rm_files "/var/lib/dbus/machine-id" /var/lib/dbus/machine-id

# --------------------------------------- 3. network credentials (wifi etc.) ---
rm_files "NetworkManager connections (wifi PSKs)" /etc/NetworkManager/system-connections/*
rm_files "wpa_supplicant confs (wifi PSKs)" /etc/wpa_supplicant/*.conf /etc/wpa_supplicant/wpa_supplicant*.conf /etc/wpa_supplicant*.conf
rm_files "DHCP/NM leases" /var/lib/NetworkManager/*.lease /var/lib/NetworkManager/*.leases /var/lib/dhcp/*.leases /var/lib/dhcpcd/*.lease

# --------------------------------------------- 4. user credentials + keys ------
# ~/.claude* glob: .claude/ (credentials, history, projects) and .claude.json (oauth + trust state)
for f in "$H"/.claude*; do
  [ -e "$f" ] || [ -L "$f" ] || continue
  act rm -rf -- "$f"; note_cleared "claude state $f"
done
[ -e "$H/.claude.json" ] || [ -d "$H/.claude" ] || true
rm_tree  "gh auth" "$H/.config/gh"
# ~/.cod* glob: .cod-bus.conf and any other cod state
for f in "$H"/.cod*; do
  [ -e "$f" ] || [ -L "$f" ] || continue
  act rm -rf -- "$f"; note_cleared "cod state $f"
done
# PRIVATE memory repo must never ship
rm_tree  "EMGOR_SKILLS (private memory repo)" "$H/EMGOR_SKILLS"
# ALL of ~/.ssh: private keys, id_*, authorized_keys, known_hosts, config
rm_tree  "~/.ssh (all)" "$H/.ssh"
# claude trust prompts will re-appear on the new node's first manual login (see CLAUDE_RC_AUTONOMOUS.md piece 6)

# ------------------------------------------------ 5. node identity (device.md) -
ident_dirs=()
[ -d /demiurge/local ] && ident_dirs+=(/demiurge/local)
for d in "$H"/_______DEMIURGE/demiurge/local; do [ -d "$d" ] && ident_dirs+=("$d"); done
if [ "${#ident_dirs[@]}" -eq 0 ]; then note_skipped "device.md (no local bucket found)"; fi
for d in "${ident_dirs[@]}"; do
  rm_files "device.md ($d)" "$d/device.md"
  # node repo history contains the old device.md and any mesh remotes: drop it, firstboot re-inits.
  if [ -d "$d/.git" ]; then act rm -rf -- "$d/.git"; note_cleared "node repo .git ($d/.git)"; fi
done

# ------------------------------------------------------------- 6. history ----
rm_files "shell/python history" "$H/.bash_history" "$H/.python_history" /root/.bash_history /root/.python_history

# ------------------------------------------------ 7. boot-partition cloud-init secrets
# The flasher / Imager wrote SECRETS here: user-data (login password hash + EMGOR's ssh public key),
# network-config (wifi SSID/PSK), meta-data (instance-id). Overwrite all three with STOCK content; no hash,
# no keys, no wifi. Runs BEFORE the zero-free-space step so the freed blocks are zeroed too.
BOOTDIRS=(); _seen=""
for bp in /boot/firmware /boot; do
  [ -d "$bp" ] || continue
  rp="$(readlink -f "$bp")"; case " $_seen " in *" $rp "*) continue;; esac
  _seen="$_seen $rp"; BOOTDIRS+=("$bp")
done
write_stock() {   # write_stock <file> <content...>  (only if the file exists or its dir is a boot dir)
  local f="$1"; shift
  if [ "$DRY" = 1 ]; then echo "[dry-run] overwrite $f with stock content"; else printf '%s\n' "$@" > "$f"; fi
}
for bp in "${BOOTDIRS[@]}"; do
  write_stock "$bp/user-data" '#cloud-config' \
    '# Sealed golden image: stock empty cloud-config. Raspberry Pi Imager / the DEMIURGE FLASHER rewrites this file' \
    '# at flash time (user, password, ssh keys, hostname). No secrets live here.' \
    '#' \
    '# Example (commented):' \
    '# hostname: demiurge-example' \
    '# manage_etc_hosts: true'
  note_cleared "$bp/user-data (stock empty cloud-config; no hash/keys)"
  write_stock "$bp/network-config" \
    '# Sealed golden image: stock network-config. No wifi credentials.' \
    '# network:' \
    '#   version: 2' \
    '#   wifis:' \
    '#     renderer: NetworkManager' \
    '#     wlan0:' \
    '#       dhcp4: true' \
    '#       optional: true'
  note_cleared "$bp/network-config (stock; no SSID/PSK)"
  write_stock "$bp/meta-data" '# Sealed golden image: no instance-id. cloud-init treats the next flash as a FRESH instance.'
  note_cleared "$bp/meta-data (instance-id cleared)"
  rm_files "$bp firstrun.sh / userconf / device conf" "$bp/firstrun.sh" "$bp"/userconf* "$bp/demiurge-device.conf" "$bp/demiurge-device.conf.done"
done
[ "${#BOOTDIRS[@]}" -gt 0 ] || note_skipped "boot partition (no /boot/firmware or /boot)"

# --------------------------------------------------------- 8. cloud-init ------
# Full state reset. Base config (/etc/cloud/cloud.cfg, cloud.cfg.d/*) is KEPT: cloud-init needs it to function.
if command -v cloud-init >/dev/null 2>&1; then
  if [ "$DRY" = 1 ]; then echo "[dry-run] cloud-init clean --logs --seed"; else cloud-init clean --logs --seed || say "cloud-init clean returned non-zero (continuing)"; fi
  note_cleared "cloud-init state (cloud-init clean --logs --seed)"
else
  note_skipped "cloud-init binary (not installed)"
fi
# Belt and braces: everything under /var/lib/cloud (instances, seed, data, instance symlink, user-data.txt).
_n=0
for f in /var/lib/cloud/* /var/lib/cloud/.[!.]*; do
  [ -e "$f" ] || [ -L "$f" ] || continue
  act rm -rf -- "$f"; _n=$((_n+1))
done
[ "$_n" -gt 0 ] && note_cleared "/var/lib/cloud/* ($_n entries)" || note_skipped "/var/lib/cloud/* (empty)"
# Instance-specific / cached user-data under /etc/cloud only (NOT cloud.cfg or cloud.cfg.d base config).
rm_files "instance-specific /etc/cloud artifacts" /etc/cloud/cloud.cfg.d/99-demiurge-hostname.cfg /etc/cloud/user-data* /etc/cloud/*.user-data /etc/cloud/ds-identify.cfg

# Assert: no secrets remain on the boot partition. WARN only, so the operator can fix before capture.
SECRET_RE='users\.noreply\.github\.com|^[[:space:]-]*ssh-(rsa|ed25519|dss)|ecdsa-sha2-|\$(6|y)\$'
SECRETS_FOUND=0
for bp in "${BOOTDIRS[@]}"; do
  hits="$(grep -rIlE "$SECRET_RE" "$bp" 2>/dev/null || true)"
  if [ "$DRY" = 1 ]; then echo "[dry-run] assert $bp has no ssh keys / github-noreply / password hashes"
  elif [ -n "$hits" ]; then
    SECRETS_FOUND=1
    echo "WARNING: secret-looking content still under $bp (ssh key / users.noreply.github.com / \$6\$|\$y\$ hash):" >&2
    echo "$hits" | sed 's/^/  - /' >&2
  fi
done
[ "$SECRETS_FOUND" = 0 ] && note_cleared "boot-partition secret assert: clean" || note_skipped "!!! boot-partition secrets STILL PRESENT (see WARNING above) !!!"

# ----------------------------------------------------------------- 9. apt ----
act apt-get clean; note_cleared "apt cache (apt-get clean)"

# ----------------------------------------------------------- 10. hostname ----
if [ "$DRY" = 1 ]; then echo "[dry-run] hostname -> $NEUTRAL_HOSTNAME (/etc/hostname, /etc/hosts 127.0.1.1)"
else
  echo "$NEUTRAL_HOSTNAME" > /etc/hostname
  if grep -qE '^127\.0\.1\.1' /etc/hosts 2>/dev/null; then sed -i -E "s/^127\.0\.1\.1.*/127.0.1.1\t$NEUTRAL_HOSTNAME/" /etc/hosts
  else printf '127.0.1.1\t%s\n' "$NEUTRAL_HOSTNAME" >> /etc/hosts; fi
fi
note_cleared "hostname -> $NEUTRAL_HOSTNAME (takes effect at next boot)"

# ---------------------------------------------- 11. lock baked user password ---
# Imager OS customisation sets the real password at first boot. Until then the baked account has none.
if id "$DUSER" >/dev/null 2>&1; then act passwd -l "$DUSER" >/dev/null; note_cleared "$DUSER password (locked; Imager sets it)"; fi

# ------------------------------------- 11b. sudoers rules naming a real user -
# Removes /etc/sudoers.d files that grant a rule to a user OTHER than the generic
# distro user `demiurge`. A rule naming `demiurge` is KEPT — it is public-safe and
# the rig needs it (e.g. demiurge-patch runs `sudo -n systemctl restart demiurge`).
# A file naming a real personal user (e.g. "gorpi7v2 ALL=..." or "pi ...") is removed.
# Keep specific files regardless with SEAL_KEEP_SUDOERS="file1 file2". README always kept.
for f in /etc/sudoers.d/*; do
  [ -f "$f" ] || continue
  b="$(basename "$f")"; [ "$b" = README ] && continue
  keep=0; for k in ${SEAL_KEEP_SUDOERS:-}; do [ "$k" = "$b" ] && keep=1; done
  [ "$keep" = 1 ] && { note_skipped "sudoers.d/$b (kept via SEAL_KEEP_SUDOERS)"; continue; }
  if awk '{sub(/#.*/,"")} NF && $1 !~ /^%/ && $1 != "demiurge" && $1 !~ /^(Defaults|Cmnd_Alias|User_Alias|Host_Alias|Runas_Alias|Cmd_Alias|@includedir|@include)/ {f=1} END{exit !f}' "$f" 2>/dev/null; then
    act rm -f -- "$f"; note_cleared "sudoers.d/$b (names a non-generic user)"
  else note_skipped "sudoers.d/$b (no non-demiurge user rule — kept)"; fi
done
# Raspberry Pi userconf (password hash written by Imager / userconf-pi)
rm_files "Pi userconf password" /boot/firmware/userconf* /boot/userconf*
# (demiurge password already locked in step 11)

# ----------------------------------------------- 12. misc per-boot state ------
rm_files "systemd random-seed" /var/lib/systemd/random-seed
rm_files "root/user tmux sockets (stale)" /tmp/tmux-*/default

# ------------------------------------------------------------- 14. logs -------
# Before zero-fill, so earlier steps' own logging is wiped too. Truncate in place (services keep their fds), drop rotated copies.
if [ "$DRY" = 1 ]; then echo "[dry-run] journalctl --rotate; vacuum; truncate /var/log/*; rm rotated + /var/log/journal/*"
else
  journalctl --rotate >/dev/null 2>&1 || true
  journalctl --vacuum-time=1s >/dev/null 2>&1 || true
  for d in /var/log/journal/*; do [ -d "$d" ] && rm -rf -- "$d"; done
  find /var/log -type f \( -name '*.gz' -o -name '*.xz' -o -name '*.[0-9]' -o -name '*.old' \) -delete 2>/dev/null || true
  find /var/log -type f -exec truncate -s 0 {} + 2>/dev/null || true
  sync
fi
note_cleared "logs (/var/log truncated, rotated + journal removed)"

# ------------------------------------------------ 15. ZERO FREE SPACE (final fs step)
# A dd+xz capture also captures unallocated + deleted blocks. Fill every rw real fs with zeros, then delete the
# filler: deleted data becomes unrecoverable and xz compresses the zeros to nothing. "No space left" from dd is
# SUCCESS (the expected end). Never aborts the seal. Slow: ~ free-space / disk speed. Services may complain while
# / is momentarily full; seal on a quiesced node (stop demiurge/audio first).
ZERO_MNTS=()
if command -v findmnt >/dev/null 2>&1; then
  while read -r tgt fst opts; do
    case "$fst" in ext4|ext3|ext2|vfat|f2fs|xfs|btrfs) ;; *) continue;; esac
    case ",$opts," in *,ro,*) continue;; esac
    [ -d "$tgt" ] || continue
    ZERO_MNTS+=("$tgt")
  done < <(findmnt -rno TARGET,FSTYPE,OPTIONS 2>/dev/null)
fi
[ "${#ZERO_MNTS[@]}" -gt 0 ] || ZERO_MNTS=(/ /boot/firmware)
zero_cleanup() { for m in "${ZERO_MNTS[@]}"; do rm -f -- "${m%/}/.ZEROFILL" 2>/dev/null || true; done; }
trap zero_cleanup EXIT
for m in "${ZERO_MNTS[@]}"; do
  zf="${m%/}/.ZEROFILL"
  if [ "$DRY" = 1 ]; then
    echo "[dry-run] zero free space on $m: dd if=/dev/zero of=$zf bs=4M status=none || true; sync; rm -f $zf; sync"
  else
    say "zeroing free space on $m (can take minutes)"
    dd if=/dev/zero of="$zf" bs=4M status=none 2>/dev/null || true   # ENOSPC is the expected, successful end
    sync
    rm -f -- "$zf" || true
    sync
  fi
  note_cleared "free space zeroed on $m"
done

# --------------------------------------------------- 13. install + enable regen
if [ "$DRY" = 1 ]; then
  echo "[dry-run] install $FB_SCRIPT_SRC -> $FB_SCRIPT_DST ; $FB_UNIT_SRC -> $FB_UNIT_DST"
  echo "[dry-run] rm -rf $FB_STATE ; systemctl daemon-reload ; systemctl enable demiurge-firstboot-regen.service"
else
  install -m 0755 "$FB_SCRIPT_SRC" "$FB_SCRIPT_DST"
  install -m 0644 "$FB_UNIT_SRC"   "$FB_UNIT_DST"
  rm -rf -- "$FB_STATE"
  systemctl daemon-reload
  systemctl enable demiurge-firstboot-regen.service
fi
note_cleared "firstboot-regen installed + ENABLED (stamp $FB_STATE removed)"

# ------------------------------------------------------------- summary -------
echo
echo "================ seal summary (dry=$DRY) ================"
echo "CLEARED:"; for i in "${CLEARED[@]}"; do echo "  - $i"; done
echo "ALREADY ABSENT / SKIPPED:"; for i in "${SKIPPED[@]:-}"; do [ -n "$i" ] && echo "  - $i"; done
echo "NOT touched: /home/$DUSER repo + ~/.npm-global + systemd --user units + linger (the install), tailscale/other service state."
echo "NEXT: sudo poweroff, then capture the card. Do NOT boot it again before capture."
