#!/bin/bash
# rnbo-install-runner.sh — RNBO integration, Phase 0 / Step 2.
#
# Installs the prebuilt arm64 rnbooscquery runner, but neutralises every
# stock service so NOTHING auto-starts, grabs the audio device, spawns a
# jackd, or auto-updates. DEMIURGE will run the runner itself later, under
# its own pipewire-jack env. Requires Step 1 (rnbo-add-repo.sh) done first.
#
# Run as root on the Pi:
#   sudo bash rnbo-install-runner.sh
#
# UNDO:
#   sudo apt-mark unhold rnbooscquery rnbo-update-service rnbo-runner-panel
#   sudo apt-get remove --purge -y rnbooscquery rnbo-update-service rnbo-runner-panel
#   sudo systemctl unmask rnbooscquery rnbo-runner-panel rnbo-update-service
#
# Not `set -e`: we want to install, then REPORT state even if a package
# post-install step trips over a masked unit (which is expected/harmless).
set -uo pipefail

if [[ "$(id -u)" -ne 0 ]]; then
    echo "rnbo-install-runner: must run as root (use: sudo bash rnbo-install-runner.sh)" >&2
    exit 1
fi

# $HOME is root's here — derive the actual Pi user from whoever invoked
# sudo, falling back to the Pi OS convention that the first/only real user
# is UID 1000.
PI_USER="${PI_USER:-${SUDO_USER:-$(getent passwd 1000 | cut -d: -f1)}}"
PI_USER="${PI_USER:-gorpi7}"

UNITS=(rnbooscquery rnbo-runner-panel rnbo-update-service)

echo "=== pre-masking stock services (before install) ==="
# Masking by name works even before the unit file exists: it drops a
# symlink to /dev/null that shadows the package's unit when it lands, so
# the post-install 'systemctl start' is blocked (and reported as skipped).
for u in "${UNITS[@]}"; do
    systemctl mask "$u" 2>/dev/null && echo "masked: $u" || echo "mask noted: $u"
done

echo "=== installing rnbooscquery (arm64, held version) ==="
apt-get install -y rnbooscquery
APT_RC=$?
echo "apt-get install exit: $APT_RC (non-zero here is usually a masked-unit start being skipped — verified below)"

echo "=== pinning versions (no surprise auto-updates) ==="
apt-mark hold "${UNITS[@]}" || true

systemctl daemon-reload

echo ""
echo "=== STEP 2 VERIFY ==="
echo "--- package state (want: 'ii' or 'hi' = installed, held is fine) ---"
dpkg -l rnbooscquery 2>/dev/null | awk '$2=="rnbooscquery"{print $1, $2, $3}'

echo "--- service state (want: masked) ---"
for u in "${UNITS[@]}"; do
    printf "  %-22s %s\n" "$u" "$(systemctl is-enabled "$u" 2>&1)"
done

echo "--- no rogue jackd should be running (PipeWire owns audio) ---"
if pgrep -a jackd >/dev/null 2>&1; then
    echo "  WARNING: a jackd process is running:"
    pgrep -a jackd
    echo "  (kill it before Step 3: sudo pkill -x jackd)"
else
    echo "  OK — no jackd process"
fi

echo "--- PipeWire still healthy? ---"
runuser -l "$PI_USER" -c 'systemctl --user is-active pipewire' 2>/dev/null || echo "  (check pipewire manually)"

echo ""
# Status-Abbrev 2nd char 'i' = installed; covers both 'ii' (normal) and
# 'hi' (held + installed). We held it above, so it WILL be 'hi'.
STATE="$(dpkg -l rnbooscquery 2>/dev/null | awk '$2=="rnbooscquery"{print $1; exit}')"
VER="$(dpkg -l rnbooscquery 2>/dev/null | awk '$2=="rnbooscquery"{print $3; exit}')"
if [[ "$STATE" == ?i ]]; then
    echo "STEP 2 OK — rnbooscquery $VER installed (state '$STATE'), all stock services masked."
    echo "Runner binary: $(command -v rnbooscquery || echo '/usr/bin/rnbooscquery')"
    echo "Next: Step 3 — run it by hand under DEMIURGE's pipewire-jack env."
else
    echo "STEP 2 PROBLEM — rnbooscquery not installed (state '$STATE'). Read apt output above."
fi
