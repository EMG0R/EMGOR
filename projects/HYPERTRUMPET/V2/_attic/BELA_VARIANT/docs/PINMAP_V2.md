# HYPERTRUMPET V2 — pinmap (DRAFT)

Status: draft, board = **Bela Gem Stereo** (PocketBeagle 2 based, not Gem Multi). Exact header pins filled in
2026-11-11. No Teensy: Bela reads everything. Source of v1 assignments: `V1_SCRAPE.md` §1.
Grounding / star-ground plan: `GROUNDING_PLAN.md` (read before wiring).
Tags: **[V]** verified in a primary source, **[I]** inferred, **[U]** unverified, check on the bench.
Sources: datasheet https://bela.io/datasheets/bela_gem_datasheet.pdf (Fig. 1, Tables 3/4);
Gem Stereo rev A4 KiCad source https://github.com/BelaPlatform/bela-hardware/tree/master/GemStereo/GemStereo_rev_A4
(analog header = J3: pad 1 REF, pad 2 GND, pads 3..10 = A7..A0); ADC datasheet
https://www.ti.com/document-viewer/ADS8168/datasheet/GUID-3893B402-8A32-46EA-9AFE-E923A462AF4C.
Pin names `P1.nn` / `P2.nn` are the datasheet's 2x18 header positions. **[U]** how P1/P2 are physically
reached on the assembled Stereo (datasheet Fig. 1 draws only the analog header, line in/out, headphones);
confirm on the board before wiring.

## Rules
- I2C sensors are read in an AuxiliaryTask, never in `render()`.
- Analog/digital channels are read in `render()` (they arrive with the audio frames, free).
- Smoothing/scaling moves out of firmware into one `Controls` layer in C++ (fixes v1's double-map bug
  that made sliders/pots saturate at 125).
- Debounce on all digital inputs (v1 had none).

## Audio inputs
Gem Stereo has **2 audio ins** (AC-coupled line in, TLV320AIC3104): 3-pin header LINE IN (`GND`=GNDA, `LIL`, `LIR`) or
the 4-pole 3.5 mm jack (T=L, R1=R, R2=GND, S=NC). [V] datasheet Table 1, Fig. 1.
| Bela audio in | Header pin | Signal | v1 |
|---|---|---|---|
| L (in 0) | LINE IN `LIL` | bell mic, via an external preamp/bias (capsule is not a line source; the Stereo exposes no MicBias on its headers [V by absence, datasheet Fig. 1 vs Fig. 2]) | — (was external) |
| R (in 1) | LINE IN `LIR` | contact mic 1 (body) via buffer, audio rate | CC48 @100 Hz peak |
| ground | LINE IN `GND` (= GNDA) | audio group return (GROUNDING_PLAN group A) | |

Input is already DC-blocked on the board (1 uF series, 220 ohm, 47 pF, BAS70 clamp) [V, rev A4 `audio_in_net`].
There is no "built-in mic" on the Gem Stereo; "built-in" meant the bell mic fitted in the trumpet.
**Contact mic 2: Stereo has no third audio in, so it goes to analog in A0 (see note A).**

## Analog inputs (Bela Gem Stereo analog header J3, 8 ch, 16-bit, DC-coupled)
Header, 10 pins, GND returns on its own pin: `REF` (pad 1, **leave unconnected**), `GND` (pad 2), then `A7`..`A0`
(pads 3..10) [V, datasheet Fig. 1; rev A4 PCB J3]. Pin 1 = triangle on the board.
| Ch | J3 pad | Sensor | v1 CC / pin |
|---|---|---|---|
| A0 | 10 | contact mic 2 (buffered, biased at mid-scale ~2.05 V) | CC49 / A9 |
| A1 | 9 | Valve 1 Hall (analog) | CC33 / 2 (was digital) |
| A2 | 8 | Valve 2 Hall (analog) | CC34 / 3 (was digital) |
| A3 | 7 | Valve 3 Hall (analog) | CC35 / 4 (was digital) |
| A4 | 6 | FSR | CC24 / A7 |
| A5 | 5 | Joystick X (flip in software) | CC30 / A10 |
| A6 | 4 | Joystick Y | CC31 / A11 |
| A7 | 3 | Slider 1 | CC20 / A0 |
| GND | 2 | analog sensor ground (GROUNDING_PLAN group B; one wire to the sensor local star) | |
| REF | 1 | do not connect | |

### ADS1115 on I2C 0x48 (4 single-ended ch, 16-bit, ADDR -> GND)
Moved off the Bela because the 3 Halls and contact mic 2 now take 4 of the 8 channels. These are the slow controls;
ADS1115 does 860 SPS max (ADS1015 3300 SPS if faster is wanted; same 0x48 address, same wiring) [U, TI part data not re-checked].
| ADS ch | Sensor | v1 CC / pin |
|---|---|---|
| AIN0 | Slider 2 | CC21 / A1 |
| AIN1 | Slider 3 | CC22 / A6 |
| AIN2 | Pot 1 | CC28 / A13 |
| AIN3 | Pot 2 | CC29 / A14 (v1 never used it) |
Supply it from 3V3 (never 5 V: inputs must stay within VDD + 0.3 V [U]). Range is 0-3.3 V, so PGA = +/-4.096 V is fine, scale in the `Controls` layer.
Reads happen in the I2C AuxiliaryTask, not `render()`.

Note A (resolved for Stereo): contact mic 1 uses audio in R, the bell mic uses audio in L; contact mic 2 takes
**analog A0**, and Pot 2 (with Sliders 2/3 and Pot 1) moved to the ADS1115 above.
Contact mic 2 on the analog ADC: the input is unipolar (0..4.096 V), so the buffer must bias the piezo at ~2.05 V
and the firmware takes |sample - mid| peak envelope (as v1 did at 512/1023). Analog sample rate on the Gem is
250 kS/s across channels [V datasheet Table 1]; the per-channel rate under the Bela software is not confirmed [U].

**Voltage range:** Bela analog ins are **0-4.096 V**, inferred [I]: ADS816x internal reference is 4.096 V, FSR 0..VREF
(TI datasheet above) and the rev A4 schematic leaves the 3V3-to-REFIO jumper (JP2) open. The Gem datasheet does not state it;
the older Bela page states 0-4.096 V with 5 V tolerance (https://learn.bela.io/using-bela/about-bela/bela-hardware/).
Power pots/sliders/FSR/joystick/Halls from 3.3 V, not 5 V (a 3.3 V sensor swing uses ~80 % of the range).
Each Hall (ratiometric linear part, 3.3 V rated) goes direct to its pad, <= 10 k source impedance [U, input buffer/impedance not checked];
Hall output at no-magnet sits near mid-supply, so read the idle value at start-up and treat it as the zero.

## Digital (Bela D0-D15, INPUT, debounced) — header positions from datasheet Tables 3/4 [V]
The Halls are now analog (above), so digital needs only 8 lines.
| Ch | Header pin | Signal | v1 CC / pin |
|---|---|---|---|
| D0 | P1.21 | Joystick button | CC32 / 5 |
| D1 | P1.23 | Arcade button | CC40 / 6 |
| D2 | P1.25 | Encoder 1 A | 7 |
| D3 | P1.27 | Encoder 1 B | 8 |
| D4 | P1.30 | Encoder 1 button | CC38 / 9 |
| D5 | P1.32 | Encoder 2 A | 10 |
| D6 | P1.34 | Encoder 2 B | 11 |
| D7 | P2.01 | Encoder 2 button | CC39 / 12 |
| D8 P2.03, D9 P2.09, D10 P2.25, D11 P2.27, D12 P2.29, D13 P2.36, D14 P2.33, D15 P2.35 | | spare | |
Digital ground for buttons/encoders: **P1.22 GND** (sits between D0 and D1); alt. P1.16. Do not use P1.15 (USBGND). 3.3 V: P1.14 or P2.23.
Avoid P2.02/P2.04/P2.06/P2.08/P2.12 (LED A/B, BUTTON, POWER LED DISABLE, PWR BTN) and P1.33/P1.36 (codec I2C). P1.24 (5V) is next to D1 (P1.23): mind the short.
Bela digital is **3.3 V only** (never exceed 3.3 V on digital pins; older-Bela docs, same SoC family: https://learn.bela.io/using-bela/about-bela/bela-hardware/) [I].
**Pull-ups: the Gem docs do not say whether D0-D15 have internal pull-ups, so fit external 10 k pull-ups to 3.3 V on every button and encoder line** [U].
(An earlier draft claimed "no internal pull-ups enabled by default"; that is not documented for the Gem.)
Encoders decoded per audio frame (quadrature at audio rate, no missed detents).

## I2C (one bus)
Pins: **P1.26 SDA, P1.28 SCL** (3.3 V), or the **Qwiic connector J6** (GND, 3V3, SDA, SCL) which is the same bus [V, datasheet Table 3; rev A4 schematic `i2c`].
Pull-ups: **10 k to 3V3 are already on the Gem** (R13/R14) [V, rev A4 schematic]; do not add more, and note each breakout adds its own [U per board].
Linux bus number: not documented for the Stereo (hyperGuitar on Gem Multi used `/dev/i2c-1`) [U]; check `i2cdetect -l`.
Do not use P1.33/P1.36 (codec bus).
| Addr | Device | v1 | Rate |
|---|---|---|---|
| 0x48 | ADS1115 (Sliders 2/3, Pots 1/2) | CC21/22/28/29 | 100 Hz |
| 0x2E | NeoTrellis 4x4 | CC50–65 | 60 Hz scan |
| 0x77 | BME680 breath | CC47 | max |
| 0x29 | VL53L0X lidar | CC23 | 30 Hz |
| 0x68 | MPU6500 IMU | CC41–46 | 200 Hz (up from 50) |
| 0x60 | Adafruit NeoDriver (seesaw) — drives both NeoPixel strips | pins 28/29 | 30 Hz |
v1 had the IMU on a second bus; on Bela all five share one (no address clashes).
NeoPixels can't be bit-banged from Bela reliably -> NeoDriver over I2C replaces pins 28/29.
**LED power (NeoDriver, strips, NeoTrellis LEDs) is never taken from a Bela pin and its return never goes to a Bela ground pin.**
Feed from the LED branch and return to STAR (`GROUNDING_PLAN.md` §4); only the I2C signals + Qwiic GND touch the Bela.
On hyperGuitar a NeoTrellis ground shared with the audio side caused LED-colour-pitched noise; moving its ground pin fixed it.

## Open
- Board variant: **resolved, Gem Stereo** (contact mic 2 -> analog A0, Pot 2 -> ADS1115; see above).
- Unverified: physical access to P1/P2 on the assembled Stereo; analog range 0-4.096 V (inferred); D0-D15 internal
  pull-ups; Linux I2C bus number; 3V3/5V header current limits. Full list: `GROUNDING_PLAN.md` §8.
- BME680 is a slow barometric sensor for breath (v1 full scale at 0.32 hPa). Candidate upgrade: a
  differential pressure sensor (e.g. MPRLS / XGZP) for faster, drift-free breath. Decide at build.
- Hall valves: **decided analog** (A1-A3, half-valve depth). They displaced Slider 2/3 and Pots 1/2 to the ADS1115.
  If a faster path is wanted for the Halls, they are already on the Bela ADC; if the ADS1115 is too slow for the
  sliders, use an ADS1015 (same address).
