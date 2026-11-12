#!/usr/bin/env bash
# install_devices.sh -- copies the built .amxd devices (max/build) + their js helpers to the Ableton User Library and Max 9/Library.
#   core (instrument)  -> Presets/Instruments/Max Instrument/PMOR/
#   lanes (MIDI)       -> Presets/MIDI Effects/Max MIDI Effect/PMOR/
#   js helpers         -> next to every .amxd and ~/Documents/Max 9/Library (Max search path)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
U="$HOME/Music/Ableton/User Library/Presets"; L="$HOME/Documents/Max 9/Library"
I="$U/Instruments/Max Instrument/PMOR"; M="$U/MIDI Effects/Max MIDI Effect/PMOR"
mkdir -p "$I" "$M" "$L"
cp "$here/build/PMOR core.amxd" "$I/"
for f in "$here"/build/PMOR\ lane*.amxd; do cp "$f" "$M/"; done
for j in "$here"/devices/*.js "$here"/devices/schema.json; do cp "$j" "$I/"; cp "$j" "$M/"; cp "$j" "$L/"; done
echo "installed: $I  $M  (+ js in $L)"
