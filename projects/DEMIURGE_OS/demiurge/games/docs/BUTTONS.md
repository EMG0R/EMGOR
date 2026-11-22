# Canonical System Button Map

This is the **authoritative button map for the entire NEPTR / Demiurge system**
(synth side AND arcade-console side). It documents the fixed *role* of each
physical footswitch button. Any other button reference that disagrees with this
is wrong and should be corrected to match.

## The footswitch buttons

The Teensy footswitch buttons are named `b1`..`b9` in the firmware and are emitted
as MIDI Control Change `CC60`..`CC68` — i.e. `b{n}` = `CC{59 + n}`
(pressed = 127, released = 0, MIDI channel 1).

| Firmware | CC | Role | Cluster |
|---|---|---|---|
| b1 | 60 | **ARCADE** button (also = fire / primary action) | center / big |
| b4 | 63 | **UP**    | left d-pad |
| b2 | 61 | **DOWN**  | left d-pad |
| b5 | 64 | **LEFT**  | left d-pad |
| b3 | 62 | **RIGHT** | left d-pad |
| b9 | 68 | **A** | right face |
| b8 | 67 | **B** | right face |
| b6 | 65 | **X** | right face |
| b7 | 66 | **Y** | right face |

## Layout / clusters

```
        LEFT cluster (4-way D-PAD)        CENTER         RIGHT cluster (face)

                 b4 (UP)                                       b9 (A)
                                          b1 (ARCADE)
       b5 (LEFT)      b3 (RIGHT)                        b6 (X)        b8 (B)
                                        fire / primary
                 b2 (DOWN)                                     b7 (Y)
```

- **LEFT cluster = a 4-way D-PAD:** b4=UP, b2=DOWN, b5=LEFT, b3=RIGHT.
- **RIGHT cluster = face buttons:** b9=A, b8=B, b6=X, b7=Y.
- **CENTER = the big ARCADE button (b1)** — the primary action / fire.

## Where each consumer reads these

- **Synth (csound):** the NEPTR synth chain does NOT consume the footswitch
  buttons CC60–68 at all. It drives everything off the seven encoders (rotations
  CC20–26, buttons CC70–76) plus the analog sensors. The button map above is
  documented in the synth tree for completeness only
  (`NEPTR phase4/TEENSYY/midi_reference/SENSOR_MAP.md`,
  `NEPTR phase4/csound/midi_handler.orc` header).

- **Arcade console (`demiurge/games/`):** the supervisor + `lib/gamepad.py` read
  the buttons. NOTE the current console wiring differs from the canonical
  physical roles above — see the caveat below. Do not "fix" the console logic to
  match this table; the table is the *physical hardware* reference, the console
  code is a separate concern.

## Console-wiring caveat (current `lib/gamepad.py` behaviour)

In **console mode** the arcade supervisor does NOT use b2–b5 for the d-pad.
Instead, console-mode movement comes from the **analog joystick axes** (`jx`/`jy`),
because arcade games want a real 4-way stick. The buttons it does read in console
mode:

- `b1` (ARCADE) → fire / primary action (RetroPad A *and* B).
- `b9` → also A (fire), `b8` → also B.
- `b4` (canonically UP) → Start + Coin (free-play / continue-on-death).
- `jb` (joystick click) → reserved for the `b1`+`jb` console-open/close handshake;
  deliberately NOT a game control.

So the canonical *physical* roles (b2–b5 = d-pad, b6/b7 = X/Y) are not all wired
on the arcade-console side today. This is intentional for a one-stick + one-button
cabinet; the table above remains the source of truth for what each button *is*.
