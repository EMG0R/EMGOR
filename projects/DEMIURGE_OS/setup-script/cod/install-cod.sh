#!/usr/bin/env bash
# install-cod.sh -- Demiurge's hook for COD (github.com/EMG0R/cod). Bucket: sync.
#
#   install-cod.sh             full: layout + fetch cod + install dormant services
#   install-cod.sh --refresh   no fetch: re-copy from the current checkout (used by cod-update)
#
# What it does (idempotent; user-level; never needs root except optional linger):
#   1. Downloads public EMG0R/cod into $COD_ROOT/versions/<hash>, $COD_ROOT/current -> it,
#      using the Demiurge updater scripts (stage/swap) parameterized by DEMIURGE_ROOT.
#      ~/cod -> $COD_ROOT/current (only if ~/cod does not exist).
#   2. Installs cod's own files from the checkout: claude-persist, the `cod` client,
#      cod-tmux / cod-agents / cod-hub units. (cod ships no installer yet; this is the
#      minimal Demiurge-side hook. See docs/cod.md "what cod should own".)
#   3. Keeps it current: cod-update.timer (enabled; polls GitHub only, never the tank).
#   4. DORMANT: cod-bus is NOT enabled and is guarded on ~/.cod-bus.conf. Hub, agents and
#      tmux units are written but not enabled unless agents are already registered.
#   5. Tailscale: never `up`, never logged in. COD_TAILSCALE=install only installs the package.
# Never touches: ~/.cod-bus.conf, ~/.claude-persist/, Demiurge audio services.
# Never starts/stops/restarts any unit. Never rsync --delete.
#
# Env: COD_ROOT COD_REMOTE COD_BRANCH COD_TAILSCALE(check|install) SYSTEMCTL LOGINCTL DRY_RUN
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="${1:-full}"
COD_ROOT="${COD_ROOT:-$HOME/.local/share/cod-update}"
COD_REMOTE="${COD_REMOTE:-https://github.com/EMG0R/cod.git}"
COD_BRANCH="${COD_BRANCH:-main}"
COD_TAILSCALE="${COD_TAILSCALE:-check}"
SYSTEMCTL="${SYSTEMCTL:-systemctl}"
LOGINCTL="${LOGINCTL:-loginctl}"
LIB="$HOME/.local/lib/demiurge-cod"
UNITDIR="$HOME/.config/systemd/user"
CHANGED=0

log()  { echo "[cod] $*"; }
warn() { echo "[cod] !! $*" >&2; }
# put <mode> <src> <dst>: install only when content differs (idempotent, marks CHANGED).
put() {
  if [ -f "$3" ] && cmp -s "$2" "$3"; then return 0; fi
  install -D -m "$1" "$2" "$3"; CHANGED=1; log "installed $3"
}
usr() { "$SYSTEMCTL" --user "$@"; }

# Templates + updater scripts: from the Demiurge tree, or already in $LIB (refresh).
if [ -d "$HERE/../updater" ]; then
  TPL="$HERE"; UPD="$HERE/../updater"
else
  TPL="$HERE/templates"; UPD="$HERE"
fi

# --- 1. helper scripts + layout -----------------------------------------------
mkdir -p "$COD_ROOT/versions" "$COD_ROOT/local" "$UNITDIR"
for n in stage swap; do
  if [ -f "$UPD/demiurge-update-$n.sh" ]; then put 0755 "$UPD/demiurge-update-$n.sh" "$LIB/demiurge-update-$n"
  else put 0755 "$UPD/demiurge-update-$n" "$LIB/demiurge-update-$n"; fi
done
if [ "$HERE" != "$LIB" ]; then
  put 0755 "$HERE/install-cod.sh" "$LIB/install-cod.sh"
  for t in cod-bus.service 10-dormant-guard.conf 10-demiurge-claude-bin.conf cod-update.service cod-update.timer; do
    put 0644 "$HERE/$t" "$LIB/templates/$t"
  done
fi
# updater.conf is LOCAL: written once, never overwritten.
if [ ! -f "$COD_ROOT/local/updater.conf" ]; then
  cat > "$COD_ROOT/local/updater.conf" <<CONF
DEMIURGE_REMOTE=$COD_REMOTE
DEMIURGE_BRANCH=$COD_BRANCH
DEMIURGE_VERIFY_FILES="aquarium/cod aquarium/agents/claude-persist aquarium/agents/systemd/cod-agents.service aquarium/agents/systemd/cod-tmux.service"
DEMIURGE_NO_LOCAL_LINK=1
CONF
fi

# --- 2. fetch cod (first install only; the timer keeps it current) ------------
if [ "$MODE" != --refresh ] && [ ! -L "$COD_ROOT/current" ]; then
  log "fetching $COD_REMOTE"
  export DEMIURGE_ROOT="$COD_ROOT" DEMIURGE_FIRST_INSTALL=1
  "$LIB/demiurge-update-stage" || warn "stage failed"
  "$LIB/demiurge-update-swap"  || warn "swap failed"
fi
if [ ! -L "$COD_ROOT/current" ]; then
  warn "no cod checkout yet (offline?). Units for the updater are installed; it retries every 6h."
  SRC=""
else
  SRC="$COD_ROOT/current/aquarium"
  [ -e "$HOME/cod" ] || [ -L "$HOME/cod" ] || ln -s "$COD_ROOT/current" "$HOME/cod"
fi

# --- 3. cod's own files -> user dirs ------------------------------------------
# ONE entrypoint, two callers: once cod ships its canonical tank-node installer,
# Demiurge just calls it in dormant mode instead of copying a guessed layout.
# The copy path below is only the fallback for a cod checkout that predates it.
if [ -n "$SRC" ] && [ -f "$SRC/install-tank-node.sh" ]; then
  log "delegating to cod's aquarium/install-tank-node.sh --dormant"
  bash "$SRC/install-tank-node.sh" --dormant || warn "cod install-tank-node.sh --dormant failed"
  SRC=""
fi
if [ -n "$SRC" ]; then
  [ -f "$SRC/agents/claude-persist" ] && put 0755 "$SRC/agents/claude-persist" "$HOME/.local/bin/claude-persist"
  [ -f "$SRC/cod" ] && put 0755 "$SRC/cod" "$HOME/.local/bin/cod"
  # Legacy copy from the old Demiurge cod-bus dir (it defaulted to a private hub); replace only if present.
  [ -f "$HOME/bin/cod" ] && put 0755 "$SRC/cod" "$HOME/bin/cod"
  for u in "$SRC"/agents/systemd/*.service "$SRC"/systemd/*.service; do
    [ -f "$u" ] && put 0644 "$u" "$UNITDIR/$(basename "$u")"
  done
  # cod-bus.service: prefer cod's own if it ever ships one, else the Demiurge fallback.
  if [ -f "$SRC/systemd/cod-bus.service" ] || [ -f "$SRC/agents/systemd/cod-bus.service" ]; then :; else
    put 0644 "$TPL/cod-bus.service" "$UNITDIR/cod-bus.service"
  fi
  put 0644 "$TPL/10-dormant-guard.conf"       "$UNITDIR/cod-bus.service.d/10-dormant-guard.conf"
  put 0644 "$TPL/10-demiurge-claude-bin.conf" "$UNITDIR/cod-agents.service.d/10-demiurge.conf"
fi
put 0644 "$TPL/cod-update.service" "$UNITDIR/cod-update.service"
put 0644 "$TPL/cod-update.timer"   "$UNITDIR/cod-update.timer"

# --- 4. systemd: reload only if something changed; enable (never start) -------
# linger so user units run at boot without a login (needs root; best effort).
if ! "$LOGINCTL" show-user "$(id -un)" 2>/dev/null | grep -q '^Linger=yes'; then
  sudo -n "$LOGINCTL" enable-linger "$(id -un)" 2>/dev/null || warn "could not enable linger; run: sudo loginctl enable-linger $(id -un)"
fi
[ "$CHANGED" = 1 ] && { usr daemon-reload || warn "systemctl --user unavailable; units written, enable them after login"; }
enable_once() { usr is-enabled "$1" >/dev/null 2>&1 || usr enable "$1" || warn "could not enable $1"; }
enable_once cod-update.timer
# Agent persistence: enable only when agents are already registered AND the legacy
# claude-rc / demiurge-agents units are not doing the same job (two mechanisms).
if ls "$HOME"/.claude-persist/*.sid >/dev/null 2>&1; then
  if [ -e "$UNITDIR/claude-rc.service" ] || [ -e "$UNITDIR/demiurge-agents.service" ]; then
    warn "legacy claude-rc/demiurge-agents unit present: leaving cod-agents DISABLED (migrate by hand: disable legacy, enable cod-tmux cod-agents)"
  else
    enable_once cod-tmux.service; enable_once cod-agents.service
  fi
fi
# Tank membership is a LOCAL decision: ~/.cod-bus.conf with URL+token. Re-runs on an
# existing member keep it enabled; we never create or edit the file.
if [ -f "$HOME/.cod-bus.conf" ] && grep -q '^COD_BUS_URL=.' "$HOME/.cod-bus.conf" && grep -q '^COD_BUS_TOKEN=.' "$HOME/.cod-bus.conf"; then
  enable_once cod-bus.service
else
  log "cod-bus dormant (no tank config); Start/Join tank in the COD app turns it on"
fi

# --- 5. tailscale: ready, never joined ----------------------------------------
if command -v tailscale >/dev/null 2>&1; then
  log "tailscale present (not touched; COD app Link Device authenticates it)"
elif [ "$COD_TAILSCALE" = install ]; then
  log "installing tailscale package (no login, no 'up')"
  curl -fsSL https://tailscale.com/install.sh | sudo sh || warn "tailscale install failed"
else
  log "tailscale not installed (COD_TAILSCALE=install to pre-install; Link Device does it otherwise)"
fi
log "done"
