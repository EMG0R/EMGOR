#!/usr/bin/env bash
# firstrun.sh — first-boot hook: stock Pi OS Lite (64-bit, Trixie) -> Demiurge node.
# Bucket: sync. WRAPPER ONLY: it fetches the public repo and runs its bootstrap.sh.
# It reimplements no install/updater/device logic.
#
# Two modes (same file):
#   install  (no args)  : bake config, copy self to /usr/local/sbin, write + enable
#                         demiurge-firstrun.service. Safe from Imager's systemd.run hook.
#   run      (arg: run) : what the service executes each boot until Demiurge is installed:
#                         prereqs -> clone -> bootstrap.sh as the Pi user -> mark done + disable.
# DRY_RUN=1 prints the plan, changes nothing, needs no root.
set -euo pipefail

# --- default node naming (used only when DEMIURGE_DEVICE / DEMIURGE_USER are NOT set) -----------------
# Precedence for DEVICE (final hostname = DEVICE exactly, no prefix):
#   1. DEMIURGE_DEVICE env (EMGOR's DEMIURGE FLASHER)   -> verbatim, always wins
#   2. current hostname if set and != raspberrypi (Imager customisation): used verbatim (sanitised) as DEVICE
#   3. last 4 hex of the MAC of wlan0, else eth0, else first non-lo iface -> DEVICE=demiurge-<mac4> (e.g. demiurge-a1b2), so the default hostname still reads as a Demiurge
# Never falls back to a real node name (neptr etc.). USER default = first uid-1000 user (Imager creates its own), never "pi".
_sanitise() { echo "$1" | tr 'A-Z' 'a-z' | sed -E 's/[^a-z0-9]+/-/g; s/^-+//; s/-+$//'; }
_mac4() {
  local i a
  for i in wlan0 eth0 $(ls /sys/class/net 2>/dev/null | grep -v '^lo$' || true); do
    a="$(cat "/sys/class/net/$i/address" 2>/dev/null | tr -d ':\n' | tr 'A-Z' 'a-z' || true)"
    if [ "${#a}" -ge 4 ] && [ "$a" != "000000000000" ]; then echo "${a: -4}"; return 0; fi
  done
  a="$(tr -d '[:space:]' < /etc/machine-id 2>/dev/null | cut -c1-4 || true)"; echo "${a:-0000}"
}
_derive_device() {
  local h d; h="$(hostname 2>/dev/null | tr 'A-Z' 'a-z' || true)"
  if [ -n "$h" ] && [ "$h" != raspberrypi ] && [ "$h" != localhost ]; then
    d="$(_sanitise "$h")"
    case "$d" in ""|demiurge|demiurge-image|demiurge-golden) d="";; esac
    [ -n "$d" ] && { echo "$d"; return 0; }
  fi
  echo "demiurge-$(_mac4)"
}
_derive_user() { local u; u="$(getent passwd 1000 2>/dev/null | cut -d: -f1 || true)"; echo "${u:-demiurge}"; }

# ======================= EDIT BLOCK (baked in at flash time) =================
DEMIURGE_DEVICE="${DEMIURGE_DEVICE:-$(_derive_device)}"   # unique suffix, lowercase a-z0-9-. Unset => derived (hostname, else MAC4). See above.
DEMIURGE_ROLE="${DEMIURGE_ROLE:-instrument}"         # instrument | navigator | ...
DEMIURGE_HARDWARE="${DEMIURGE_HARDWARE:-pi5-16gb}"   # pi5-16gb | pi5-8gb | ...
DEMIURGE_PAIR="${DEMIURGE_PAIR:-}"                   # optional paired node, e.g. neuralgrid-b
DEMIURGE_REMOTE="${DEMIURGE_REMOTE:-https://github.com/EMG0R/demiurge.git}"   # PUBLIC distro (anon clone); NOT the private DEMIURGE_OS
DEMIURGE_USER="${DEMIURGE_USER:-$(_derive_user)}"       # unset => first uid-1000 user (the Imager user); bootstrap runs as it
DEMIURGE_BRANCH="${DEMIURGE_BRANCH:-main}"
# Phase 8 asks you to type "yes" for the boot-config edit. Headless first boot can't, so feed it.
DEMIURGE_AUTO_YES="${DEMIURGE_AUTO_YES:-1}"
# ===================== END EDIT BLOCK ========================================

DRY="${DRY_RUN:-0}"
MODE="${1:-install}"
LOG=/var/log/demiurge-firstrun.log
CONF=/etc/demiurge-firstrun.conf
SELF_DST=/usr/local/sbin/demiurge-firstrun.sh
UNIT=/etc/systemd/system/demiurge-firstrun.service
STATE=/var/lib/demiurge-firstrun
DONE="$STATE/done"
REPO_DIR="/home/$DEMIURGE_USER/_______DEMIURGE"

if [ "$DRY" = 1 ]; then
  say() { echo "[dry-run] $*"; }
  run() { echo "[dry-run] $*"; }
else
  say() { echo "$(date '+%F %T') firstrun: $*" | tee -a "$LOG"; }
  run() { "$@"; }
fi

[[ "$DEMIURGE_DEVICE" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]] || { echo "DEMIURGE_DEVICE must be lowercase a-z0-9-" >&2; exit 2; }
if [ "$DRY" != 1 ] && [ "$(id -u)" -ne 0 ]; then echo "run as root (sudo), or use DRY_RUN=1" >&2; exit 1; fi

# ---------------------------------------------------------------- install ----
do_install() {
  say "install mode: node $DEMIURGE_DEVICE role=$DEMIURGE_ROLE hardware=$DEMIURGE_HARDWARE pair=${DEMIURGE_PAIR:-"-"} user=$DEMIURGE_USER"
  say "write $CONF (baked config, mode 0600)"
  say "copy self -> $SELF_DST"
  say "write + enable $UNIT (oneshot, After=network-online, skips once $DONE exists)"
  say "strip systemd.run* / kernel-command-line.target tokens from cmdline.txt if present (no loop)"
  say "if system is fully booted: start the unit now (--no-block); else it runs on the next boot"
  [ "$DRY" = 1 ] && return 0

  mkdir -p "$STATE"
  [ -f "$DONE" ] && { say "already installed ($DONE), nothing to arm"; return 0; }
  umask 077
  {
    for v in DEVICE ROLE HARDWARE PAIR REMOTE USER BRANCH AUTO_YES; do
      n="DEMIURGE_$v"; printf '%s=%q\n' "$n" "${!n}"
    done
  } > "$CONF"
  umask 022
  install -m 0755 "$0" "$SELF_DST"
  cat > "$UNIT" <<UNITEOF
[Unit]
Description=Demiurge first-boot install (wraps bootstrap.sh; disables itself when done)
Wants=network-online.target
After=network-online.target
ConditionPathExists=!$DONE

[Service]
Type=oneshot
RemainAfterExit=no
TimeoutStartSec=0
ExecStart=$SELF_DST run

[Install]
WantedBy=multi-user.target
UNITEOF
  systemctl daemon-reload
  systemctl enable demiurge-firstrun.service
  for f in /boot/firmware/cmdline.txt /boot/cmdline.txt; do
    [ -f "$f" ] && sed -i -E 's# ?systemd\.(run|run_success_action|run_failure_action)=[^ ]*##g; s# ?systemd\.unit=kernel-command-line\.target##' "$f"
  done
  sync
  case "$(systemctl is-system-running 2>/dev/null || true)" in
    running|degraded) systemctl start --no-block demiurge-firstrun.service ;;
    *) say "not in a full boot yet; service will run on the next boot" ;;
  esac
  say "armed. Log: $LOG"
}

# -------------------------------------------------------------------- run ----
do_run() {
  [ "$DRY" = 1 ] || { [ -f "$CONF" ] && . "$CONF"; REPO_DIR="/home/$DEMIURGE_USER/_______DEMIURGE"; }
  say "run mode: node $DEMIURGE_DEVICE role=$DEMIURGE_ROLE hardware=$DEMIURGE_HARDWARE pair=${DEMIURGE_PAIR:-"-"}"
  if [ "$DRY" != 1 ] && [ -f "$DONE" ]; then say "done marker present; nothing to do"; return 0; fi

  say "check user $DEMIURGE_USER exists"
  if [ "$DRY" != 1 ]; then
    id "$DEMIURGE_USER" >/dev/null 2>&1 || { say "ERROR: user '$DEMIURGE_USER' missing. Set DEMIURGE_USER to the Imager user."; exit 1; }
  fi

  say "wait for network (up to 5 min), then ensure git + curl"
  if [ "$DRY" != 1 ]; then
    ok=0
    for _ in $(seq 1 60); do
      if getent hosts github.com >/dev/null 2>&1; then ok=1; break; fi
      sleep 5
    done
    [ "$ok" = 1 ] || { say "no network (github.com unresolved). Exiting; the service retries next boot. Check Wi-Fi settings."; exit 1; }
    need=()
    for p in git curl; do command -v "$p" >/dev/null 2>&1 || need+=("$p"); done
    if [ "${#need[@]}" -gt 0 ]; then
      say "apt-get install: ${need[*]}"
      apt-get update -qq && apt-get install -y "${need[@]}" || { say "apt failed; retry next boot"; exit 1; }
    fi
  else
    run "apt-get update && apt-get install -y git curl   # only if missing"
  fi

  say "clone $DEMIURGE_REMOTE ($DEMIURGE_BRANCH) -> $REPO_DIR as $DEMIURGE_USER (skipped if present)"
  if [ "$DRY" = 1 ]; then
    run "runuser -u $DEMIURGE_USER -- git clone --branch $DEMIURGE_BRANCH $DEMIURGE_REMOTE $REPO_DIR"
  elif [ -d "$REPO_DIR/.git" ]; then
    say "clone exists, not touching"
  else
    runuser -u "$DEMIURGE_USER" -- git clone --branch "$DEMIURGE_BRANCH" "$DEMIURGE_REMOTE" "$REPO_DIR" || { say "clone failed; retry next boot"; exit 1; }
  fi

  say "run bootstrap.sh as $DEMIURGE_USER (Phase 8 reboots; this unit re-runs after, to finish phases 9-11 + agent + self-update)"
  local yes_note=""; [ "$DEMIURGE_AUTO_YES" = 1 ] && yes_note=" < <(yes yes)"
  if [ "$DRY" = 1 ]; then
    run "runuser -l $DEMIURGE_USER -c 'cd $REPO_DIR && DEVICE=$DEMIURGE_DEVICE ROLE=$DEMIURGE_ROLE HARDWARE=$DEMIURGE_HARDWARE PAIR=${DEMIURGE_PAIR} DEMIURGE_REMOTE=$DEMIURGE_REMOTE BRANCH=$DEMIURGE_BRANCH REPO_DIR=$REPO_DIR bash bootstrap.sh'$yes_note"
  else
    [ -f "$REPO_DIR/bootstrap.sh" ] || { say "ERROR: $REPO_DIR/bootstrap.sh missing in the clone"; exit 1; }
    local cmd="cd '$REPO_DIR' && DEVICE='$DEMIURGE_DEVICE' ROLE='$DEMIURGE_ROLE' HARDWARE='$DEMIURGE_HARDWARE' PAIR='$DEMIURGE_PAIR' DEMIURGE_REMOTE='$DEMIURGE_REMOTE' BRANCH='$DEMIURGE_BRANCH' REPO_DIR='$REPO_DIR' bash bootstrap.sh"
    set +e
    if [ "$DEMIURGE_AUTO_YES" = 1 ]; then
      runuser -l "$DEMIURGE_USER" -c "$cmd" < <(yes yes) >>"$LOG" 2>&1
    else
      runuser -l "$DEMIURGE_USER" -c "$cmd" >>"$LOG" 2>&1
    fi
    rc=$?
    set -e
    say "bootstrap.sh exit code $rc"
    [ "$rc" -eq 0 ] || { say "bootstrap failed; see $LOG. Service stays armed and retries next boot."; exit "$rc"; }
  fi

  say "finish check: Phase 8 boot config active (isolcpus=3 in /proc/cmdline)?"
  if [ "$DRY" = 1 ]; then
    run "if isolcpus=3 active: touch $DONE; systemctl disable demiurge-firstrun.service; rm -f $CONF. Else leave armed (re-runs after the reboot)"
  elif grep -qw 'isolcpus=3' /proc/cmdline 2>/dev/null; then
    mkdir -p "$STATE"; date > "$DONE"
    systemctl disable demiurge-firstrun.service >>"$LOG" 2>&1 || true
    rm -f "$CONF" "$UNIT" "$SELF_DST"; systemctl daemon-reload || true
    say "Demiurge installed. firstrun disabled and removed."
  else
    say "Phase 8 reboot not done yet. Staying armed; will re-run on the next boot."
    return 0
  fi

  local banner
  banner="##########################################################################
 ONE MANUAL STEP LEFT (cannot be automated):
   ssh $DEMIURGE_USER@$DEMIURGE_DEVICE.local
   claude          # type /login, finish the browser/code flow
   gh auth login   # only needed to git push
 Then pre-accept the trust prompts in ~/.claude.json (see
 setup-script/CLAUDE_RC_AUTONOMOUS.md). After that the agent self-hosts
 every boot and Demiurge self-updates from public GitHub.
##########################################################################"
  if [ "$DRY" = 1 ]; then echo "[dry-run] would print:"; echo "$banner"; else echo "$banner" | tee -a "$LOG"; fi
  say "FINAL: DEVICE=$DEMIURGE_DEVICE hostname=$DEMIURGE_DEVICE user=$DEMIURGE_USER (seeded into device.md via bootstrap DEVICE=)"
}

case "$MODE" in
  install) do_install ;;
  run)     do_run ;;
  *) echo "usage: firstrun.sh [install|run]" >&2; exit 2 ;;
esac
