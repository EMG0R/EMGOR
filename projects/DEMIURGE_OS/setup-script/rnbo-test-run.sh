#!/bin/bash
# rnbo-test-run.sh — RNBO integration, Phase 0 / Step 3.
#
# Transiently launches the runner as a pipewire-jack CLIENT (Demiurge's
# shim env), with auto-connect OFF and NO patch loaded (-d). Verifies it
# does NOT spawn a jackd and does NOT disturb PipeWire, then kills it.
# Self-cleaning. Runs as the normal user — NO sudo.
#
#   bash rnbo-test-run.sh
set -uo pipefail

CFG=/tmp/rnbo-demiurge.json
LOG=/tmp/rnbo-runner.log
export XDG_RUNTIME_DIR="/run/user/$(id -u)"

# Demiurge runner config: default schema with the 4 ownership keys flipped
# so DEMIURGE controls routing, not the runner.
cat > "$CFG" <<JSON
{
    "backup_dir": "$HOME/Documents/rnbo/backup/",
    "compile_cache_dir": "$HOME/Documents/rnbo/cache/so",
    "control_auto_connect_midi": false,
    "datafile_dir": "$HOME/Documents/rnbo/datafiles/",
    "db_path": "$HOME/Documents/rnbo/oscqueryrunner.sqlite",
    "instance_audio_fade_in": 20.0,
    "instance_audio_fade_out": 20.0,
    "instance_auto_connect_audio": false,
    "instance_auto_connect_audio_indexed": false,
    "instance_auto_connect_midi": false,
    "instance_auto_connect_midi_hardware": false,
    "instance_auto_connect_port_group": true,
    "instance_auto_start_last": false,
    "instance_port_to_osc": true,
    "package_dir": "$HOME/Documents/rnbo/packages/",
    "patcher_midi_program_change_channel": "none",
    "preset_midi_program_change_channel": "none",
    "recording_dir": "$HOME/Documents/rnbo/datafiles/",
    "save_dir": "$HOME/Documents/rnbo/saves/",
    "set_midi_program_change_channel": "none",
    "set_preset_default_patcher_named": false,
    "set_preset_midi_program_change_channel": "none",
    "source_cache_dir": "$HOME/Documents/rnbo/cache/src",
    "uuid_path": "$HOME/.config/rnbo/runner-id.txt"
}
JSON
echo "config written: $CFG"

echo "=== baseline (before launch) ==="
pgrep -xa jackd || echo "  no jackd (good)"
echo -n "  pipewire: "; systemctl --user is-active pipewire pipewire-pulse wireplumber | tr '\n' ' '; echo

echo "=== sourcing DEMIURGE env (pipewire-jack shim) ==="
source /opt/demiurge/lib/demiurge-env.sh
echo "  LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

echo "=== launching runner (-d: no patch) under the shim ==="
JACK_NO_AUDIO_RESERVATION=1 rnbooscquery -c "$CFG" -d > "$LOG" 2>&1 &
PID=$!
echo "  pid=$PID; waiting 6s..."
sleep 6

echo "=== runner alive? ==="
if kill -0 "$PID" 2>/dev/null; then echo "  yes (pid $PID)"; else echo "  NO — it exited; see log below"; fi

echo "=== registered on the PipeWire/JACK graph? ==="
pw-cli ls Node 2>/dev/null | grep -iA1 'rnbo\|RNBO' || echo "  (no rnbo node via pw-cli)"
pw-link -o 2>/dev/null | grep -i rnbo || echo "  (no rnbo output ports yet — expected with -d / no patch)"

echo "=== OSC (1234/udp) + HTTP (5678/tcp) listening? ==="
ss -lntu 2>/dev/null | grep -E ':1234|:5678' || echo "  (no 1234/5678 listener)"

echo "=== did it spawn a jackd? (MUST be none) ==="
pgrep -xa jackd || echo "  no jackd (good)"

echo "=== pipewire still healthy? ==="
echo -n "  "; systemctl --user is-active pipewire pipewire-pulse wireplumber | tr '\n' ' '; echo

echo "=== runner log ==="
sed 's/^/  | /' "$LOG"

echo "=== cleanup ==="
kill -INT "$PID" 2>/dev/null; sleep 1; kill -9 "$PID" 2>/dev/null
if pgrep -xa rnbooscquery >/dev/null; then echo "  WARNING: runner still running"; pgrep -xa rnbooscquery; else echo "  runner stopped"; fi
pgrep -xa jackd && echo "  WARNING: jackd left running — kill it" || echo "  no jackd left behind (good)"
