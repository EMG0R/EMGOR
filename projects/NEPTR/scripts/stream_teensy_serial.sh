#!/bin/bash
#
# Watch the Teensy's serial telemetry live on the laptop. The board is plugged
# into the Pi (not the laptop); this SSHes in and streams its raw serial print
# back to your terminal. No Python, no deps, no sudo.
#
#   ./scripts/stream_teensy_serial.sh                 # default Pi + /dev/ttyACM0
#   ./scripts/stream_teensy_serial.sh user@host        # override target
#   ./scripts/stream_teensy_serial.sh user@host /dev/ttyACM1   # + override port
#
# Prefers the Pi-side demiurge-stream-teensy wrapper (installed by
# setup-demiurge.sh); falls back to inline stty+cat if it isn't there. Ctrl-C
# stops cleanly (ssh -t kills the remote reader with the session).
#
# PORT CONTENTION: stop the MIDI bridge (scripts/launch_phase4.sh) first — if it
# is also reading the port, both readers split the byte stream into garbage.

set -euo pipefail

TARGET="${1:-gorpi7@demiurge.local}"
PORT="${2:-/dev/ttyACM0}"

exec ssh -t "$TARGET" \
    "command -v demiurge-stream-teensy >/dev/null 2>&1 && exec demiurge-stream-teensy '$PORT' \
     || { stty -F '$PORT' raw 115200 -echo && exec cat '$PORT'; }"
