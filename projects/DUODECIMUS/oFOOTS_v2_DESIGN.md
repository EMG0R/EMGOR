# DUODECIMUS — design spec

Date: 2026-09-04
Board: Teensy 4.1 (confirmed — v1 uses A15, which exists only on the 4.1)
Supersedes: `v1/foots/foots.ino`

Wiring and bring-up: see `BUILD.md`.
Firmware: `foots2/foots2.ino` — compiles clean, zero warnings.

---

## 1. Configuration

| | |
|---|---|
| Encoders | 12, direct GPIO, all with push-switches |
| NeoPixels | 12, one per encoder |
| Display | 1.5" GME128128-01 OLED, **SH1107** controller, on I2C. No TFT. |
| Pedal jacks | 2, both auto-detecting sustain or expression |
| Joystick | none |
| Arcade button | none |
| microSD | not used |
| Pin usage | 41 of 42 top-side, plus bottom pads 44 and 45. Pin 12 spare. |
| v1 wiring | preserved. Exactly one existing wire moves. |

Nothing is on an expander, a shift register, or a resistor ladder. Every
switch is a direct GPIO read.

---

## 2. Preserving the v1 wiring

Twelve encoders with switches is thirty-six pins. Add the LED line, the I2C
pair, and two jacks at two pins each and the total is forty-three. The Teensy
4.1's top side has forty-two.

The map is built around keeping v1's harness intact rather than around
minimising pad count. All ten original encoders keep their exact v1 pins, and
the expression pedal's tip stays on 39.

**Exactly one existing wire moves: NeoPixel data, 38 to 13.** Pin 38 is
analog-capable and jack A needs it for ring sense. Pin 13 was the TFT's clock
line, so it came free when the TFT was removed.

Removing the TFT freed pins 8 through 13, which is exactly enough for the two
new encoders plus the relocated LED line, with pin 12 left spare.

The OLED goes on pads 44 and 45, the Wire1 bus's alternate pins. Every on-top
I2C pair is held by an original encoder: 18 and 19 by encoder 4, 16 and 17 by
encoders 3 and 4, 24 and 25 by encoder 6. Using any of them would mean
rewiring three connections instead of soldering two pads.

Pads are **digital only**, which is why they carry I2C and never a pedal jack.
Ring detection depends on reading a mid-scale voltage, and on a digital pin an
expression pedal and a sustain pedal are indistinguishable.

Alternatives considered:

| Option | Cost |
|---|---|
| Nothing moves at all | Possible, but jack A stays expression-only, no auto-detect |
| Zero bottom pads | Possible, but encoder 6 rewires and jack A stays expression-only |
| Keep the TFT | Needs six more pins, and there is nowhere for them |
| Resistor ladder for switches | Cannot detect two switches pressed at once |
| MCP23017 expander | Rejected as an added chip |

---

## 3. Jack detection

Sleeve to ground, ring and tip each to their own analog pin, no resistors.

An expression pedal is a pot on a stereo plug: sleeve one end, ring the other,
tip the wiper. A sustain pedal is a switch on a **mono** plug, which carries no
ring conductor at all, so the plug barrel covers and grounds the jack's ring.

Ring is read with the internal pull-up every 250 ms:

| Ring reading | State |
|---|---|
| above 3800 of 4095 | nothing plugged in |
| below 700 | mono plug → sustain |
| in between | stereo plug → expression |

The middle case is the ~22k internal pull-up in series with the pedal's pot,
which lands near 1280 for a 10k pedal and near 3355 for a 100k one. Both sit
clear of either threshold.

Once expression is confirmed, ring becomes an output driven high, so the pedal
sees the full rail and the tip gives its whole sweep. Probing continues by
briefly releasing ring, sampling, and restoring it, which is why hot-swap
works in both directions.

**Ring must never be tied to a power rail.** A sustain plug would short it to
ground through the barrel.

Sustain polarity is captured at plug-in, so normally-open and normally-closed
pedals both work with no setting. Expression range self-calibrates from
observed travel.

---

## 4. Control scheme

The joystick and arcade button are gone, so page navigation and the mode
commands live on encoder buttons. Encoder 1's button is SHIFT.

| Action | Result |
|---|---|
| SHIFT + turn encoder 1 | page |
| SHIFT + turn encoder 12 | bank |
| SHIFT + press encoder 2 | accumulator on/off |
| SHIFT + press encoder 3 | invert range |
| SHIFT + press encoder 4 | save bank |
| SHIFT + press encoder 5 | load bank |
| Tap encoder 1 alone | its normal toggle |

This is what replaces v1's `knobMode` and `reverseButtonCC`, both of which were
three-second two-encoder holds that blocked the loop and were undiscoverable.

Direct GPIO is what makes this possible. A resistor ladder cannot see two
buttons at once, which would have forced long-presses instead of combos.

---

## 5. MIDI map

Page selects the MIDI channel. Eight pages by twelve encoders is 192
assignments and only 128 CC numbers exist, so a single-channel table cannot be
extended from v1's eight encoders to twelve.

| | |
|---|---|
| Channel | page, 1 through 8 |
| Rotation | CC 20 through 31 |
| Button | CC 40 through 51 |
| Expression | CC 11, channel 1 |
| Sustain | CC 64, channel 1 |

This is a breaking change from v1's channel-10 CC table. Existing host
mappings will need redoing.

---

## 6. Firmware rework

v1 problems fixed:

1. **Blocking delays.** `delay(50)` in debounce, `delay(500)` in `flashConfirm`,
   `delay(5)` in the loop. All stalled MIDI and dropped encoder steps. Now
   fully non-blocking on millis.
2. **Half-quadrature decoding.** v1 read only CLK edges and sampled DT, missing
   and double-counting steps. Replaced with a full quadrature state machine
   that accumulates four transitions per detent.
3. **`MIDIUSB.h` is not the Teensy API.** Moved to native `usbMIDI`.
4. **Hardcoded channel 10** (`0xB9`). Now derived from the page.
5. **Presets died on power-off.** Now four EEPROM banks, saved explicitly.
6. **The display was never used.** v1 printed "Hello World" and nothing else.

### OLED

The panel is a GME128128-**01**, whose controller is an **SH1107**, monochrome.
Only the -02 in that family is an SSD1327. The first build used the SSD1327
driver and put garbage on the glass.

A 128 × 128 monochrome frame is 2 KB. The bus runs at 400 kHz, since 1 MHz was
unreliable. The display only redraws when state actually changed, and never
faster than every 120 ms.

Initialisation probes both 0x3C and 0x3D, and the result is checked. If no
panel answers, every display call is skipped so a missing or miswired screen
cannot wedge the loop.

---

## 7. Power and LEDs

The v1 flicker is not current draw, despite the comment in the source. It is
3.3V data driving 5V WS2812s, which sits on the logic threshold. Fix with a
74AHCT125 level shifter, a 330Ω series resistor, and a 1000 µF bulk cap.

At brightness 64 the strip draws roughly 180 mA, so the whole device sits near
300 mA and stays inside USB. Twelve WS2812s at full white would be about
720 mA and would need external 5V.

---

## 8. Housing

Bambu X1C, 256 mm cube. A twelve-encoder foot controller is wider than that,
so the split is forced.

- **Split horizontally** into a bottom tray and a top shell. Cutting left and
  right would put a seam through the control surface. Horizontal makes it a
  shadow line around the perimeter and leaves the deck continuous.
- **Print the top shell upside down**, deck face on the plate. Textured finish
  on the visible face, no supports.
- **Use the AMS for a translucent light channel.** Print light pipes into the
  opaque shell in one job so the twelve pixels read as a continuous glowing
  seam rather than twelve dots. Printed-in-place TPU feet in the same job.
- **Shell direction: brutalist slab.** Heavy chamfers, deck around 15°, one
  translucent seam tracing the perimeter.
- Connector-ise everything crossing the seam so the halves separate for service.

---

## 9. Open items

- Deck dimensions and encoder grid pitch, which set the housing split line.
- Whether the ST7796S library sitting unused in `v1/` was meant for something.
  v2 has no pins for it.
- Hardware MIDI DIN out is not possible in this map. All eight serial ports are
  consumed. It would cost an encoder or more pads.

---

## Appendix — KY-023 joystick geometry

Not used in v2. Recorded because it was measured from the NEPTR phase-4 shell,
where it is already proven, and it matches the KY-023 datasheet.

| Feature | Value |
|---|---|
| Hole in deck | Ø 30.7 mm |
| Mounting posts | 4, on a 27 × 20 mm rectangle, concentric with the hole |
| Pilot hole | Ø 2.9 mm, M3 self-tapper |
| Post top face | 4.1 mm below the outer deck surface |
| Module PCB | 34 × 26 mm, about 32 mm tall |

NEPTR also ran a Ø 6 mm column from the floor to the same height, dead centre
under the module. That is the load path when someone stands on it.
