#!/usr/bin/env bash
# install.sh — fresh Pi OS Lite (64-bit, Trixie) -> Demiurge. One entry point.
#
# THIS WRAPS, AND DOES NOT REPLACE, setup-script/setup-demiurge.sh.
# The 11 phases live there and are the canonical installer; this file only adds
# what a flashed image needs around them: hostname, device.md, phase ordering
# across the Phase 8 reboot. Never copy phase logic into this file.
#
#   DEVICE=twink ./install.sh                         # hostname twink
#   DEVICE=neuralgrid-a PAIR=neuralgrid-b ROLE=navigator HARDWARE=pi5x2 ./install.sh
#   DRY_RUN=1 DEVICE=twink ./install.sh               # print the plan, change nothing
#
# Flow (re-run after each reboot; safe to re-run any time):
#   1. hostname -> $DEVICE                (only if DEVICE is given; skipped if already set)
#   2. device.md seeded if missing        (never overwritten; via node-repo/init-node-repo.sh)
#   3. not yet booted with Phase 8 cmdline -> setup-demiurge.sh 1   (phases 1-8, ends in a
#      reboot; Phase 8 asks you to type "yes" for the boot-config edit)
#      booted with it (isolcpus=3 in /proc/cmdline) -> setup-demiurge.sh 9  (phases 9-11)
# The installer never overwrites live.conf, and Phase 8 is idempotent.
# Bucket: sync (ships to all). It writes device identity into the LOCAL bucket only.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SETUP="$HERE/setup-script/setup-demiurge.sh"
DEVICE="${DEVICE:-}"
DRY="${DRY_RUN:-0}"
run() { if [ "$DRY" = 1 ]; then echo "[dry-run] $*"; else "$@"; fi; }

[ -f "$SETUP" ] || [ "$DRY" = 1 ] || { echo "missing $SETUP" >&2; exit 1; }
[ "$(id -u)" -ne 0 ] || { echo "run as the normal Pi user (it uses sudo), not root" >&2; exit 1; }

# 1. hostname
if [ -n "$DEVICE" ]; then
  [[ "$DEVICE" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]] || { echo "DEVICE must be lowercase a-z0-9-" >&2; exit 2; }
  WANT="$DEVICE"
  if [ "$(hostname)" = "$WANT" ]; then
    echo "hostname already $WANT"
  else
    echo "setting hostname -> $WANT (was $(hostname))"
    run sudo hostnamectl set-hostname "$WANT"
    if [ "$DRY" != 1 ]; then
      if grep -q '^127\.0\.1\.1' /etc/hosts; then sudo sed -i "s/^127\.0\.1\.1.*/127.0.1.1\t$WANT/" /etc/hosts
      else printf '127.0.1.1\t%s\n' "$WANT" | sudo tee -a /etc/hosts >/dev/null; fi
    fi
  fi
else
  echo "DEVICE not set: hostname untouched. (Convention: hostname = the device name; unnamed default demiurge-<MAC4>.)"
fi

# 2. device.md (seed if missing; the script itself refuses to overwrite)
if [ -n "$DEVICE" ]; then
  run bash "$HERE/setup-script/node-repo/init-node-repo.sh" DEVICE="$DEVICE" \
      ${ROLE:+ROLE="$ROLE"} ${HARDWARE:+HARDWARE="$HARDWARE"} ${PAIR:+PAIR="$PAIR"}
else
  echo "no DEVICE: device.md not seeded (run init-node-repo.sh DEVICE=<unique> later)"
fi

# 3. the canonical installer
if grep -qw 'isolcpus=3' /proc/cmdline 2>/dev/null; then
  echo "Phase 8 boot config is active -> running phases 9-11"
  run bash "$SETUP" 9
else
  echo "Running phases 1-8 (will reboot at the end of Phase 8; re-run install.sh afterwards)"
  run bash "$SETUP" 1
fi
