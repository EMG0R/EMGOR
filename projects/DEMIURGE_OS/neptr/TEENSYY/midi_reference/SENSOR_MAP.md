# NEPTR Phase 4 — Teensy sensor → MIDI CC reference

This is the mapping the MIDI version of the firmware emitted on channel 1.
The new serial-output firmware (`TEENSYY/TEENSYY.ino`) carries the same
sensors but uses a structured frame; the `serial_bridge.py` daemon on the
Pi re-emits these exact CCs so csound's `midi_handler.orc` keeps working
unchanged.

Every CC is on **MIDI channel 1**, 7-bit (0–127).

## Encoders (rotation)

| Encoder | CC | Direction emission |
|---|---|---|
| Enc1 | 20 | CW click → CC20 val=127, CCW click → CC20 val=0 |
| Enc2 | 21 | same scheme |
| Enc3 | 22 | same |
| Enc4 | 23 | same |
| Enc5 | 24 | same |
| Enc6 | 25 | same |
| Enc7 | 26 | same |

Encoders are **relative**, not absolute. The new firmware accumulates the
direction delta per frame; the bridge re-emits one CC event per clicked
detent so csound's `$ENC_ROT(...)` macros stay valid.

*Direction is flipped in the new firmware* — the previous direction felt
backwards to the user. CW now reads CW.

## Buttons (digital, momentary)

| Button | CC | Notes |
|---|---|---|
| BTN1–9 | 60–68 | pressed = 127, released = 0 |
| EncBtn1 | 70 | Enc1 button |
| EncBtn2 | 71 | Enc2 button |
| EncBtn3 | 72 | Enc3 button |
| EncBtn4 | 73 | Enc4 button |
| EncBtn5 | 74 | Enc5 button |
| EncBtn6 | 75 | Enc6 button |
| EncBtn7 | 76 | Enc7 button |
| JoyBtn | 77 | joystick click |

## Analog continuous

| Sensor | CC | Teensy raw | MIDI scale | Notes |
|---|---|---|---|---|
| Joystick X | 27 | 0–4095 (12-bit) | 0–127 linear | smoothed α=0.1 |
| Joystick Y | 28 | 0–4095 | 0–127 linear | smoothed α=0.1 |
| Expression pedal (TRS) | 29 | 0–4095 | 0–127 mapped from raw 128–3992 (calibrated) | the new 12-bit ADC gives ~32× the resolution of the old 10-bit reading; bridge does final 7-bit MIDI scaling |
| FSR1 | 34 | 0–4095 | 0–127 with `(smooth/4095)^2` exponential curve | foot-pressure response feels linear-perceptual |
| FSR2 | 35 | 0–4095 | 0–127 same curve | |
| Mic L (piezo) | 78 | 0–4095 envelope | 0–127 mapped 0–800 (was 0–200 in 10-bit) | asymmetric attack/release follower |
| Mic R (piezo) | 79 | 0–4095 envelope | 0–127 mapped 0–800 | same |

## Distance / IMU

| Sensor | CC | Raw | MIDI scale |
|---|---|---|---|
| LiDAR 1 (Wire) | 30 | mm 0–2000 | 0–127 mapped 2–46 cm |
| LiDAR 2 (Wire2) | 36 | mm 0–2000 | 0–127 mapped 2–46 cm |
| Accel X | 31 | ±17000 (MPU 16-bit / ±2g) | 0–127 centered at 64 |
| Accel Y | 32 | ±17000 | 0–127 |
| Accel Z | 33 | ±17000 | 0–127 |

## How csound uses these

The dispatch logic lives in `csound/midi_handler.orc` (instr 99). High-level
map (more detail in that file's header):

- **CC20** — secondary param, dispatched by `gkLastToggle`
- **CC21, CC22, CC24, CC25, CC26** — slot encoder rotations (per-menu effect)
- **CC23** — tertiary param, `gkLastToggle`-dispatched
- **CC29** — expression pedal, `gkLastToggle29`-dispatched (sticky)
- **CC70–CC76** — encoder buttons → menu nav + effect toggles
- **CC119 ch16** — master BPM from DEMIURGE (not from Teensy)

## How the bridge translates serial → MIDI

The new firmware emits one structured frame per 10 ms (100 Hz):

```
NEPTR2 t=<ms> e1=<delta> e2=<delta> ... b1=0|1 ... eb1=0|1 ... jb=0|1
       jx=<12bit> jy=<12bit> exp=<12bit>
       fsr1=<12bit> fsr2=<12bit>
       tof1=<mm|-1> tof2=<mm|-1>
       ax=<i16> ay=<i16> az=<i16>
       ml=<envelope> mr=<envelope>
```

`serial_bridge.py` parses each frame and:

- For each non-zero encoder delta, emits |delta| repeats of the matching CC
  with val=127 (positive delta = CW) or val=0 (negative).
- For each button whose state changed since the last frame, emits the
  matching CC with val=127 (pressed) or val=0 (released).
- For every analog, computes the 7-bit MIDI value using the scaling in the
  table above and emits the CC **only when the 7-bit value changes** — same
  "scaled-change-only" rule the MIDI firmware used, preserves csound's
  expectation that CC traffic is sparse.

The Teensy's higher-resolution 12-bit reading gives smoother, less noisy
analog values that the bridge then quantizes to 7-bit MIDI. Net effect on
the csound side: same protocol, much cleaner input.
