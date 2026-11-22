# NEPTR Phase 4 — Teensy serial → MIDI bridge

`serial_bridge.py` replaces the old direct USB-MIDI path between the Teensy
and csound. The Teensy now emits structured serial frames (one per 10 ms,
documented at the top of `TEENSYY/TEENSYY.ino`); this bridge parses them and
re-emits the same MIDI CC events the old MIDI firmware produced. csound's
`midi_handler.orc` is unchanged.

Why the indirection: the Teensy serial format carries 12-bit ADC values and
documented field names instead of opaque CC numbers, so any sensor mapping
re-tune happens here in Python (no Teensy reflash needed).

## Mapping summary

See `TEENSYY/midi_reference/SENSOR_MAP.md` for the full sensor → CC table.
The bridge mirrors it exactly.

## Dependencies

Already installed by `setup-demiurge.sh` (Phase 1):
- `python3-mido`
- `python3-rtmidi`

The bridge reads the Teensy USB-CDC port using only the Python standard
library (`os` / `termios` / `select`) — see "Why not pyserial" below — so
`python3-serial` is NOT required.

## Why not pyserial

The Teensy enumerates as USB type "Serial + MIDI", so it presents both a
USB-MIDI device (`Teensy MIDI MIDI 1`) and a CDC ACM serial port
(`/dev/ttyACM0`). pyserial's POSIX backend treats the Teensy's intermittent
zero-length CDC read URBs as a fatal "device disconnected or multiple access
on port" and raises `SerialException` on every read — so a pyserial-based
bridge delivers zero frames even though `cat /dev/ttyACM0` streams them fine.

The bridge therefore opens the port the same way `cat` does: a raw file
descriptor (`os.open`), put into raw mode at 115200 8N1 via `termios`, read
with `select` + `os.read`. Empty/short reads are tolerated, not treated as
disconnects. This is robust and dependency-free.

## Manual run (for testing)

```sh
python3 ~/NEPTR_phase4/csound/serial_bridge.py /dev/ttyACM0
```

You should see:
```
[bridge] /dev/ttyACM0 @ 115200 → ALSA-seq port 'NEPTR Serial Bridge'
```

Then in another terminal, hook the bridge's MIDI output to Midi Through
(client `14:0` is where DEMIURGE auto-routes everything and csound listens):
```sh
aconnect "NEPTR Serial Bridge":0 14:0
```

Watch CCs flow:
```sh
aseqdump -p 14:0
```

(Move the joystick / spin an encoder — you should see CC events on channel 1
with the numbers documented in `SENSOR_MAP.md`.)

## Note: the Teensy also still sends USB-MIDI directly

The currently-flashed firmware emits BOTH the NEPTR2 serial frames AND the
legacy USB-MIDI CCs (ALSA client `Teensy MIDI`, auto-routed to `14:0`). If
you run this bridge while that direct path is also connected, csound will
receive each sensor twice. Pick one:
- Bridge path (recommended, re-tunable in Python): disconnect the direct
  Teensy MIDI client, e.g. `aconnect -d "Teensy MIDI":0 14:0`, and let the
  bridge feed `14:0` instead.
- Legacy direct path: don't run the bridge.

The bridge does not touch the direct MIDI routing itself; manage it via
`aconnect` (or the launcher) so the two agents' responsibilities stay
separate.

## Systemd service (recommended)

Drop the following at `/etc/systemd/system/neptr-bridge.service` (file is in
this folder as `neptr-bridge.service`):

```ini
[Unit]
Description=NEPTR Teensy serial → MIDI bridge
After=sound.target user@1000.service
Requires=user@1000.service
# Wait for the Teensy USB device to appear
ConditionPathExists=/dev/ttyACM0

[Service]
Type=simple
# gorpi7 = (insert your name here, this is mine) — Pi user from Raspberry Pi
# Imager. Replace below if yours is different.
User=gorpi7
Group=gorpi7
Environment=HOME=/home/gorpi7
Environment=XDG_RUNTIME_DIR=/run/user/1000
ExecStart=/usr/bin/python3 /home/gorpi7/NEPTR_phase4/csound/serial_bridge.py /dev/ttyACM0
ExecStartPost=/bin/sh -c 'sleep 1 && aconnect "NEPTR Serial Bridge":0 14:0 || true'
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
```

Enable + start:
```sh
sudo systemctl daemon-reload
sudo systemctl enable --now neptr-bridge.service
journalctl -u neptr-bridge -f
```

## Reflashing the Teensy

The bridge holds `/dev/ttyACM0` open. Before reflashing:
```sh
sudo systemctl stop neptr-bridge.service
demiurge-flash-teensy ~/firmware.hex
sudo systemctl start neptr-bridge.service
```

Or simpler: the bridge auto-reconnects on serial errors (reopen loop).
Flashing will drop the port briefly but the bridge will pick it back up when
the Teensy re-enumerates after HalfKay boots the new firmware.

## When you change the sensor → CC mapping

All scaling lives in `serial_bridge.py`'s `ANALOG_MAP` table and the
`*_scale()` helpers. Edit, restart the service. No reflash, no csound
restart.

## Higher fidelity upgrade path (future)

The bridge currently scales 12-bit Teensy values down to 7-bit MIDI. To
preserve the full 12 bits for `fsr1`, `fsr2`, `exp` (the user-requested
"highest fidelity" channels), the bridge can be extended to send 14-bit
MIDI pairs (MSB + LSB on CC+32) and the corresponding csound handlers
swapped to `ctrl14`. Not done yet — Teensy-side noise reduction and the
100 Hz update rate already deliver most of the perceptual win.
