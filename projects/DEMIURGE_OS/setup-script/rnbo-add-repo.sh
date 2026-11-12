#!/bin/bash
# rnbo-add-repo.sh — RNBO integration, Phase 0 / Step 1.
#
# Adds Cycling '74's apt repository (Trixie / arm64) and its signing key,
# then refreshes apt and reports whether `rnbooscquery` is visible for our
# architecture. INSTALLS NOTHING. Fully reversible — see UNDO below.
#
# Run as root on the Pi:
#   sudo bash rnbo-add-repo.sh
#
# UNDO:
#   sudo rm -f /usr/share/keyrings/apt-cycling74-pubkey.asc \
#              /etc/apt/sources.list.d/cycling74.list
#   sudo apt-get update
#
# Repo facts (verified 2026-06-05 against the live Release file):
#   base : https://c74-apt.nyc3.digitaloceanspaces.com/raspbian/
#   suite: trixie   arch: arm64   components: main extra
#   (bookworm in that repo is the OLD armhf 32-bit channel — not for us.)
set -euo pipefail

if [[ "$(id -u)" -ne 0 ]]; then
    echo "rnbo-add-repo: must run as root (use: sudo bash rnbo-add-repo.sh)" >&2
    exit 1
fi

KEY_URL="https://raw.githubusercontent.com/Cycling74/rnbo.oscquery.runner/main/config/apt-cycling74-pubkey.asc"
KEY_DST="/usr/share/keyrings/apt-cycling74-pubkey.asc"
LIST_DST="/etc/apt/sources.list.d/cycling74.list"

echo "=== fetching Cycling '74 signing key ==="
curl -fsSL "$KEY_URL" -o "$KEY_DST"
chmod 0644 "$KEY_DST"
echo "key installed: $KEY_DST"

echo "=== writing apt source (trixie / arm64) ==="
cat > "$LIST_DST" <<EOF
deb [signed-by=$KEY_DST arch=arm64] https://c74-apt.nyc3.digitaloceanspaces.com/raspbian/ trixie main extra
EOF
echo "source installed: $LIST_DST"
cat "$LIST_DST"

echo "=== apt-get update ==="
apt-get update

echo "=== is rnbooscquery visible for arm64? ==="
apt-cache policy rnbooscquery || true

echo ""
echo "=== STEP 1 RESULT ==="
# NB: read the candidate into a var with awk (consumes all input) rather than
# piping to `grep -q`, which exits early and — under `set -o pipefail` —
# makes the upstream apt-cache die with SIGPIPE and falsely fail the test.
CAND="$(apt-cache policy rnbooscquery 2>/dev/null | awk '/Candidate:/{print $2; exit}')"
if [[ -n "$CAND" && "$CAND" != "(none)" ]]; then
    echo "OK — rnbooscquery is visible. Candidate version: $CAND"
    echo "Nothing installed yet. Proceed to Step 2 (install + mask service)."
else
    echo "PROBLEM — rnbooscquery has no install candidate for this arch/suite."
    echo "Do NOT proceed. Check the apt-get update output above for repo errors."
fi
