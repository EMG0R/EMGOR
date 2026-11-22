# NeoTrellis pad and control map (test 10_delay, bringup-1.12)

Final map from the user, 2026-11-17. Pad n = Trellis key n-1 (pad 1 top-left, key = row*4 + col). v1 pads are exactly as audited in `docs/V1_MAP_AUDIT.md`; the pads v1 left empty are on/off latches for the effects v1 only had on sensors. **Sensors never switch an effect on**: a sensor only drives a param of an effect that is already on. Code: `fw/fx_pads.h` (pads), `fw/fx_controls.h` (sensors, faders, encoders), `fw/fx_glue_chain.h` (chain order, `kFaderFx`).

## FX page (default)
| Pad | Function | Mode | v1? | Colour |
|---|---|---|---|---|
| 1 | flanger on/off; lidar (inverted) drives its mix while on | latch | new | blue |
| 2 | roar on/off; pot 1 drives DRIVE | latch | new (v1 pad 2 was dead) | orange |
| 3 | freeze CLEAR: FREEZE factor ramps 100 -> 0 over 2.3 s, then freeze off | trigger | v1 | ice |
| 4 | freeze LET-IN: freeze on, input gate open while held | hold | v1 | ice |
| 5 | ringmod on/off (ringmod keeps its own FREQ; the FSR no longer drives it) | latch | v1 | amber |
| 6 | MOOD on/off; the joystick drives transpose index (X) and probability (Y) | latch | new | purple |
| 7 | blur latch | latch | v1 | violet |
| 8 | chorus latch | latch | v1 | cyan |
| 9 | delay on | latch | v1 | indigo |
| 10 | delay smooth-mode (glide) toggle | latch | v1 | indigo |
| 11 | delay preset A: feedback 0.787, mix 1.0 (mix ramp ~800 ms) | latch | v1 | indigo |
| 12 | delay preset B: feedback 0.24, mix 0.31 (the FSR sets the time, see below); 9, 11, 12 exclusive | latch | v1 | indigo |
| 13 | looper: tap record / overdub / punch out, hold 500 ms = delete all | tap / hold | v1 | near-white |
| 14 | looper: tap play / stop, hold 500 ms = stop | tap / hold | v1 | near-white |
| 15 | autopan on/off; lidar drives depth and period (200..600 ms) | latch | new | mint |
| 16 | wah on/off | latch | new | green |

Latch pads toggle on release (< 600 ms); a press of 600 ms or more focuses that effect's panel in the web UI. LEDs: each effect's colour, dim when off, bright when on (breathing), fading while its tail runs, press flash, red blink = refused by admission control (projected total peak above 65 % using the device-measured peak table, or the effect's own run-time peak if higher; a runtime guard additionally sheds the most recently enabled effect when 8 of 32 callbacks exceed 75 % or one exceeds 90 %, and blinks its pad red).

## HARMONY page (either encoder button toggles it; all effects keep running, only the pad meaning swaps)
Intervals in semitones above the input. LEDs use a separate palette: amber (row 1), coral (row 2), magenta (row 3), cyan (row 4); dim = free, bright = voice.
| | col 1 | col 2 | col 3 | col 4 |
|---|---|---|---|---|
| row 1 | 2 (+2) | b3 (+3) | 3 (+4) | 4 (+5) |
| row 2 | b5 (+6) | 5 (+7) | #5 (+8) | 6 (+9) |
| row 3 | b7 (+10) | 7 (+11) | b9 (+13) | 9 (+14) |
| row 4 | 11 (+17) | #11 (+18) | b13 (+20) | 13 (+21) |
Pads latch on the press; any combination, but at most 3 voices sound: a 4th press steals the oldest. Voices fade in and out in about 20 ms; the harmony effect (`fx_harmony.h`, 3 voices on the shared pitch-synchronous shifter core, dry unchanged) switches on with the first voice and off after the last one's fade. Encoder 1 rotation sets the harmony mix (0.05 per detent) on this page and does nothing on the FX page.

## Faders, FSR, accel
- Fader table `kFaderFx`: 1 reverb, 2 drone, 3 grain. **Exclusive**: only one of reverb, grain, drone at a time. The last fader to rise above 0 wins; the active one fades out (its tail runs) and goes idle. A displaced effect cannot come back until its fader has been at 0 once, so a parked fader does not fight the others. `state` shows `ex:[owner, blocked-mask]`.
- FSR: delay time ALWAYS = 600 ms x sqrt(FSR), minimum 48 ms (absDelay `scale 0. 1. 0. 60. 0.5`); the delay repitches tape-style (glide = SMOOTH, 3 ms when smooth mode is off). Pad 12's v1 time of 412 ms is only its starting value.
- Accel Y -> reverb warp (v1 curve), feedback fixed.

## Not present
Pad hardware absent (no power): the firmware keeps running without it; the Trellis is probed again with a back-off of 150 ms (first 3 s) up to 5 s, and on power-up it is re-initialised (keypad events + LEDs) and the whole LED buffer is redrawn.

---

## padfx-4.9 (TEST=14_padfx): harmony page, looper hold, freeze

### Harmony page (momentary: hold = voice on, release = off, at most 3 voices, the 4th press steals the oldest)

Close voicings for a monophonic trumpet with up to three added voices. Pad 1 is top-left; the intervals are semitones from the played note.

| | col 1 | col 2 | col 3 | col 4 |
|---|---|---|---|---|
| row 1 | -12 octave below | -10 minor 7th below | -9 major 6th below | -8 minor 6th below |
| row 2 | -7 fifth below | -5 fourth below | -4 major 3rd below | -2 major 2nd below |
| row 3 | +2 sus2 / add9 | +3 minor 3rd | +4 major 3rd | +5 sus4 |
| row 4 | +7 fifth | +9 sixth | +10 minor 7th | +12 octave |

Why this order and these intervals: the low intervals sit on rows 1-2 (top), the colour tones above them on rows 3-4. The fifth and fourth below (-7, -5) stack into quartal / sus voicings under the line, +2 and +5 are the sus2 / sus4 colour tones, and any two or three of these sit within an octave and a fourth or fifth apart, so they never produce a minor-second cluster (the -1 / +1 / +6 pads of the old table did). The old table (-10 ... +9, minus the octave) is replaced.

Shifter quality (host test `bringup/fw/tests/harm_voicing_test.cpp`, trumpet-like input, ctx.f0 = 0 as the padfx chain runs it): every voice is within 7 cents of target for the down shifts, all 16 intervals have the same level (spread 0.1 dB, -3.1 dB re dry at MIX 1 / VOICE 0.7); the larger readings at +9 / +10 on a 523 Hz input are the autocorrelation's 34-cent lag step, not a shifter error. No change to `fx_harmony.h` was needed.

### Looper (pad 12)

A press of 350 ms (`LOOP_HOLD_MS`) or longer is a HOLD. A normal click lasts 60-180 ms, so 350 ms is about twice the longest ordinary tap: a hold is recognised quickly (the pad starts fading at once) and a tap can never reach it. The delete still needs the finger kept down through the 1 s fade-out; releasing during the fade cancels it, so an accidental long press loses nothing. Click windows: double click 300 ms (first pass only).

### Freeze (pads 9 gate / 10 reset)

Root cause of "freeze broken on padfx-4.8": the phase-cloud freeze needs about 95 KB of HOT memory (host measurement, `tests/freeze_slot_test.cpp`: ok = 0 at 40 and 64 KB, ok = 1 at 96 KB, plus 50.7 KB cold in SDRAM) but the shared AXI slot was 40 KB, so the freeze constructor failed and the pad refused it. The slot is now 96 KB (paid for by trimming the drone arenas from 90 KB to 58 KB: `fx_glue_drone_pfx.h`). Default GAIN 2.0 and MIX 0.9, because the new freeze is about 7 dB quieter than the old one: a frozen -30 dBFS tone reads -30.6 dBFS rms at the output (host, `tests/freeze_host_test.cpp`), and the 300 ms clear ramp takes it to -55 dBFS.
