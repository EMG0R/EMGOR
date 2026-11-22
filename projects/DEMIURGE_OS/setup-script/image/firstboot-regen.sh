#!/usr/bin/env bash
# demiurge-firstboot-regen.sh — runs ONCE on the first boot of a flashed golden image (systemd oneshot).
# Regenerates ssh host keys + machine-id, sets hostname = DEVICE (no prefix), seeds device.md via
# init-node-repo.sh, then writes a stamp and disables its own unit. Failure leaves it armed for next boot.
#
# Does NOT touch the user/password/wifi/ssh settings: those belong to Raspberry Pi Imager (cloud-init /
# firstrun.sh). We run after them and only read the hostname they set.
#
# DEVICE precedence: DEMIURGE_DEVICE env > boot-partition conf (demiurge-device.conf) > hostname Imager/flasher set
# (not raspberrypi; used verbatim, sanitised) > "demiurge-" + last 4 hex of MAC (wlan0, eth0, first non-lo).
# Never a real node name (neptr etc.).
# Boot conf (FAT, editable on any computer): lines KEY=value for DEVICE, ROLE, HARDWARE, PAIR.
# DRY_RUN=1 prints actions only (needs no root).
set -euo pipefail

DRY="${DRY_RUN:-0}"
DUSER="${DEMIURGE_USER:-demiurge}"
STATE=/var/lib/demiurge-firstboot
STAMP="$STATE/done"
LOG=/var/log/demiurge-firstboot.log
UNIT=demiurge-firstboot-regen.service
NEUTRAL_RE='^(raspberrypi|localhost|demiurge|demiurge-image|demiurge-golden)$'

say() { echo "$(date '+%F %T') firstboot-regen: $*" | tee -a "$LOG" 2>/dev/null || echo "firstboot-regen: $*"; }
act() { if [ "$DRY" = 1 ]; then echo "[dry-run] $*"; else "$@"; fi; }

[ "$DRY" = 1 ] || { [ "$(id -u)" -eq 0 ] || { echo "must run as root" >&2; exit 1; }; mkdir -p "$STATE"; }
[ "$DRY" = 1 ] || [ ! -f "$STAMP" ] || { say "stamp present, nothing to do"; exit 0; }

# --- 0. let cloud-init finish applying Imager's user/hostname/wifi (bounded wait) -------------
if command -v cloud-init >/dev/null 2>&1; then
  say "waiting (<=120s) for cloud-init"
  [ "$DRY" = 1 ] || timeout 120 cloud-init status --wait >/dev/null 2>&1 || say "cloud-init wait ended (timeout/err); continuing"
fi

# --- 1. ssh host keys (cloud-init usually makes them; ssh-keygen -A fills any gap, idempotent) ---
say "ssh host keys"
act ssh-keygen -A

# --- 2. machine-id ----------------------------------------------------------------------------
say "machine-id"
if [ "$DRY" = 1 ]; then echo "[dry-run] regenerate /etc/machine-id if empty/uninitialized; link /var/lib/dbus/machine-id"
else
  mid="$(tr -d '[:space:]' < /etc/machine-id 2>/dev/null || true)"
  if [ -z "$mid" ] || [ "$mid" = "uninitialized" ]; then
    rm -f /etc/machine-id; systemd-machine-id-setup >>"$LOG" 2>&1
  fi
  mkdir -p /var/lib/dbus
  ln -sf /etc/machine-id /var/lib/dbus/machine-id
fi

# --- 3. choose DEVICE / ROLE / HARDWARE / PAIR ------------------------------------------------
conf=""; for c in /boot/firmware/demiurge-device.conf /boot/demiurge-device.conf; do [ -f "$c" ] && { conf="$c"; break; }; done
getconf() { [ -n "$conf" ] && grep -E "^$1=" "$conf" | tail -n1 | cut -d= -f2- | tr -d '\r"'"'"' ' || true; }
sanitise() { echo "$1" | tr 'A-Z' 'a-z' | sed -E 's/[^a-z0-9]+/-/g; s/^-+//; s/-+$//'; }

DEVICE="$(sanitise "${DEMIURGE_DEVICE:-}")"
[ -n "$DEVICE" ] || DEVICE="$(sanitise "$(getconf DEVICE)")"
ROLE="$(getconf ROLE)"; HARDWARE="$(getconf HARDWARE)"; PAIR="$(sanitise "$(getconf PAIR)")"
if [ -z "$DEVICE" ]; then
  cur="$(hostname 2>/dev/null || echo raspberrypi)"
  if ! [[ "$cur" =~ $NEUTRAL_RE ]]; then DEVICE="$(sanitise "$cur")"; fi
fi
if [ -z "$DEVICE" ] || [ "$DEVICE" = demiurge ]; then
  DEVICE=""
  for i in wlan0 eth0 $(ls /sys/class/net 2>/dev/null | grep -v '^lo$' || true); do
    a="$(tr -d ':\n' < "/sys/class/net/$i/address" 2>/dev/null | tr 'A-Z' 'a-z' || true)"
    if [ "${#a}" -ge 4 ] && [ "$a" != "000000000000" ]; then DEVICE="${a: -4}"; break; fi
  done
  [ -n "$DEVICE" ] || DEVICE="$(tr -d '[:space:]' < /etc/machine-id 2>/dev/null | cut -c1-4)"
  [ -n "$DEVICE" ] || DEVICE=0000
  DEVICE="demiurge-$DEVICE"   # unnamed default still reads as a Demiurge
fi
[[ "$DEVICE" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]] || { say "bad DEVICE '$DEVICE'"; exit 2; }
ROLE="${ROLE:-instrument}"
if [ -z "$HARDWARE" ]; then
  kb="$(awk '/MemTotal/{print $2}' /proc/meminfo 2>/dev/null || echo 0)"
  if   [ "$kb" -gt 12000000 ]; then HARDWARE=pi5-16gb
  elif [ "$kb" -gt 6000000 ];  then HARDWARE=pi5-8gb
  else HARDWARE=pi5-4gb; fi
fi
say "identity: DEVICE=$DEVICE ROLE=$ROLE HARDWARE=$HARDWARE PAIR=${PAIR:-"-"} (conf: ${conf:-none})"

# --- 4. hostname = DEVICE ------------------------------------------------------------
HN="$DEVICE"
say "hostname -> $HN"
if [ "$DRY" = 1 ]; then echo "[dry-run] write /etc/hostname, /etc/hosts, hostname $HN, preserve_hostname drop-in, restart avahi"
else
  echo "$HN" > /etc/hostname
  hostname "$HN"
  if grep -qE '^127\.0\.1\.1' /etc/hosts; then sed -i -E "s/^127\.0\.1\.1.*/127.0.1.1\t$HN/" /etc/hosts
  else printf '127.0.1.1\t%s\n' "$HN" >> /etc/hosts; fi
  # cloud-init's update_hostname runs every boot; stop it reverting to the Imager name.
  if [ -d /etc/cloud/cloud.cfg.d ]; then echo "preserve_hostname: true" > /etc/cloud/cloud.cfg.d/99-demiurge-hostname.cfg; fi
  systemctl is-active --quiet avahi-daemon && systemctl restart avahi-daemon || true
fi

# --- 5. device.md via init-node-repo.sh (never overwrites an existing device.md) ---------------
INIT=""
for p in /demiurge/current/setup-script/node-repo/init-node-repo.sh \
         "/home/$DUSER/_______DEMIURGE/setup-script/node-repo/init-node-repo.sh"; do
  [ -f "$p" ] && { INIT="$p"; break; }
done
if [ -z "$INIT" ]; then say "WARNING: init-node-repo.sh not found; device.md NOT seeded"
else
  DIRARG=(); [ -d /demiurge/local ] && DIRARG=(DIR=/demiurge/local)
  say "seed device.md via $INIT ${DIRARG[*]:-}"
  if [ "$DRY" = 1 ]; then echo "[dry-run] runuser -u $DUSER -- env DEVICE=$DEVICE ROLE=$ROLE HARDWARE=$HARDWARE PAIR=$PAIR ${DIRARG[*]:-} bash $INIT"
  else
    runuser -u "$DUSER" -- env DEVICE="$DEVICE" ROLE="$ROLE" HARDWARE="$HARDWARE" PAIR="$PAIR" "${DIRARG[@]}" bash "$INIT" >>"$LOG" 2>&1 \
      || { say "init-node-repo failed (see $LOG); staying armed for next boot"; exit 1; }
  fi
fi

# --- 6. restart ssh so it serves the (re)generated host keys -----------------------------------
[ "$DRY" = 1 ] || { systemctl restart ssh.service >>"$LOG" 2>&1 || systemctl restart sshd.service >>"$LOG" 2>&1 || say "ssh restart failed (non-fatal)"; }

# --- 7. consume the boot conf, stamp, self-disable --------------------------------------------
if [ "$DRY" = 1 ]; then echo "[dry-run] rename $conf -> .done; write $STAMP; systemctl disable $UNIT"
else
  [ -n "$conf" ] && mv -f "$conf" "$conf.done" || true
  date > "$STAMP"
  systemctl disable "$UNIT" >>"$LOG" 2>&1 || true
  sync
fi
say "FINAL: DEVICE=$DEVICE hostname=$HN (device.md seeded with DEVICE=$DEVICE)"
say "done: $HN. Log in as $DUSER with the password set in Imager. Run 'claude' /login + 'gh auth login' once."
