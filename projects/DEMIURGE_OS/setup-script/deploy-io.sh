#!/bin/bash
# deploy-io.sh — install demiurge-io, the sensor -> parameter control plane
# (run via sudo on the Pi).
#
# WHAT THIS IS. demiurge-io reads the Teensy's 100 Hz NEPTR2 serial frame at
# full resolution, conditions and routes it through ~/demiurge/sensors.json, and
# writes the result into the NEPTR engine as OSC /p on 7770. It is what makes a
# LiDAR / gyro / FSR able to drive any expression destination.
#
# IT REPLACES neptr-bridge.service. Both read /dev/ttyACM0 and only one process
# can own a CDC ACM port, so this script disables the bridge. Nothing is lost:
# the bridge existed so analog calibration could be retuned without reflashing
# the Teensy, and sensors.json now does that better (live, hot-reloaded,
# editable from the phone).
#
# IT DOES NOT TOUCH ENCODERS OR TOGGLES. Those stay on the Teensy's own direct
# USB-MIDI wire into csound (--forward-control is off by default; enabling it
# would make every encoder click register twice). Consequence, by design: if
# this daemon dies mid-set the instrument keeps playing exactly as before.
# Sensors are purely additive.
#
# SAFE ON A BARE PI. With no Teensy attached the unit's ConditionPathExists
# keeps it stopped, and the virtual panel at /neptr/panel drives the same
# pipeline over UDP 8003 instead. Nothing here fails on hardware-less hardware.
set -e
# Run via sudo, so $HOME is root's — derive the actual Pi user from whoever
# invoked sudo (falls back to the Pi OS convention of UID 1000 being the
# first/only real user, same assumption setup-demiurge.sh's "1000" sed
# substitutions already make, for the rare case this is run some other way).
PI_USER="${PI_USER:-${SUDO_USER:-$(getent passwd 1000 | cut -d: -f1)}}"
PI_USER="${PI_USER:-gorpi7}"
REPO="/home/$PI_USER/_______DEMIURGE"
USER_HOME="/home/$PI_USER"

test -d "$REPO/src/demiurge-io/demiurge_io" || {
  echo "missing $REPO/src/demiurge-io — sync the repo first"; exit 1; }

# The package runs straight from the repo checkout (stdlib only, nothing to
# build), mirroring how demiurge-web.service runs web/demiurge_web.py in place.
install -d /opt/demiurge/src
rm -rf /opt/demiurge/src/demiurge-io
cp -a "$REPO/src/demiurge-io" /opt/demiurge/src/demiurge-io

# Template User=/ExecStart= from the actual Pi user — the repo ships
# User=gorpi7 as a documented placeholder, same convention as
# config/demiurge.service (rendered by setup-demiurge.sh).
sed -e "s/^User=.*/User=$PI_USER/" \
    -e "s#^ExecStart=.*#ExecStart=/usr/bin/python3 -m demiurge_io --config $USER_HOME/demiurge/sensors.json#" \
    "$REPO/config/demiurge-io.service" > /etc/systemd/system/demiurge-io.service

# Seed a safe-boot config ONLY if absent — never clobber the player's mappings.
if [ ! -f "$USER_HOME/demiurge/sensors.json" ]; then
  install -d -o "$PI_USER" -g "$PI_USER" "$USER_HOME/demiurge"
  sudo -u "$PI_USER" python3 - <<'PY'
import json, os, sys
sys.path.insert(0, "/opt/demiurge/src/demiurge-io")
from demiurge_io.config import default_config
from demiurge_io.sources.teensy import SENSOR_DESCRIPTORS
cfg = default_config(SENSOR_DESCRIPTORS)   # every sensor declared, nothing routed
p = os.path.expanduser("~/demiurge/sensors.json")
open(p, "w").write(json.dumps(cfg, indent=2) + "\n")
print("seeded", p, "with", len(cfg["sources"]), "sensors, 0 maps")
PY
else
  echo "kept existing $USER_HOME/demiurge/sensors.json"
fi

# Retire the bridge — see the header. Tolerated if it was never installed.
systemctl disable --now neptr-bridge 2>/dev/null || true

systemctl daemon-reload
systemctl enable demiurge-io

echo
echo "IO-INSTALLED."
echo "  start:  sudo systemctl start demiurge-io   (needs /dev/ttyACM0)"
echo "  logs:   journalctl -u demiurge-io -f"
echo "  no Teensy? run it headless for the virtual panel:"
echo "     python3 -m demiurge_io --no-teensy   (from /opt/demiurge/src/demiurge-io)"
echo
echo "CHECK the Teensy's direct MIDI feed is still wired into csound:"
echo "     aconnect -l | grep -i teensy"
echo "     aconnect 'Teensy MIDI':0 14:0     # if the bridge's teardown missed it"
