#!/bin/bash
# deploy-nam.sh — install the full NAM stack on a DEMIURGE Pi.
#
# Run from your Mac (or any machine with SSH access):
#   ./setup-script/deploy-nam.sh [user@host]
#
# What this does:
#   1. Rsyncs DEMIURGE_OS/nam/ (docs/templates) and neptr/csound/nam/ (the ONE
#      opcode source + vendored NAM core) to the Pi
#   2. Deploys the demiurge-run-nam wrapper to /opt/demiurge/bin/
#   3. Builds csound-nam.so via neptr/csound/nam/build-on-pi.sh (-j2) and
#      installs nam_stage.csd on the Pi
#   4. Rebuilds the demiurge-launcher (Rust) with .nam extension support
#   5. Restarts demiurge.service to pick up the new launcher + plugin
#
# Prerequisites on the Pi (should already be present on DEMIURGE):
#   libcsound64-dev, cmake, build-essential, git
#   ~/.cargo/bin/cargo  (Rust toolchain)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEMIURGE_OS="$(cd "$SCRIPT_DIR/.." && pwd)"
# Pi rig user@host: single source is pi-rig.conf at the repo root. Override
# with PI_USER / PI_HOST env vars, or pass user@host as the first arg.
[[ -f "$DEMIURGE_OS/pi-rig.conf" ]] && source "$DEMIURGE_OS/pi-rig.conf"
TARGET="${1:-${PI_USER:-${PI_USER_DEFAULT:-gorpi7}}@${PI_HOST:-${PI_HOST_DEFAULT:-demiurge.local}}}"

echo "=== DEMIURGE NAM deploy → $TARGET ==="

# ---------------------------------------------------------------------------
# 1. Sync nam/ directory to Pi
# ---------------------------------------------------------------------------
echo "--- Syncing nam/ ..."
rsync -avh --delete \
    --exclude '.DS_Store' \
    --exclude 'nam/build/' \
    --exclude 'nam/libs/' \
    "$DEMIURGE_OS/nam/" \
    "$TARGET:~/demiurge/nam/"

# The opcode source of truth lives under neptr/. Keep the vendored libs/ (the
# known-good NAM core) but not Mac build dirs. No --delete here: libs/ is large.
ssh "$TARGET" 'mkdir -p ~/demiurge/neptr/csound/nam'
rsync -avh \
    --exclude '.DS_Store' \
    --exclude '/build/' \
    --exclude '/build_inline/' \
    --exclude '/profiles/' \
    "$DEMIURGE_OS/neptr/csound/nam/" \
    "$TARGET:~/demiurge/neptr/csound/nam/"

# ---------------------------------------------------------------------------
# 2. Deploy demiurge-run-nam wrapper
# ---------------------------------------------------------------------------
echo "--- Installing demiurge-run-nam wrapper..."
rsync -avh "$DEMIURGE_OS/src/wrappers/demiurge-run-nam" "$TARGET:/tmp/demiurge-run-nam"
ssh "$TARGET" 'sudo install -m 755 /tmp/demiurge-run-nam /opt/demiurge/bin/demiurge-run-nam'

# ---------------------------------------------------------------------------
# 3. Build and install csound-nam.so + nam_stage.csd
# ---------------------------------------------------------------------------
echo "--- Building csound-nam.so on Pi..."
# The single build script (-j2: -j4 browned out a Pi 5 on 2026-10-03).
ssh "$TARGET" 'bash ~/demiurge/neptr/csound/nam/build-on-pi.sh && \
    sudo install -D -m 644 ~/demiurge/nam/templates/nam_stage.csd /opt/demiurge/lib/nam_stage.csd'

# ---------------------------------------------------------------------------
# 4. Rebuild demiurge-launcher with .nam extension support
# ---------------------------------------------------------------------------
echo "--- Syncing launcher source..."
rsync -avh \
    "$DEMIURGE_OS/src/demiurge-launcher-rs/" \
    "$TARGET:~/demiurge/src/demiurge-launcher-rs/"

echo "--- Building demiurge-launcher (Rust)..."
ssh "$TARGET" '
    set -e
    cd ~/demiurge/src/demiurge-launcher-rs
    PATH=$HOME/.cargo/bin:$PATH cargo build --release 2>&1
    sudo install -m 755 target/release/demiurge-launcher /opt/demiurge/bin/demiurge-launcher
    echo "launcher installed"
'

# ---------------------------------------------------------------------------
# 5. Restart demiurge to pick up everything
# ---------------------------------------------------------------------------
echo "--- Restarting demiurge.service..."
ssh "$TARGET" '
    sudo systemctl restart demiurge.service
    sleep 2
    sudo systemctl is-active demiurge.service && echo "demiurge: active" || echo "demiurge: inactive (check journalctl)"
'

echo ""
echo "=== deploy-nam done ==="
echo ""
echo "Add a .nam profile to ~/profiles/ on the Pi, then in live.conf:"
echo "  chain ="
echo "    in1 ->"
echo "    ~/profiles/your_model.nam"
echo ""
echo "OSC profile switch (from Mac):"
echo "  oscsend $TARGET 9000 /nam/idx i 1"
echo "  oscsend $TARGET 9000 /nam/bypass i 1"
