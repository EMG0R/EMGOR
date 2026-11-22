#!/usr/bin/env bash
# THE single enable command:
#   sudo CONFIRM_NOT_LIVE_AUDIO=yes bash setup-script/updater/enable.sh
# Installs scripts + units and enables them. Does NOT start/restart anything audio.
# Does NOT migrate /demiurge to the versions layout: until /demiurge/current is a
# symlink the units refuse to act (ConditionPathExists / script guards).
set -euo pipefail
[ "${CONFIRM_NOT_LIVE_AUDIO:-}" = "yes" ] || { echo "REFUSING: set CONFIRM_NOT_LIVE_AUDIO=yes. Not for a live audio Pi."; exit 1; }
[ "$(id -u)" = 0 ] || { echo "run with sudo"; exit 1; }
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
install -d /opt/demiurge/bin /demiurge/versions /demiurge/local
for n in stage swap revert; do install -m0755 "$D/demiurge-update-$n.sh" "/opt/demiurge/bin/demiurge-update-$n"; done
install -m0755 "$D/demiurge-apply.sh" /opt/demiurge/bin/demiurge-apply
ln -sf /opt/demiurge/bin/demiurge-update-revert /usr/local/bin/demiurge-update-revert
install -m0644 "$D"/demiurge-update-*.service "$D"/demiurge-update-*.timer "$D"/demiurge-apply.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable demiurge-update-swap.service demiurge-update-stage.timer demiurge-apply.service   # enable only; no --now
# Queue the user-level apply (COD etc.) for the next boot; also covers nodes whose old swap.sh has no post-swap hook.
H="$(basename "$(readlink -f /demiurge/current 2>/dev/null)" 2>/dev/null || true)"
echo "${H:-bootstrap}" > /demiurge/local/apply-pending
echo "apply-pending set: next boot runs setup-script/apply-user.sh (COD dormant install)."
echo "enabled (timer starts after next boot). Set DEMIURGE_REMOTE in /demiurge/local/updater.conf."
