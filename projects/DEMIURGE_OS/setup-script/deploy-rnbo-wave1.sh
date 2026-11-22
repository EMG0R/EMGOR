#!/bin/bash
# deploy-rnbo-wave1.sh — install the RNBO → NEPTR audio integration.
#
# Installs: the rebuilt launcher (rnbo stage type), updated wrappers
# (demiurge-run-rnbo, csound cd-fix), unified mode-select, the compile-enabled
# runner binary at /opt/rnbo/bin, its config + always-on rnbo-runner.service.
# Then restarts demiurge.service onto the new ~/demiurge/live.conf.
#
# Run as the normal user (calls sudo for privileged installs):
#   bash deploy-rnbo-wave1.sh
set -euo pipefail
REPO="$HOME/_______DEMIURGE"
RUNNER_BIN="$HOME/rnbo.oscquery.runner/build/bin/rnbooscquery"
export XDG_RUNTIME_DIR="/run/user/$(id -u)"

echo "=== stop temporary hand-started runner units ==="
systemctl --user stop rnbo-built rnbo-spike 2>/dev/null || true
systemctl --user reset-failed rnbo-built rnbo-spike 2>/dev/null || true

echo "=== install rebuilt launcher + wrappers + env prelude ==="
sudo install -m0755 "$REPO/src/demiurge-launcher-rs/target/release/demiurge-launcher" /opt/demiurge/bin/demiurge-launcher
sudo install -m0755 "$REPO/src/wrappers/demiurge-env.sh" /opt/demiurge/lib/demiurge-env.sh
for w in "$REPO"/src/wrappers/demiurge-run-*; do
    sudo install -m0755 "$w" "/opt/demiurge/bin/$(basename "$w")"
done

echo "=== install patched csound JACK plugin (long-port-name stack-overflow fix) ==="
# csound 6.18 librtjack overflows a 64-byte stack buffer on JACK port names >63
# chars (pipewire-jack / Scarlett names) -> *** stack smashing *** on every open.
# Rebuild it if the prebuilt isn't present; back up the stock plugin once.
RTJACK_DST=/usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/librtjack.so
if [[ -f "$REPO/config/csound/librtjack.so" ]]; then
    sudo test -f "$RTJACK_DST.orig" || sudo cp -a "$RTJACK_DST" "$RTJACK_DST.orig"
    sudo install -m0644 "$REPO/config/csound/librtjack.so" "$RTJACK_DST"
    echo "  installed patched librtjack.so (orig at $RTJACK_DST.orig)"
else
    echo "  prebuilt missing — run $REPO/setup-script/build-rtjack-fix.sh first"
fi

# NOTE: sync_layer on/off is handled INSIDE the launcher (it reads live.conf and,
# when sync_layer=off, runs the chain's program directly on the interface). There
# is no mode-select service/drop-in. The launcher install above is all that's needed.

echo "=== install compile-enabled runner binary -> /opt/rnbo/bin ==="
sudo install -d /opt/rnbo/bin
sudo install -m0755 "$RUNNER_BIN" /opt/rnbo/bin/rnbooscquery

echo "=== runner config (user) ==="
install -d "$HOME/.config/rnbo"
install -m0644 "$REPO/config/rnbo/demiurge-runner.json" "$HOME/.config/rnbo/demiurge.json"

echo "=== install RNBO auto-reload watcher (--user: reload slot 0 on Max push) ==="
install -d "$HOME/.local/bin" "$HOME/.config/systemd/user"
install -m0755 "$REPO/config/rnbo/demiurge-rnbo-autoreload.sh"      "$HOME/.local/bin/demiurge-rnbo-autoreload.sh"
install -m0644 "$REPO/config/rnbo/demiurge-rnbo-autoreload.service" "$HOME/.config/systemd/user/demiurge-rnbo-autoreload.service"
systemctl --user daemon-reload
systemctl --user enable --now demiurge-rnbo-autoreload.service

echo "=== install RNBO runner gate (ExecCondition: only run when rnbo is in use) ==="
sudo install -m0755 "$REPO/config/rnbo/demiurge-rnbo-gate.sh" /usr/local/bin/demiurge-rnbo-gate.sh

echo "=== install + start (gated) rnbo-runner.service ==="
sudo install -m0644 "$REPO/config/rnbo/rnbo-runner.service" /etc/systemd/system/rnbo-runner.service
sudo systemctl daemon-reload
# Still enabled at boot, but ExecCondition keeps it DOWN unless live.conf opts in
# (rnbo = on) or a *.rnbo is in the chain. enable --now is safe: it condition-skips.
sudo systemctl enable --now rnbo-runner.service
sleep 3

echo "=== restart demiurge.service onto the new launcher + live.conf ==="
sudo systemctl restart demiurge.service
sleep 4

echo ""
echo "=== STATUS ==="
echo -n "rnbo-runner: "; systemctl is-active rnbo-runner.service
echo -n "demiurge:    "; systemctl is-active demiurge.service
echo "--- runner OSCQuery up? ---"
curl -sf -o /dev/null http://localhost:5678/rnbo/info/version && echo "  yes (:5678)" || echo "  NO"
echo "--- compile supported? ---"
curl -s http://localhost:5678/rnbo/info/unsupported_cmds 2>/dev/null | python3 -c "import sys,json;print('  unsupported_cmds:',json.load(sys.stdin).get('VALUE'))" 2>/dev/null || true
echo ""
echo "deploy-wave1 done. Check audio graph with:  pw-link -l | grep -iE 'untitled|csound'"
