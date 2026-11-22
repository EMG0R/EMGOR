#!/bin/bash
# Auto-connect all physical MIDI sources to demiurge-midi (Midi Through, client 14)
sleep 1
# Teensy MIDI (NEPTR body) and Bela are read by the UI / serial bridge, never by the
# hub: wiring them into 14:0 doubles every encoder click in csound. Same exclusion
# as demiurge-midi-connect (the service). 2026-11-05.
aconnect -l | grep -P '\[type=kernel,card' | grep -vE "'[^']*(Teensy MIDI|Bela)[^']*'" | grep -oP 'client \K\d+' | while read client; do
    aconnect "$client":0 14:0 2>/dev/null || true
    # Mirror: also connect the bus BACK to the device so USB MIDI gear
    # (e.g. the Pocket OpGorator) hears 0xF8 clock ticks and the
    # CC116/CC119 broadcasts, not just sends into the bus. Idempotent —
    # a duplicate aconnect errors harmlessly.
    aconnect 14:0 "$client":0 2>/dev/null || true
done
