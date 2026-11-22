# 4 i Got — Quad Audio Interface

**Project: EMGOR SYNTH | Date: 3/25/26 | Target Done: 7/1/26**

---

## What It Does

Quad I/O USB audio interface on Teensy 4.0. Acts as USB sound card + MIDI device.

- 4 ch out — 2× PCM5102 stereo DAC
- 4 ch in — 2× PCM1808 stereo ADC
- MIDI IN + OUT — 1/8" TRS Type A, hardware UART
- Sustain IN — 1/8" TRS footswitch

---

## Why Teensy 4.0 Handles This Guaranteed

Two independent hardware I2S buses (SAI1 + SAI2) are built into the iMX RT1062. The Teensy Audio Library has dedicated objects for both:

| Audio Object | Bus | Direction |
|---|---|---|
| `AudioOutputI2S` | SAI1 TX | pin 7 out → DAC 1 |
| `AudioInputI2S` | SAI1 RX | pin 8 in ← ADC 1 |
| `AudioOutputI2S2` | SAI2 TX | pin 2 out → DAC 2 |
| `AudioInputI2S2` | SAI2 RX | pin 3 in ← ADC 2 |

All four share one MCLK/BCLK/LRCLK — hardware synchronized, zero clock drift. DMA handles all transfers. MIDI runs on Serial1 hardware UART (separate interrupt). CPU load ≈5% at 44.1kHz on 600MHz Teensy 4.0. DSP pipeline runs in 32-bit float on the hardware FPU before I2S conversion.

---

## Parts List

| Part | Qty | Notes | Link |
|------|-----|-------|------|
| **Teensy 4.0** | 1 | Dual I2S, HW FPU, USB Audio+MIDI | [Amazon](https://www.amazon.com/Teensy-4-0-With-Pins/dp/B08259KDHY) |
| **PCM5102 DAC** | 2 | 112dB SNR, line-level out, I2S | [Amazon](https://a.co/d/07m0uIzp) |
| **PCM1808 ADC** | 2 | 24-bit 96kHz, 105dB SNR, I2S — buy 2-pack | [Amazon](https://a.co/d/0jbIUViK) |
| **3.5mm TRS Jack** | 7 | Layout L→R: Audio IN 1, Audio IN 2, MIDI IN, Sustain/Expr, MIDI OUT, Audio OUT 1, Audio OUT 2 | [Amazon](https://a.co/d/098v7rsj) |
| **6N138 Optocoupler** | 1 | MIDI IN galvanic isolation — kills ground loops | [Amazon](https://a.co/d/0iZ6eqAC) |
| **Resistor Variety Pack** | 1 kit | 1/4W through-hole. Need: 220Ω ×3 (MIDI), 1kΩ ×5 (1 opto base + 4 ADC input protection), 4.7kΩ ×1 (MIDI pull-up), 10kΩ ×2 (DAC XSMT/FMT safety pulls) | [Amazon](https://www.amazon.com/REXQualis-Resistor-Assortment-Kit-Values/dp/B07D54XMFK) |
| **1N4148 diode** | 1 | MIDI IN reverse protection | [Amazon](https://a.co/d/0dLhOUFu) |
| **100nF ceramic cap** | 8 | Bypass — one per VCC pin on each IC, place within 2mm on PCB | [Amazon](https://a.co/d/00Ju4G9m) |
| **10µF electrolytic cap** | 2 | Bulk decoupling — one on 3.3V rail, one on 5V rail | [Amazon](https://a.co/d/05WSyciH) |
| **Ferrite bead 600Ω @ 100MHz** | 2 | On 5V rail to each PCM1808 VCC (analog power) — splits analog from digital power | [Amazon](https://a.co/d/03H4Oz5R) |
| **10µF electrolytic cap** | 4 | AC coupling on DAC L/R outputs → audio jacks (skip if breakout has them) | [Amazon](https://a.co/d/05WSyciH) |
| **500mA polyfuse** | 1 | Resettable fuse on 5V rail — USB power protection | — |
| **3D Print Enclosure** | 1 | Clips/magnets to lander | Custom |

---

## Pin Connections

### Teensy 4.0 → DAC 1 (PCM5102)
```
pin 7  (DOUT)  → DIN
pin 20 (LRCLK) → LCK
pin 21 (BCLK)  → BCK
               ✕ SCK — NOT connected (breakout ties SCK→GND, uses internal PLL)
3.3V           → VIN
GND            → GND
```
**Safety pull resistors on motherboard PCB:**
- XSMT → 10kΩ → 3.3V (guarantees unmuted)
- FMT  → 10kΩ → GND  (guarantees I2S format)

### Teensy 4.0 → DAC 2 (PCM5102)
```
pin 2  (DOUT2) → DIN
pin 20 (LRCLK) → LCK   [shared net]
pin 21 (BCLK)  → BCK   [shared net]
               ✕ SCK — NOT connected
3.3V / GND     → same as DAC 1
```
Same XSMT/FMT pull resistors as DAC 1.

### DAC Audio Output Routing
```
DAC 1 L pad ── 10µF AC coupling cap ── Audio OUT 1 TRS jack TIP
DAC 1 R pad ── 10µF AC coupling cap ── Audio OUT 1 TRS jack RING
DAC 2 L pad ── 10µF AC coupling cap ── Audio OUT 2 TRS jack TIP
DAC 2 R pad ── 10µF AC coupling cap ── Audio OUT 2 TRS jack RING
All jack SLEEVEs → AGND
```
Note: Skip AC coupling caps if breakout board already has output caps onboard. Check physically.

### Teensy 4.0 ← ADC 1 (PCM1808)
```
pin 8  (DIN1)  ← DOUT
pin 20 (LRCLK) → LRC   [shared net]
pin 21 (BCLK)  → BCK   [shared net]
pin 23 (MCLK)  → SCK   (MCLK only goes to ADCs, NOT DACs)
3.3V           → VDD (digital)
5V             → VCC   (analog — via ferrite bead from 5V rail)
GND            → DGND  (to digital ground plane)
GND            → AGND  (to analog ground plane)
MD0            → GND   [slave mode]
MD1            → GND   [slave mode]
FMT            → GND   [I2S format]
```

### ADC 1 Audio Input Routing
```
Audio IN 1 TRS jack TIP    ── 1kΩ ── LINR  (left channel)
Audio IN 1 TRS jack RING   ── 1kΩ ── RINR  (right channel)
Audio IN 1 TRS jack SLEEVE → AGND
```
1kΩ series resistors protect ADC inputs from hot signals.

### Teensy 4.0 ← ADC 2 (PCM1808)
```
pin 3  (DIN2)  ← DOUT
All clocks     → shared nets from ADC 1
MD0, MD1, FMT  → GND
Power/GND      → same as ADC 1
```

### ADC 2 Audio Input Routing
```
Audio IN 2 TRS jack TIP    ── 1kΩ ── LINR  (left channel)
Audio IN 2 TRS jack RING   ── 1kΩ ── RINR  (right channel)
Audio IN 2 TRS jack SLEEVE → AGND
```

### WARNING: DAC Output Voltage
The PCM5102A outputs ~6V peak-to-peak. This WILL damage PCM1808 ADC inputs
if connected directly. Never loop DAC output back to ADC input without a 2:1
voltage divider (two equal resistors, e.g. 10kΩ + 10kΩ).

### MIDI IN — 6N138 circuit (1/8" TRS Type A)
```
TRS RING (MIDI pin 4) ── 220Ω ── 6N138 pin 2 (anode)
TRS TIP  (MIDI pin 5) ────────── 6N138 pin 3 (cathode)
1N4148 diode          ── cathode→pin 3, anode→GND (protection)
6N138 pin 8 (Vcc)     → 5V
6N138 pin 1 (GND)     → GND
6N138 pin 6 (emitter) → GND
6N138 pin 5 (output)  → 4.7KΩ → 3.3V
6N138 pin 5           → Teensy pin 0 (RX1)
```

### MIDI OUT (1/8" TRS Type A)
```
Teensy pin 1 (TX1) ── 220Ω ── TRS RING (MIDI pin 4)
3.3V               ── 220Ω ── TRS TIP  (MIDI pin 5)
```

### Sustain IN (1/8" TRS)
```
TRS TIP    → Teensy pin 4  (INPUT_PULLUP — LOW = pedal pressed)
TRS SLEEVE → GND
```

---

## PCB Grounding Rules (Critical for Low Noise)

**Split ground plane:**
- Analog ground (AGND) under all audio ICs — PCM5102, PCM1808 analog sections, audio jack grounds
- Digital ground (DGND) under Teensy, USB, MIDI circuit, PCM1808 digital sections
- Connect AGND and DGND at **one single point** — star ground near the Teensy GND pin
- Never let digital return current cross the analog ground plane

**Power:**
- 500mA polyfuse on 5V rail between Teensy VUSB out and board 5V distribution
- Run 3.3V and 5V as separate power planes or wide traces (≥1mm)
- Place ferrite bead between 5V digital rail and VCC (analog) on each PCM1808
- Place 100nF bypass cap within 2mm of every VCC/VDD/VCCA pin — shorter trace = better
- Place 10µF bulk cap at the 3.3V entry point and 5V entry point

**Clock lines (MCLK, BCLK, LRCLK):**
- Route as short as possible
- Keep away from analog audio input traces and ADC input pins
- MCLK (pin 23) is a fast square wave — treat it like a digital signal, route on digital ground side

**Audio traces:**
- Keep audio IN traces (ADC inputs) on the analog ground side
- Do not run audio traces parallel to clock or USB traces
- Audio jacks: sleeve/GND pin goes to AGND only

**MIDI / 6N138:**
- 6N138 provides galvanic isolation — MIDI circuit ground is fully floating from your ground until the opto output connects to DGND. Keep MIDI circuit physically separated from audio section.

---

## Arduino IDE Setup

- Board: Teensy 4.0
- Tools → USB Type: **MIDI + Audio**
- CPU Speed: 600 MHz

---

## Files

```
4-i-gor/
├── 4-i-gor.ino
├── 4-i-gor.md
├── 4-i-gor-pcb.md
└── 3d/
    ├── enclosure.stl
    └── adapter-holder.stl
```
