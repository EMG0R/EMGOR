#!/bin/bash
#
# NEPTR Phase 4 — push from Mac to Pi.
#
# Mac is the source of truth. This script overwrites
# <pi rig>:~/NEPTR_phase4/ with the contents of _______PHASE4/. It is the
# ONLY way Pi state should change. The rig's user@host comes from
# ../../pi-rig.conf (override with PI_USER / PI_HOST env vars, or pass
# user@host as the first arg) — see that file to rename the rig.
#
# Usage:
#   ./scripts/sync_to_pi.sh                    # default target (pi-rig.conf)
#   ./scripts/sync_to_pi.sh user@host          # override target
#
# Removal (zero-trace, since nothing is touched outside ~/NEPTR_phase4):
#   ssh <pi rig> 'rm -rf ~/NEPTR_phase4'

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Pi username + hostname: single source is pi-rig.conf at the DEMIURGE_OS
# repo root. Override per-invocation with PI_USER / PI_HOST env vars, or by
# passing user@host as the first arg; edit pi-rig.conf once to rename the rig.
[[ -f "$ROOT/../pi-rig.conf" ]] && source "$ROOT/../pi-rig.conf"
TARGET="${1:-${PI_USER:-${PI_USER_DEFAULT:-gorpi7}}@${PI_HOST:-${PI_HOST_DEFAULT:-demiurge.local}}}"

echo "=== NEPTR_phase4 sync → $TARGET ==="
echo "(menus.json, gen/menu_cycle.orc, spectral/sm_heroes.json are EMORY-EDITED on the Pi via the web UI: never overwritten; seeded only if missing)"
echo "local root: $ROOT"
echo ""

# --delete IS DANGEROUS HERE, and these excludes are why it is still safe.
#
# 2026-09-15: a dry run of this script showed it was about to delete 5921 files
# on the Pi. Almost all of them were things that exist ONLY on the Pi and are
# not reproducible from the Mac:
#
#   .git/            The Pi carries its OWN repository, with its own worktrees
#                    and the branches feature/looper-pedal, looper-build and
#                    master. "Mac is the source of truth" is true for the
#                    WORKING TREE; it was never true for that history, and
#                    --delete would have destroyed it with no backup.
#   _deploy_backups/ The on-device rollback copies. These are the only way back
#                    when a deploy makes the instrument sound worse - which is
#                    exactly what happened on 2026-09-15, when the 20260909
#                    backup was the thing that restored a working grainverb and
#                    IR reverb. Deleting them removes the undo button.
#   *.bak-*          Hand-made "before I touched it" copies, same purpose.
#
# Keep these excludes. If you need a genuinely clean mirror, do it deliberately
# and take a copy of .git and _deploy_backups first.
rsync -avh --delete \
    --exclude '.git/' \
    --exclude '_deploy_backups/' \
    --exclude '*.bak-*' \
    --exclude '.DS_Store' \
    --exclude '.claude' \
    --exclude '.run' \
    --exclude '__pycache__' \
    --exclude '*.pyc' \
    --exclude 'TEENSYY' \
    --exclude 'middii.maxpat' \
    --exclude 'csound/nam/build/' \
    --exclude 'menus.json' \
    --exclude 'csound/handler/gen/menu_cycle.orc' \
    --exclude 'csound/spectral/sm_heroes.json' \
    "$ROOT/" \
    "$TARGET:NEPTR_phase4/"

rsync -avh --ignore-existing "$ROOT/menus.json" "$ROOT/csound/spectral/sm_heroes.json" "$TARGET:NEPTR_phase4/" 2>/dev/null || true
rsync -avh --ignore-existing "$ROOT/csound/spectral/sm_heroes.json" "$TARGET:NEPTR_phase4/csound/spectral/" 2>/dev/null || true
rsync -avh --ignore-existing "$ROOT/csound/handler/gen/menu_cycle.orc" "$TARGET:NEPTR_phase4/csound/handler/gen/" 2>/dev/null || true
ssh "$TARGET" 'chmod +x ~/NEPTR_phase4/scripts/launch_phase4.sh ~/NEPTR_phase4/scripts/sync_to_pi.sh 2>/dev/null || true'

# Install / refresh the Kivy UI systemd unit. Idempotent: only touches
# /etc/systemd/system if the synced copy differs, and only restarts the
# service if it was already running. First-time installs still need:
#   sudo apt install -y python3-kivy
#   python3 -m venv --system-site-packages ~/kivy-venv && ~/kivy-venv/bin/pip install python-osc
#   sudo systemctl enable --now neptr-ui.service
ssh "$TARGET" 'set -e
  # Template User=/Group=/HOME= from the account actually running this
  # (same substitution setup-demiurge.sh does for demiurge.service) — the
  # repo copy ships User=gorpi7 as a documented placeholder, not a literal
  # requirement. Compare the TEMPLATED result, not the raw repo file, so a
  # rig under any other username does not reinstall + restart every sync.
  RENDERED="$(sed -e "s/^User=.*/User=$USER/" \
                   -e "s/^Group=.*/Group=$(id -gn)/" \
                   -e "s#^Environment=HOME=.*#Environment=HOME=$HOME#" \
                   -e "s#^WorkingDirectory=.*#WorkingDirectory=$HOME/NEPTR_phase4#" \
                   ~/NEPTR_phase4/scripts/neptr-ui.service)"
  if [ "$RENDERED" != "$(cat /etc/systemd/system/neptr-ui.service 2>/dev/null)" ]; then
    echo "$RENDERED" | sudo tee /etc/systemd/system/neptr-ui.service >/dev/null
    sudo systemctl daemon-reload
  fi
  if systemctl is-active --quiet neptr-ui.service; then
    sudo systemctl restart neptr-ui.service
  fi

  # Refresh the user-level audio engine unit. Idempotent; only restarts
  # if it was already enabled/active. First-time enable still needs:
  #   systemctl --user enable --now neptr-phase4.service
  install -d ~/.config/systemd/user
  if ! diff -q ~/NEPTR_phase4/scripts/neptr-phase4.service ~/.config/systemd/user/neptr-phase4.service >/dev/null 2>&1; then
    install -m 0644 ~/NEPTR_phase4/scripts/neptr-phase4.service ~/.config/systemd/user/neptr-phase4.service
    systemctl --user daemon-reload
  fi
  if systemctl --user is-active --quiet neptr-phase4.service; then
    systemctl --user restart neptr-phase4.service
  fi

  # Restart demiurge (the actual production audio service).
  # live.conf documents this as the correct way to apply .orc/.csd changes.
  if systemctl is-active --quiet demiurge.service; then
    sudo systemctl restart demiurge.service
    echo "demiurge restarted"
  else
    echo "demiurge not active — start it manually if needed"
  fi'

echo ""
echo "=== sync done ==="
echo ""
echo "To apply changes: ssh $TARGET 'sudo systemctl restart demiurge'"
