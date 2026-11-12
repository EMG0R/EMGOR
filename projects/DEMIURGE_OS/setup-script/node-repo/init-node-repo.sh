#!/usr/bin/env bash
# init-node-repo.sh DEVICE=<unique> [ROLE=..] [HARDWARE=..] [PAIR=<other-unique>] [DIR=..]
# Creates this node's OWN small git repo (device layer). LOCAL bucket, never shared.
# Idempotent: existing files (esp. device.md) are NEVER overwritten.
# This is the single place device.md gets seeded; install.sh calls it.
set -euo pipefail
for a in "$@"; do case "$a" in *=*) export "${a%%=*}=${a#*=}";; esac; done

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
DEVICE="${DEVICE:-}"
[[ "$DEVICE" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]] || { echo "DEVICE=<unique> required (lowercase a-z0-9-), e.g. DEVICE=twink" >&2; exit 2; }
DIR="${DIR:-$REPO/demiurge/local}"        # node repo root = the LOCAL bucket
ROLE="${ROLE:-instrument}"; HARDWARE="${HARDWARE:-pi5-8gb}"; PAIR="${PAIR:-}"
TEMPLATE="$REPO/MD/device.template.md"
[ -f "$TEMPLATE" ] || { echo "missing $TEMPLATE" >&2; exit 1; }

mkdir -p "$DIR/config"

if [ -e "$DIR/device.md" ]; then
  echo "device.md exists, leaving it alone: $DIR/device.md"
else
  [ -f "$TEMPLATE" ] || echo "note: $TEMPLATE absent (reference only); emitting front-matter directly" >&2
  UPPER="$(echo "$DEVICE" | tr 'a-z-' 'A-Z ')"
  NODE_ID="$(echo "$DEVICE" | tr '-' '_')"
  PAIR_LINE=""
  [ -n "$PAIR" ] && PAIR_LINE="pair:        $PAIR   # other board of this one logical device"
  # Emit REAL front-matter (not a copy of the fenced template doc, which parsers can't read).
  cat > "$DIR/device.md" <<EOFDEV
---
# --- identity ---
hostname:    $DEVICE   # = the device name exactly (no prefix). Unnamed default: demiurge-<MAC4>.
device_name: $UPPER   # human display name (EDIT)
unique:      $DEVICE
node_id:     $NODE_ID   # tank node id; stable, snake_case
role:        $ROLE   # instrument | server | navigator | controller | laptop
hardware:    $HARDWARE   # pi5-16gb | pi5-8gb | pi4-8gb | mac | ...
$PAIR_LINE
# --- audio profile ---
audio_hat:   none-yet   # e.g. hifiberry-dac-adc-pro | none-yet (EDIT)
power:       high        # live.conf power knob default for this device
runs_audio:  true        # false for a server/laptop that hosts UI but no RT chain
# --- cod tank ---
tank_node:   true
codling:     null        # device persona md path, if any (device persona overrides demiurge.md)
---

# $UPPER — device.md

Sits on top of the shared MD/DEMIURGE.md (parent class). Device persona wins;
Demiurge is the parent. COD reads this to register the node; Demiurge reads it
for device-specific behaviour. See MD/device.template.md for the key reference.

## What this device IS
(one paragraph — EDIT)

## sync vs local
This file is the LOCAL bucket (demiurge/local/, never ships). State which Demiurge
services run here and what else is local vs sync. Always answer: sync or local.

## Notes
(anything an agent must know before touching this box — EDIT)
EOFDEV
  echo "seeded $DIR/device.md  (EDIT IT: device_name, audio_hat, power, codling, prose)"
fi

[ -e "$DIR/.gitignore" ] || cat > "$DIR/.gitignore" <<'GI'
# keys and secrets never enter a node repo; SSH keys stay in ~/.ssh
*.key
*.pem
id_*
secrets/
.env
GI
[ -e "$DIR/config/.gitkeep" ] || touch "$DIR/config/.gitkeep"
[ -e "$DIR/README.md" ] || cat > "$DIR/README.md" <<EOF2
# node repo: $DEVICE
This directory is the device layer of one Pi (its own git repo). Not part of the shared
Demiurge repo. See setup-script/node-repo/README.md in the shared tree.
EOF2

if [ ! -d "$DIR/.git" ]; then
  git -C "$DIR" init -q
  git -C "$DIR" add -A
  git -C "$DIR" -c user.name="${GIT_AUTHOR_NAME:-$DEVICE}" -c user.email="${GIT_AUTHOR_EMAIL:-$DEVICE@localhost}" \
      commit -q -m "node repo init: $DEVICE" || true
fi

# ---- HOOK: Agent A's SSH-mesh remotes plug in here --------------------------
# Intentionally empty. No remotes, no keys are created by this script.
# A's mesh script should run, e.g.:  git -C "$DIR" remote add <peer> <ssh-url>
[ -x "$HERE/mesh-hook.sh" ] && "$HERE/mesh-hook.sh" "$DIR" "$DEVICE" || true
# -----------------------------------------------------------------------------
echo "node repo ready: $DIR"
