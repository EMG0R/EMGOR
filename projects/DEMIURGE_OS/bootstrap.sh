#!/usr/bin/env bash
# bootstrap.sh — ONE script: fresh Pi OS Lite (64-bit, Trixie) -> running Demiurge node
# with its own claude agent, public-Git access and self-update ready. Bucket: sync.
#
#   DEVICE=neuralgrid-a ROLE=navigator HARDWARE=pi5-8gb PAIR=neuralgrid-b bash bootstrap.sh
#   DRY_RUN=1 DEVICE=neuralgrid-a bash bootstrap.sh      # print the plan, change nothing
#
# WRAPPER ONLY. It calls install.sh (hostname, device.md, phases 1-11),
# setup-script/updater/enable.sh, and follows setup-script/CLAUDE_RC_AUTONOMOUS.md.
# It reimplements none of them. Idempotent: re-run after the Phase 8 reboot.
# Never logs anyone in. Never pulls/resets an existing clone. Never migrates a live rig.
# Env: DEVICE (required) ROLE HARDWARE PAIR DEMIURGE_REMOTE BRANCH REPO_DIR DRY_RUN
set -euo pipefail

DEVICE="${DEVICE:-}"
ROLE="${ROLE:-}"; HARDWARE="${HARDWARE:-}"; PAIR="${PAIR:-}"
# PUBLIC distro repo (anonymous clone). DEMIURGE_OS is the owner's PRIVATE tree; never default to it.
DEMIURGE_REMOTE="${DEMIURGE_REMOTE:-https://github.com/EMG0R/demiurge.git}"
BRANCH="${BRANCH:-main}"
REPO_DIR="${REPO_DIR:-$HOME/_______DEMIURGE}"
DRY="${DRY_RUN:-0}"
ROOT=/demiurge
STATE_DIR="$HOME/.cache/demiurge-bootstrap"
STATE="$STATE_DIR/fresh"          # written once, on the first real run
NPM_PREFIX="$HOME/.npm-global"

say()  { echo "== $*"; }
warn() { echo "!! $*" >&2; }
run()  { if [ "$DRY" = 1 ]; then echo "[dry-run] $*"; else "$@"; fi; }

# ---- 1. args --------------------------------------------------------------
[ "$(id -u)" -ne 0 ] || { echo "run as the normal Pi user (it uses sudo), not root" >&2; exit 1; }
[ -n "$DEVICE" ] || { echo "DEVICE=<unique> is required (lowercase a-z0-9-), e.g. DEVICE=neuralgrid-a" >&2; exit 2; }
[[ "$DEVICE" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]] || { echo "DEVICE must be lowercase a-z0-9-" >&2; exit 2; }
[ "$DRY" = 1 ] && say "DRY RUN: nothing will be changed, sudo is never invoked"
say "node $DEVICE  role=${ROLE:-"-"}  hardware=${HARDWARE:-"-"}  pair=${PAIR:-"-"}"
say "source $DEMIURGE_REMOTE ($BRANCH) -> $REPO_DIR"

# Fresh-vs-live is decided ONCE, before anything is installed, and remembered, so a
# re-run after install (when demiurge is legitimately active) is not mistaken for a live rig.
if [ -f "$STATE" ]; then
  FRESH="$(cat "$STATE")"
elif systemctl is-active --quiet demiurge 2>/dev/null || [ -e "$ROOT/current" ]; then
  FRESH=no
else
  FRESH=yes
fi
say "install class: $([ "$FRESH" = yes ] && echo 'FRESH' || echo 'LIVE / pre-existing (self-update layout will NOT be touched)')"
if [ "$DRY" != 1 ]; then mkdir -p "$STATE_DIR"; [ -f "$STATE" ] || echo "$FRESH" > "$STATE"; fi

# ---- 2. prereqs (automated) ----------------------------------------------
say "[automated] prerequisites"
need=()
for p in git tmux curl; do command -v "$p" >/dev/null 2>&1 || need+=("$p"); done
command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1 || need+=(nodejs npm)
command -v gh >/dev/null 2>&1 || need+=(gh)
if [ "${#need[@]}" -gt 0 ]; then
  echo "installing apt packages: ${need[*]}"
  run sudo apt-get update -qq
  run sudo apt-get install -y "${need[@]}"
else
  echo "git tmux curl node npm gh: present"
fi

# per-user npm prefix + PATH (matches CLAUDE_RC_AUTONOMOUS.md piece 1)
if [ "$DRY" = 1 ]; then
  echo "[dry-run] npm config set prefix $NPM_PREFIX; ensure PATH has $NPM_PREFIX/bin and ~/.local/bin in ~/.profile and ~/.bashrc"
else
  mkdir -p "$NPM_PREFIX/bin" "$HOME/.local/bin"
  npm config set prefix "$NPM_PREFIX"
  for rc in "$HOME/.profile" "$HOME/.bashrc"; do
    grep -qs 'npm-global/bin' "$rc" || echo 'export PATH="$HOME/.npm-global/bin:$HOME/.local/bin:$PATH"' >> "$rc"
  done
fi
export PATH="$NPM_PREFIX/bin:$HOME/.local/bin:$PATH"
if command -v claude >/dev/null 2>&1; then
  echo "claude: present ($(command -v claude))"
else
  echo "installing claude (npm global, per-user prefix)"
  run npm install -g @anthropic-ai/claude-code
fi

# ---- 3. clone (never touch an existing tree) ------------------------------
if [ -e "$REPO_DIR" ]; then
  echo "$REPO_DIR exists, not touching (no pull, no reset)"
else
  say "[automated] cloning"
  run git clone --branch "$BRANCH" "$DEMIURGE_REMOTE" "$REPO_DIR"
fi

# ---- 4. install.sh (hostname, device.md, phases) --------------------------
say "[automated] install.sh (hostname, device.md, phases)"
if [ "$DRY" != 1 ] && ! grep -qw 'isolcpus=3' /proc/cmdline 2>/dev/null; then
  cat <<'MSG'
##########################################################################
 NOTE: this Pi REBOOTS at the end of Phase 8 (it will ask you to type "yes"
 for the boot-config edit). After it comes back, log in and RE-RUN this
 exact same command. install.sh then continues at phases 9-11.
##########################################################################
MSG
fi
INSTALL="$REPO_DIR/install.sh"
if [ "$DRY" = 1 ] && [ ! -f "$INSTALL" ]; then
  INSTALL="$REPO_DIR/install.sh"
  echo "[dry-run] (install.sh not found at that path yet; in a real run it is cloned first) env DEVICE=$DEVICE ${ROLE:+ROLE=$ROLE }${HARDWARE:+HARDWARE=$HARDWARE }${PAIR:+PAIR=$PAIR }bash $INSTALL"
else
  [ -f "$INSTALL" ] || { echo "missing $INSTALL" >&2; exit 1; }
  run env DEVICE="$DEVICE" ROLE="$ROLE" HARDWARE="$HARDWARE" PAIR="$PAIR" bash "$INSTALL"
fi
# (install.sh propagates DRY_RUN itself from the environment.)

# Everything below needs the finished install (post Phase 8 reboot).
if [ "$DRY" != 1 ] && ! grep -qw 'isolcpus=3' /proc/cmdline 2>/dev/null; then
  say "Phase 8 boot config not active yet. Reboot if install.sh did not, then re-run bootstrap.sh."
  exit 0
fi

# ---- 5. COD: agent persistence + bus client (dormant) ---------------------
# One mechanism: setup-demiurge.sh Phase 12 already ran setup-script/cod/install-cod.sh, which
# fetches public EMG0R/cod and installs ITS claude-persist/cod-tmux/cod-agents + the `cod` client.
# Run again here only to pre-install Tailscale on FRESH images (package only; never logged in).
# It never joins a tank (that is the COD app's Start/Join tank) and never touches ~/.cod-bus.conf.
say "[automated] COD (dormant): agents + bus client, Tailscale-ready"
COD_HOOK="$REPO_DIR/setup-script/cod/install-cod.sh"
if [ -f "$COD_HOOK" ]; then
  run env COD_TAILSCALE="$([ "$FRESH" = yes ] && echo install || echo check)" bash "$COD_HOOK"
else
  warn "COD hook not found ($COD_HOOK); skipping COD."
fi

# ---- 6. self-update wiring (fresh installs only) -------------------------
say "self-update wiring"
if [ "$FRESH" != yes ]; then
  warn "######################################################################"
  warn " SKIPPING self-update: Demiurge was already present/running here."
  warn " Never migrating a live audio rig. Do it by hand, deliberately, if ever."
  warn "######################################################################"
elif [ -L "$ROOT/current" ]; then
  echo "$ROOT/current already a symlink; layout done"
else
  HASH="$(git -C "$REPO_DIR" rev-parse HEAD 2>/dev/null | cut -c1-12 || true)"
  HASH="${HASH:-initial}"
  echo "migrating layout: $ROOT/versions/$HASH <- $REPO_DIR (tracked files); current -> versions/$HASH"
  if [ "$DRY" = 1 ]; then
    echo "[dry-run] sudo mkdir -p $ROOT/versions $ROOT/local"
    echo "[dry-run] copy $REPO_DIR/demiurge/local -> $ROOT/local (if empty); git archive HEAD -> $ROOT/versions/$HASH; link demiurge/local -> $ROOT/local"
    echo "[dry-run] sudo ln -sfn versions/$HASH $ROOT/current"
  else
    sudo mkdir -p "$ROOT/versions" "$ROOT/local"
    sudo chown -R "$USER": "$ROOT"
    [ -d "$REPO_DIR/demiurge/local" ] && [ -z "$(ls -A "$ROOT/local")" ] && cp -a "$REPO_DIR/demiurge/local/." "$ROOT/local/"
    if [ ! -d "$ROOT/versions/$HASH" ]; then
      mkdir "$ROOT/versions/$HASH"
      git -C "$REPO_DIR" archive HEAD | tar -x -C "$ROOT/versions/$HASH"
      rm -rf "$ROOT/versions/$HASH/demiurge/local"
      # mkdir -p: the public export has no demiurge/ dir, so this ln failed and
      # aborted the whole install at the self-update wiring step. This is the
      # SECOND such site in this file; the other is in the step-6 block.
      mkdir -p "$ROOT/versions/$HASH/demiurge"
      ln -s "$ROOT/local" "$ROOT/versions/$HASH/demiurge/local"
      git -C "$REPO_DIR" rev-parse HEAD > "$ROOT/versions/$HASH/.version"
    fi
    ln -sfn "versions/$HASH" "$ROOT/current"
  fi
  CONF="$ROOT/local/updater.conf"
  if [ "$DRY" = 1 ]; then
    echo "[dry-run] write $CONF: DEMIURGE_REMOTE=$DEMIURGE_REMOTE DEMIURGE_BRANCH=$BRANCH"
  else
    printf 'DEMIURGE_REMOTE=%s\nDEMIURGE_BRANCH=%s\n' "$DEMIURGE_REMOTE" "$BRANCH" > "$CONF"
  fi
  run sudo CONFIRM_NOT_LIVE_AUDIO=yes bash "$REPO_DIR/setup-script/updater/enable.sh"
fi

# ---- 7. summary -----------------------------------------------------------
cat <<EOF2

================ SUMMARY ================
Automated (done or planned above):
  - prereqs: git tmux curl node npm gh, claude (npm global in $NPM_PREFIX)
  - clone of $DEMIURGE_REMOTE ($BRANCH) -> $REPO_DIR (untouched if it existed)
  - install.sh: hostname $DEVICE, device.md, phases (Phase 8 reboots; re-run after)
  - COD installed dormant (agents + bus client; not a tank member until the COD app joins one)
  - self-update: $([ "$FRESH" = yes ] && echo 'layout + updater.conf + enable.sh (timer starts after next boot)' || echo 'SKIPPED (live rig)')

YOU must do this once, by hand (cannot be automated):
  1. claude            # then type /login and finish the browser/code flow
  2. gh auth login     # only needed for git push
  3. Pre-accept the trust prompts in ~/.claude.json (CLAUDE_RC_AUTONOMOUS.md piece 6),
     then register an agent (claude-persist <name>); cod-agents restores it after reboots
  4. Edit demiurge/local/device.md (audio_hat, power, prose).
Nothing here restarted any audio service.
=========================================
EOF2
