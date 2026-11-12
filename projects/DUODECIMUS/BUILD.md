# DUODECIMUS — build and wiring

Teensy 4.1. Twelve encoders, twelve NeoPixels, one 1.5" SH1107 OLED, two TRS
pedal jacks. No TFT, no joystick, no arcade button.

**41 of 42 top-side pins. NO BOTTOM PADS. Pin 41 is spare.**

Built around preserving the v1 harness. Nine of the ten original encoders are
untouched, as are the NeoPixel line and the expression pedal's tip.

**Exactly two existing wires move**, both on encoder 6.

---

## 1. What changes

### Remove

The TFT is gone. Unsolder all six of its wires: 8, 9, 10, 11, 12, 13.
Encoders 11 and 12 move into that space.

### Move (two wires)

| Wire | From | To |
|---|---|---|
| Encoder 6, B | 24 | 0 |
| Encoder 6, SW | 25 | 1 |

Encoder 6's A pin stays on 23. Only its other two wires move.

This is to free pins 24 and 25, which are the Teensy's Wire2 I2C pair, for the
OLED. Every on-top I2C pair was occupied by an original encoder (18/19 by
encoder 4, 16/17 by encoders 3 and 4, 24/25 by encoder 6), so one had to give
way. Moving two wires is the cheapest possible cut, and it is what lets you
avoid the bottom pads entirely.

### Add

| New connection | Pin |
|---|---|
| Encoder 11 A / B / SW | 8, 9, 10 |
| Encoder 12 A / B / SW | 11, 12, 13 |
| OLED SCL | 24 |
| OLED SDA | 25 |
| Jack B tip | 40 |

Plus grounds and the OLED's 3.3V.

### Do not touch

Encoders 1 through 5 and 7 through 10. The NeoPixel data line on pin 38. Jack
A's tip on pin 39. Your expression pedal keeps working exactly as wired.

---

## 2. Full pin map

| Enc | A | B | SW | |
|----|----|----|----|---|
| 1  | 2  | 3  | 4  | unchanged |
| 2  | 5  | 6  | 7  | unchanged |
| 3  | 14 | 15 | 16 | unchanged |
| 4  | 17 | 18 | 19 | unchanged |
| 5  | 20 | 21 | 22 | unchanged |
| 6  | 23 | **0** | **1** | A unchanged, B and SW moved |
| 7  | 26 | 27 | 28 | unchanged |
| 8  | 29 | 30 | 31 | unchanged |
| 9  | 32 | 33 | 34 | unchanged |
| 10 | 35 | 36 | 37 | unchanged |
| 11 | 8  | 9  | 10 | new |
| 12 | 11 | 12 | 13 | new |

| Function | Pin | |
|---|---|---|
| NeoPixel data | 38 | unchanged |
| OLED SCL | 24 | new |
| OLED SDA | 25 | new |
| Jack A tip | 39 | unchanged |
| Jack B tip | 40 | new |
| spare | 41 | |

---

## 3. Encoders

Standard EC11. The centre of the three pins goes to ground, the outer two go
to A and B. One switch pin goes to SW, the other to ground.

No pull-up resistors. The firmware enables the Teensy's internal pull-ups on
all thirty-six lines. If a knob counts backwards, swap its A and B wires.

---

## 4. Pedal jacks

**One Teensy pin per jack, on the tip.** The pedal type is chosen by hand, not
detected, so no ring-sense pin is needed.

| Jack lug | Jack A | Jack B |
|---|---|---|
| Tip | pin 39 (unchanged) | pin 40 |
| Ring | 3.3V through 220Ω | 3.3V through 220Ω |
| Sleeve | GND | GND |

### The 220Ω is not optional

An expression pedal needs the rail across its pot, which is why ring goes to
3.3V. But a sustain pedal is a **mono** plug, and its barrel bridges the jack's
ring lug to the sleeve lug. Wired straight to the rail that is a dead short to
ground: brownout at best, a cooked regulator at worst.

Through 220Ω the same short draws about 15 mA and is harmless.

The cost to an expression pedal is roughly two percent off the top of its
sweep, and the firmware auto-calibrates its range anyway, so you will not
notice it.

**This applies to jack A as it is wired today.** If you have ever plugged a
sustain pedal into it, that was a short. Add the resistor.

### Selecting the pedal type

Held in EEPROM, so it survives power-off.

| | |
|---|---|
| SHIFT + press encoder 6 | toggle jack A between expression and sustain |
| SHIFT + press encoder 7 | toggle jack B |

The OLED shows the current setting as `A:EXP B:SUS`. A blank EEPROM defaults to
jack A expression and jack B sustain.

Sustain polarity is captured the moment you select the type, so normally-open
and normally-closed pedals both work with no setting. Expression range is
learned from observed travel, so sweep the pedal fully once after selecting it.

---

## 5. NeoPixels

Twelve WS2812 on pin 38, unchanged from v1.

**Add a level shifter.** v1 blamed its flicker on current draw. The real cause
is 3.3V data driving 5V WS2812s, which sits right on the logic threshold.

- Pin 38 → 74AHCT125 input (1A)
- 74AHCT125 output (1Y) → 330Ω → strip data in
- 74AHCT125 VCC → 5V, 1OE → GND
- 1000 µF capacitor across the strip's 5V and GND at the first pixel

At power-on the firmware walks a single pixel from index 0 to 11, then holds
all twelve in distinct hues. Use the chase to read off which physical pixel is
which. All twelve stay lit at all times afterward; page indication dims the
others rather than blanking them.

---

## 6. OLED

1.5" GME128128-01-IIC. The controller is an **SH1107**, monochrome 128 × 128.

**This matters and it is easy to get wrong.** The GME128128-**01** is SH1107.
Only the **-02** is SSD1327 greyscale. Same size, same connector, same part
number family. Driving one with the other's command set puts garbage on the
glass, which is exactly what happened on the first build here.

- VCC → 3.3V
- GND → GND
- SDA → pin 25
- SCL → pin 24

Wire2 bus at 400 kHz. 1 MHz was tried and was unreliable. The firmware probes
I2C address 0x3C and then 0x3D, so there is nothing to configure.

If it is still unstable, fit 2.2k pull-ups to 3.3V on both lines and keep the
runs short. The display is optional: if it never answers, the firmware skips it
entirely and encoders and MIDI carry on.

---

## 7. Ground bus

Build this before anything else. Twelve encoders need two ground connections
each, so twenty-four before you count the LEDs, OLED, and two jacks. The Teensy
has around seven ground pins.

Run a length of solid bus wire through the middle of the encoder field. Every
local ground lands there. Take the bus back to the Teensy on two or three short
thick wires. Star-grounding twenty-four thin wires to a handful of pins gives
you noise and something you cannot service.

Label every wire as you cut it.

---

## 8. Power

At brightness 64 the twelve pixels draw roughly 180 mA. With the Teensy and the
OLED you are near 300 mA, comfortably inside USB's 500 mA.

If you raise `strip.setBrightness()` or add pixels, move to external 5V. Twelve
WS2812s at full white is about 720 mA on their own.

---

## 9. Flashing

- Tools → Board → **Teensy 4.1**
- Tools → USB Type → **MIDI**
- Tools → CPU Speed → **600 MHz**

Libraries: Adafruit NeoPixel, Adafruit GFX, Adafruit SH110X, Adafruit BusIO.

```
arduino-cli compile --fqbn teensy:avr:teensy41:usb=midi,speed=600 foots2
arduino-cli upload  --fqbn teensy:avr:teensy41:usb=midi,speed=600 -p <port> foots2
```

---

## 10. Controls

Encoder 1's button is **SHIFT**.

| Action | Result |
|---|---|
| Turn any encoder | rotation CC |
| Press any encoder | button toggle CC |
| Tap encoder 1 alone | encoder 1's normal toggle |
| SHIFT + turn encoder 1 | change page |
| SHIFT + turn encoder 12 | change bank |
| SHIFT + press encoder 2 | accumulator on / off |
| SHIFT + press encoder 3 | invert range |
| SHIFT + press encoder 4 | save presets to bank |
| SHIFT + press encoder 5 | load preset |
| SHIFT + press encoder 6 | jack A pedal type |
| SHIFT + press encoder 7 | jack B pedal type |

Accumulator off sends 127 or 0 per detent, for endless relative control.
Accumulator on tracks an absolute 0 to 127 per encoder.

### Press and hold

Hold any encoder button and two one-shots fire, each once per hold, carrying
that encoder's number 1 to 12 as their value:

| Held for | Sends | Meaning |
|---|---|---|
| 5 seconds | CC 116 | capture |
| 8 seconds | CC 117 | delete |

The held knob turns solid white at 5 seconds and blinks at 8, and the screen
reads CAPTURE then DELETE, so the thresholds are visible rather than counted.

Chorded presses under SHIFT are deliberately not armed for this, so a shift
combination can never trip capture or delete. Encoder 1 held on its own does
still fire, and suppresses its own tap so no stray press is sent on release.

---

## 11. MIDI map

Everything on **channel 7**. No pages, no banks. Every CC number below sits in
undefined MIDI space on purpose, so nothing collides with a standard controller
a host might auto-map.

| | |
|---|---|
| Encoder 1..12 rotation | CC 20 through 31 |
| Encoder 1..12 button | CC 102 through 113 |
| Jack A as expression | CC 14 |
| Jack B as expression | CC 15 |
| Jack A as switch | CC 114 |
| Jack B as switch | CC 115 |
| Hold 5s, capture | CC 116, value = encoder number |
| Hold 8s, delete | CC 117, value = encoder number |

**Buttons are momentary**, 127 on press and 0 on release. They are not a
latch. The pixel still toggles on each press, but that state is device-local
and never reported, which is what lets a host time a hold.

**Rotation is relative by default**: exactly 127 clockwise, exactly 0
counter-clockwise, one message per detent. It is a direction pulse, not a
position. In accumulator mode it sends an absolute 0 to 127 instead, so a
mid-range value on CC 20..31 is the tell.

The device identifies itself over USB as product **DUODECIMUS**, manufacturer
**EMGOR**. Anything matching the MIDI port by name wants that string.

All constants are at the top of the sketch.

---

## 12. Bring-up order

1. Flash the bare board. It boots into the six-second show, then the panel
   shows a large `-` and the two jack widgets.
2. Ground bus.
3. OLED. Both addresses are probed automatically.
4. One encoder, verified in the web panel, before wiring the other eleven.
5. Encoder 6's two moved wires, and encoders 11 and 12.
6. The rest.
7. LEDs, with the level shifter already fitted. Watch the boot chase.
8. Jacks, with the 220Ω resistors fitted. Select each pedal type with SHIFT.

The web panel in `../webui/` shows every encoder live and records which
firmware index sits at which physical position.
