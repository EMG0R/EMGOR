"""demiurge-io — sensor -> parameter control plane for the NEPTR instrument.

WHY THIS EXISTS
----------------
The Teensy's "NEPTR2" serial frame (see ../TEENSYY phase4/csound/serial_bridge.py
for the previous consumer) carries two LiDARs, a 3-axis accelerometer, two
FSRs, a joystick, two piezo mics, and the panel's encoders/buttons at 100 Hz.
serial_bridge.py squashes all of that to 7-bit MIDI CC so csound's existing
midi_handler.orc can eat it unmodified. That was the right call when MIDI was
the only transport, but it throws away resolution the hardware actually has
(a LiDAR spanning 2-46cm at 7 bits is 3.5mm/step; the sensor gives ~1mm/step
raw). csound's OSC control port (7770, see midi_handler.orc's 2026-08-12
"CONTROL INPUT TRANSPORT" section) now accepts absolute-value writes at full
float resolution via `/p <id> <val>`, alongside the same `/cc <ch> <cc> <val>`
verb the MIDI path already produces. That is what this package is for: a
sensor -> normalize -> route -> OSC pipeline that keeps the full-resolution
data intact from the ADC/LiDAR all the way to the expression-target write,
while treating the pedal (and any future joystick/gyro/phone input) as one of
potentially SEVERAL sources aimed at the same 35 destinations
(handler/expr_targets.json), through a many-to-one mapping matrix a human can
edit live in ~/demiurge/sensors.json.

Encoders and buttons are NOT part of this pipeline. They are discrete control
events (menu navigation, toggles) with their own well-understood CC mapping
that serial_bridge.py already got right; this package forwards them straight
through unchanged (see sources/teensy.py) rather than running them through
conditioning/routing built for continuous sensor data.

LAYERING (each file independently testable without hardware):
    sources/      raw driver -> Reading(source_id, raw, t)      (impure: I/O)
    conditioning  raw -> (norm 0..1, valid)                     (pure)
    router        norm -> zero or more (target, value) writes    (pure)
    sinks/        (target, value) -> wire bytes                  (impure: I/O)

Stdlib only. No numpy, no pyserial, no python-osc — see sinks/csound.py and
sources/teensy.py docstrings for why each of those is deliberately avoided.
"""

__version__ = "0.1.0"
