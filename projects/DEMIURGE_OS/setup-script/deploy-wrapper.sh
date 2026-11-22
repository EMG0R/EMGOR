#!/bin/bash
# deploy-wrapper.sh — safely sync ONE src/wrappers/* file to a live Pi's
# /opt/demiurge/bin/, without silently clobbering a Pi-side fix that never
# made it back into the repo (this is what caused src/wrappers/demiurge-run-csound
# to drift out of sync with the deployed copy — see git log on that file).
#
# Run from your Mac (or any machine with SSH access):
#   ./setup-script/deploy-wrapper.sh <wrapper-filename> [user@host]
#
# Examples:
#   ./setup-script/deploy-wrapper.sh demiurge-run-csound
#   ./setup-script/deploy-wrapper.sh demiurge-run-nam <user>@<host>   # override pi-rig.conf
#
# What this does:
#   1. Confirms the named file exists locally under src/wrappers/.
#   2. Diffs the local repo copy against the DEPLOYED copy on the Pi and
#      prints the diff — you must look at this before it installs anything.
#      If there is no difference, it says so and exits (nothing to do).
#   3. Prompts for confirmation before touching the Pi.
#   4. Stages the file via /tmp on the Pi, then installs it with
#      `sudo install -m0755` (same pattern used by deploy-nam.sh and
#      deploy-rnbo-wave1.sh for wrapper installs).
#   5. Reminds you to restart demiurge.service afterward — it does NOT do
#      this automatically, since a wrapper swap while a session is running
#      is disruptive and should be a deliberate, separate step.
#
# This script does nothing unless a human runs it. It is not invoked by
# setup-demiurge.sh or any other automated path.
set -euo pipefail

WRAPPER="${1:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEMIURGE_OS="$(cd "$SCRIPT_DIR/.." && pwd)"
# Pi rig user@host: single source is pi-rig.conf at the repo root. Override
# with PI_USER / PI_HOST env vars, or pass user@host as the second arg.
[[ -f "$DEMIURGE_OS/pi-rig.conf" ]] && source "$DEMIURGE_OS/pi-rig.conf"
TARGET="${2:-${PI_USER:-${PI_USER_DEFAULT:-gorpi7}}@${PI_HOST:-${PI_HOST_DEFAULT:-demiurge.local}}}"

if [[ -z "$WRAPPER" ]]; then
    echo "usage: $0 <wrapper-filename> [user@host]" >&2
    echo "  e.g.: $0 demiurge-run-csound" >&2
    exit 1
fi
LOCAL_SRC="$DEMIURGE_OS/src/wrappers/$WRAPPER"
REMOTE_DST="/opt/demiurge/bin/$WRAPPER"

test -f "$LOCAL_SRC" || { echo "not found: $LOCAL_SRC (must be a file under src/wrappers/)" >&2; exit 1; }

echo "=== deploy-wrapper: $WRAPPER -> $TARGET:$REMOTE_DST ==="
echo ""

# ---------------------------------------------------------------------------
# 1. Diff local repo copy against the deployed copy. Requires the operator
#    to actually see what's changing before anything is overwritten.
# ---------------------------------------------------------------------------
echo "--- Diffing local repo copy against deployed copy on the Pi ..."
DIFF_OUT="$(diff -u <(ssh "$TARGET" "cat '$REMOTE_DST' 2>/dev/null || true") "$LOCAL_SRC" || true)"

if [[ -z "$DIFF_OUT" ]]; then
    echo "No difference — deployed copy already matches the repo. Nothing to do."
    exit 0
fi

echo "$DIFF_OUT"
echo ""
echo "(diff shown as: deployed-on-Pi -> repo-local; the above is what would change)"
echo ""

read -r -p "Install this version to $TARGET:$REMOTE_DST? [yes/no]: " _CONFIRM
echo ""
if [[ "$_CONFIRM" != "yes" ]]; then
    echo "Aborted. Nothing changed on the Pi."
    exit 0
fi

# ---------------------------------------------------------------------------
# 2. Stage via /tmp, then install with sudo install -m0755.
# ---------------------------------------------------------------------------
echo "--- Staging $WRAPPER via /tmp on $TARGET ..."
rsync -avh "$LOCAL_SRC" "$TARGET:/tmp/$WRAPPER"

echo "--- Installing to $REMOTE_DST ..."
ssh "$TARGET" "sudo install -m0755 '/tmp/$WRAPPER' '$REMOTE_DST' && rm -f '/tmp/$WRAPPER'"

echo ""
echo "=== deploy-wrapper done: $WRAPPER installed at $TARGET:$REMOTE_DST ==="
echo ""
echo "REMINDER: restart demiurge.service to pick up the new wrapper:"
echo "  ssh $TARGET 'sudo systemctl restart demiurge.service'"
echo "  ssh $TARGET 'sudo systemctl is-active demiurge.service'"
