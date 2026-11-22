#!/bin/bash
# Compile + flash the NEPTR body Teensy 4.1 from PRIME, then restore the rig.
#   flash-body.sh                 compile ~/DEMIURGE_OS_main/neptr/TEENSYY and flash
#   flash-body.sh <file.hex>      flash a prebuilt hex (e.g. ~/MCU/build/TEENSYY_rollback_*/)
# Only with EMGOR's word for that flash. Toolchain lives in ~/MCU (arduino-cli,
# teensy:avr 1.59.0, VL53L1X lib); config ~/MCU/arduino-cli.yaml.
#
# Why each step:
#  - prime-watch paused: the body dropping off USB must not trigger recovery.
#  - neptr-bridge stopped: it holds the CDC port; 134 baud on that port is the
#    Teensy soft-reboot-to-bootloader signal.
#  - after boot, the legacy udev one-shot used to wire Teensy MIDI into 14:0
#    (doubled encoders in csound). Fixed in demiurge-midi-connect.sh 2026-11-05;
#    this script still removes that link and restores Teensy -> UI as a guard.
set -euo pipefail
MCU=~/MCU
SRC=~/DEMIURGE_OS_main/neptr/TEENSYY
DEV=/dev/serial/by-id/usb-Teensyduino_Teensy_MIDI_19910760-if00
HEX="${1:-}"

if [ -z "$HEX" ]; then
  nice -n 19 "$MCU/bin/arduino-cli" --config-file "$MCU/arduino-cli.yaml" compile \
    --fqbn teensy:avr:teensy41:usb=serialmidi --warnings all \
    --output-dir "$MCU/build/TEENSYY" "$SRC"
  HEX="$MCU/build/TEENSYY/TEENSYY.ino.hex"
fi
[ -f "$HEX" ] || { echo "no hex: $HEX"; exit 1; }

sudo touch /run/prime-watch.pause
trap 'sudo systemctl start neptr-bridge; sudo rm -f /run/prime-watch.pause' EXIT
sudo systemctl stop neptr-bridge
sleep 1
stty -F "$DEV" 134 || true
sleep 1
timeout 60 teensy_loader_cli --mcu=TEENSY41 -w -v "$HEX"

for _ in $(seq 20); do [ -e "$DEV" ] && break; sleep 0.5; done
sleep 2
teensy=$(aconnect -l | sed -n "s/^client \([0-9]*\): 'Teensy MIDI'.*/\1/p")
ui=$(for c in $(aconnect -l | sed -n "s/^client \([0-9]*\): 'RtMidiIn Client'.*pid=\([0-9]*\)\]/\1:\2/p"); do
       pid=${c#*:}; grep -q neptrPhase4_UI "/proc/$pid/cmdline" 2>/dev/null && { echo "${c%%:*}"; break; }
     done)
if [ -n "$teensy" ]; then
  aconnect -d "$teensy:0" 14:0 2>/dev/null || true
  aconnect -d 14:0 "$teensy:0" 2>/dev/null || true
  [ -n "$ui" ] && aconnect "$teensy:0" "$ui:0" 2>/dev/null || true
fi
aconnect -l | sed -n "/'Teensy MIDI'/,/^client/p" | head -3
echo "flashed $HEX"
