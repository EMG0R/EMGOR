#!/usr/bin/env python3
# ~/demiurge/bridges/pd_clock_bridge.py
#
# DEMIURGE clock -> Pd bridge.
#
# Pd vanilla reads MIDI via raw ALSA (`-alsamidi`) and cannot see the
# shared `Midi Through` ALSA seq bus that every other DEMIURGE language
# listens on. This tiny sidecar bridges that gap: it subscribes to the
# same shared MIDI pool as everybody else, and forwards the clock
# broadcast (ch16 CC119) to Pd as a FUDI message over UDP.
#
# Pd patches pick it up with:
#
#     [netreceive 9997 1]   <- UDP 9997
#         |
#     [route bpm]
#         |
#         float  ->  BPM in 40..400
#
# Also forwards transport (ch16 CC117) as `transport <0|1>;`.
#
# Auto-spawned by the launcher whenever any `chain =` stage in live.conf
# is a `.pd` file (also runnable as a `run python` entry in a classic
# session file). Exits cleanly on SIGTERM so the launcher's
# graceful-shutdown loop works.
#
# Dependencies (pre-installed in /opt/demiurge/venv):
#   - mido (>= 1.3)
#   - python-rtmidi (backend for mido's ALSA seq I/O)

import os
import socket
import sys
import signal
import time

try:
    import mido
except ImportError:
    sys.stderr.write("pd_clock_bridge: mido not installed\n")
    sys.exit(1)

PORT_NAME_SUBSTR = "Midi Through"
CLOCK_CC_BPM      = 119   # broadcast
CLOCK_CC_TRANSPORT = 117  # transport
CLOCK_CHANNEL     = 15    # MIDI ch16, mido is 0-indexed
PD_HOST           = "127.0.0.1"
PD_PORT           = 9997

def find_midi_through():
    names = mido.get_input_names()
    for n in names:
        if PORT_NAME_SUBSTR.lower() in n.lower():
            return n
    return None

def fudi(sock, msg: str):
    # FUDI messages are terminated by `;\n`
    if not msg.endswith(";"):
        msg = msg + ";"
    sock.sendto((msg + "\n").encode("utf-8"), (PD_HOST, PD_PORT))

def main():
    running = [True]
    def on_sig(*_):
        running[0] = False
    signal.signal(signal.SIGINT,  on_sig)
    signal.signal(signal.SIGTERM, on_sig)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Poll until Midi Through is visible, then open it. The bridge may
    # boot before the full audio graph is up.
    port_name = None
    for _ in range(40):
        port_name = find_midi_through()
        if port_name:
            break
        time.sleep(0.25)
    if not port_name:
        sys.stderr.write("pd_clock_bridge: Midi Through not found\n")
        sys.exit(1)

    sys.stderr.write(f"pd_clock_bridge: listening on {port_name}, "
                     f"forwarding to {PD_HOST}:{PD_PORT}\n")

    with mido.open_input(port_name) as inp:
        while running[0]:
            # Use poll so SIGTERM isn't stuck behind a blocking recv.
            msg = inp.poll()
            if msg is None:
                time.sleep(0.002)
                continue
            if msg.type != "control_change":
                continue
            if msg.channel != CLOCK_CHANNEL:
                continue
            if msg.control == CLOCK_CC_BPM:
                # Range is 40..400, NOT 40..240: demiurge-clock.cpp encodes
                # the broadcast as bpm = 40 + (val/127)*360 (see
                # demiurge-clock.cpp:372 and demiurge-clock-pll.h's
                # CLOCK_BPM_OUT_MIN/MAX). Decoding at 40..200 misread every
                # tempo (a real 124 BPM arrived as ~86.7). The equivalent
                # bug was already fixed on the csound side 2026-07-14 (see
                # neptr/csound/midi_handler.orc instr 99) — same fix,
                # same reasoning, applied here so it doesn't regress.
                bpm = 40.0 + (msg.value / 127.0) * 360.0
                fudi(sock, f"bpm {bpm:.2f}")
            elif msg.control == CLOCK_CC_TRANSPORT:
                play = 1 if msg.value >= 64 else 0
                fudi(sock, f"transport {play}")

    sock.close()
    sys.stderr.write("pd_clock_bridge: exiting cleanly\n")

if __name__ == "__main__":
    main()
