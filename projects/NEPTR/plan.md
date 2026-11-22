# NEPTR — Phase 4 Rebuild

Teensy 4.1 MIDI controller. Rebuild of stolen original. Hardware changes vs original: 4× discrete NeoPixels → continuous addressable strip; HC-SR04 → 2× VL53L1X ToF on **separate I²C buses**; added MPU9250 9-DoF IMU. Pinout adjusted accordingly — see Pin Map.

---

## Parts

| Part | Qty | Link |
|------|-----|------|
| Teensy 4.1 (with pins) | 1 | [Amazon](https://www.amazon.com/PJRC-Teensy-4-1-with-Pins/dp/B08CTM3279) |
| Taiss KY-040 Rotary Encoder Module 5-pack (onboard pullups + debounce caps) | 2 | [Amazon](https://a.co/d/05I7NiqH) |
| Encoder knob caps (4-pack) | 2 | [Amazon](https://a.co/d/05cyQFva) |
| Twidec PBS-110 7mm Momentary Pushbutton | 8 | [Amazon](https://a.co/d/02QhDdu3) |
| Arcade button (LED removed, BTN9) | 1 | [Amazon](https://a.co/d/04WXL6kG) |
| KY-023 Analog XY Joystick | 1 | [Amazon](https://a.co/d/0i8iaHig) |
| TOF400C VL53L1X 4M ToF Laser Distance Sensor (4-pack) — running **2× concurrently** | 1 | [Amazon](https://a.co/d/0a00IPxF) |
| MPU9250 GY-9250 9-Axis IMU (gyro + accel + mag, I²C 0x68) | 1 | [Amazon](https://a.co/d/06vXOllL) |
| FSR402 Force Sensor (3-pack) | 1 | [Amazon](https://a.co/d/04UO8M78) |
| Piezo disc element 20mm with leads (contact mic) | 2 | [Amazon](https://a.co/d/07Dr4OM2) |
| PJ-307 1/8" Stereo TRS Jack (expression pedal in) | 1 | [Amazon](https://a.co/d/0fMCcA4w) |
| SEZO WS2812B 5050 Addressable LED Strip (144 LEDs/m, 1m, DC5V) | 1 | [Amazon](https://a.co/d/0eOQoq9E) |
| REXQualis Resistor Assortment Kit (10Ω–1MΩ, 30 values) — covers 10kΩ × 3 pedal/FSR, 100kΩ × 4 mic bias, 1MΩ × 2 mic shunt | 1 | [Amazon](https://www.amazon.com/REXQualis-Resistor-Assortment-Kit-Values/dp/B07D54XMFK) |
| 100nF ceramic capacitors (contact mic DC block, 2 needed) | 1 pack | [Amazon](https://a.co/d/00Ju4G9m) |
| SUNGUY 6" Micro-USB cable, 3-pack, data + charging (internal Teensy ↔ Pi USB-A) | 1 | [Amazon](https://www.amazon.com/dp/B0BHNVLW1Z) |
| IP67 USB-C female panel-mount → USB-C male, 0.2m FPC, 100W (20V/5A) PD (Pi 5 power input to outer casing) | 1 | [Amazon](https://www.amazon.com/dp/B0FK52G5S2) |
| USB panel-mount extension (Pi 5 USB-A ports to outer casing) | 3 | [Amazon](https://a.co/d/04FJ2Kdy) |
| Ethernet panel-mount extension (Pi 5 Ethernet to outer casing) | 1 | [Amazon](https://a.co/d/0cYr9aZC) |
| Pi touchscreen display | 1 | [Amazon](https://a.co/d/0bCioVEg) |
| M2/M2.5/M3 threaded insert assortment (heat-set, for 3D-printed shell) | 1 | [Amazon](https://a.co/d/04FhVgr5) |
| M2/M2.5/M3 screw assortment (for threaded inserts + panel mounts) | 1 | [Amazon](https://a.co/d/0hdkDt3e) |
| Rubber feet — Large Extra Tall #2 Round w/Metal Insert, 1" H × 1.10" W, Made in USA | 1 pack (4 ct) | [Amazon](https://www.amazon.com/dp/B01LDT7UHM) |
| Clear silicone U-channel edge trim, 3/16" profile, fits 1/32"–3/32" wall — seam gap diffuser for LED strip | 1 | [Amazon](https://a.co/d/09kChvo4) |
| 22 AWG silicone hookup wire | from stock | — |
| 3D-printed shell — HEAD + BODY | 1 set | `shell/NEPTR_p3_HEADv3.3mf`, `shell/NEPTR_p3_BODYv5.3mf` |

---

## Pin Map (Teensy 4.1, matches `TEENSYY/TEENSYY.ino`)

### Encoders — A / B / Switch — all `INPUT_PULLUP`, common → GND

| Encoder | A | B | SW | CC rot | CC sw |
|---------|---|---|----|--------|-------|
| ENC1 | 2 | 3 | 35 | 20 | 70 |
| ENC2 | 4 | 5 | 36 | 21 | 71 |
| ENC3 | 6 | 9 | 37 | 22 | 72 |
| ENC4 | 10 | 11 | 1 | 23 | 73 |
| ENC5 | 12 | **8** | 49 *(underside)* | 24 | 74 |
| ENC6 | 14 | 15 | 40 | 25 | 75 |
| ENC7 | 22 | 23 | **51** *(underside)* | 26 | 76 |

> **Pin 13 is unusable for `INPUT_PULLUP`** — it's the Teensy 4.1 onboard LED. The LED + ~470 Ω load to GND overpowers the weak internal pull-up, so the pin can't be held high cleanly. ENC5's B leg lives on **pin 8** instead. ENC7's switch was moved off pin 41 to underside pad **51** to free pin 41 (A17) for the expression pedal.

### Face Buttons — `INPUT_PULLUP`, other leg → GND

| BTN | Pin | CC |
|-----|-----|----|
| 1 *(arcade)* | **0** | 60 |
| 2 | 26 | 61 |
| 3 | 27 | 62 |
| 4 | 28 | 63 |
| 5 | 29 | 64 |
| 6 | 30 | 65 |
| 7 | 31 | 66 |
| 8 | 32 | 67 |
| 9 | 33 | 68 |

### Joystick (KY-023)

| Pin | Signal | Wire to | CC |
|-----|--------|---------|----|
| A2 (16) | X | VRx | 27 |
| A3 (17) | Y | VRy | 28 |
| 34 | Click | SW → GND, `INPUT_PULLUP` | 77 |
| 3.3V | VCC | +5V pin on module (Teensy ADC is 3.3V — power from 3.3V) | — |
| GND | GND | GND | — |

### Expression Pedal Jack (PJ-307 stereo TRS)

| Teensy | TRS lug | CC |
|---|---|---|
| **A17 (pin 41)** | TIP — wiper signal | 29 |
| GND | SLEEVE | — |
| 3.3V | RING — pot reference voltage | — |

> Moved off A10 (pin 24) — pin 24 is now **Wire2 SCL** for LiDAR 2. The pedal pot is its own voltage divider; no external resistor needed. Pedal must be Roland/Boss style (TIP = wiper). If readings look inverted, swap TIP and RING at the jack (that's the Yamaha convention).

### VL53L1X #1 + #2 — on **two separate I²C buses**, no XSHUT

Each sensor is alone on its bus at the default `0x29` — no address dance, no XSHUT wiring.

**LiDAR 1 — Wire bus (shares with MPU9250):**

| Teensy | Sensor 1 | CC |
|---|---|---|
| 18 (SDA) | SDA | — |
| 19 (SCL) | SCL | — |
| 3.3V | VIN | — |
| GND | GND | — |

**LiDAR 2 — Wire2 bus (dedicated):**

| Teensy | Sensor 2 | CC |
|---|---|---|
| **25 (SDA2)** | SDA | — |
| **24 (SCL2)** | SCL | — |
| 3.3V | VIN | — |
| GND | GND | — |

Distance CCs: #1 = 30, #2 = 36.

> **Why two buses instead of XSHUT?** The original plan put both on Wire with XSHUT-based re-addressing. Hard lessons forced a rebuild: the Pololu carrier's XSHUT pin is **not level-shifted** (pulled to 2.8 V on-board), so driving it HIGH from a 3.3 V Teensy GPIO can wedge the sensor — the correct release method is `pinMode(pin, INPUT)` letting the on-board pull-up enable it. Even with that fix, an unwired/loose XSHUT means both sensors sit at 0x29 simultaneously → a `setAddress()` writes to both → they stack on one address invisibly. Putting #2 on Wire2 eliminates the whole class of problems. Both VL53L1X stay at their 0x29 default forever.

### 9-DoF IMU (MPU9250, I²C addr 0x68) — on Wire bus, shared with LiDAR 1

| Teensy / signal | Wire to | CC |
|---|---|---|
| 18 (SDA) | IMU SDA | 31 / 32 / 33 (accel X / Y / Z) |
| 19 (SCL) | IMU SCL | (gyro + mag CCs reserved if firmware extends) |
| 3.3V | VCC | — |
| GND | GND | — |
| **AD0 → GND** | **required** — fixes I²C address at 0x68 | — |

> **AD0 must be tied to GND.** Floating or pulled high = MPU lands at 0x69 and the firmware can't find it. The most common cause of "MPU was working, now isn't" is a popped AD0 jumper. Run `TEENSYY/busscan/` to confirm — `0x68` = good, `0x69` = AD0 floated, neither = power or SDA/SCL.

### FSRs (voltage-divider read)

| Pin | Wiring | CC |
|-----|--------|----|
| 20 (A6) | 3.3V → FSR1 → A6 → 10kΩ → GND | 34 |
| 21 (A7) | 3.3V → FSR2 → A7 → 10kΩ → GND | 35 |

### Contact Mics (piezo discs, envelope-read)

Two piezo discs glued/screwed to the inside of the left and right shell walls. Used as tap/vibration sensors — not for audio. Firmware reads raw analog, takes `abs(reading - midpoint)`, smooths it, and sends as a CC.

```
Piezo (+) ──┬── 100nF ──┬── Teensy analog pin
            │            ├── 1MΩ → GND        (high-impedance shunt)
            │            ├── 100kΩ → 3.3V     (1.65V bias divider)
            │            └── 100kΩ → GND      (1.65V bias divider)
Piezo (−) ──┴── GND
```

| Pin | Wiring | CC |
|-----|--------|----|
| 38 (A14) | Contact mic L through bias circuit above | 78 |
| 39 (A15) | Contact mic R through bias circuit above | 79 |

The 1.65V bias lets the piezo's AC swing land in the middle of the 0–3.3V ADC range. For percussive use only — for clean audio you'd want a TL072 buffer first, but for tap detection raw read + envelope follow is enough.

### LED Strip

| Pin | Wire to |
|-----|---------|
| 7 | strip DIN |
| 5V (Teensy VUSB pad) **or external 5V supply** | strip 5V |
| GND | strip GND (common with Teensy GND if using external supply) |

Drop a 100 nF + 10 µF across strip +5V/GND near the data injection point if you see first-pixel flicker.

> **Power budget:** firmware caps brightness via `pixels.setBrightness(15)` so a ~96-LED rainbow stays inside the ~500 mA USB budget (~250 mA strip + ~100 mA Teensy). The **~96 mA quiescent draw** of 96 WS2812 chips is unavoidable regardless of brightness setting. Past ~96 LEDs at any visible brightness, the strip *must* run off an external 5 V supply with a shared GND — brownouts hit fast and unforgivingly. Code can be nudged toward `setBrightness(25)` if powered externally and stability holds.

---

## Pin Budget

43 GPIO used of Teensy 4.1's 55. Top edge (0–41) fully consumed **except pin 13** (onboard LED — not usable with `INPUT_PULLUP`). Underside pads in use: **49** (ENC5 switch), **51** (ENC7 switch). Free underside pads: 42–48, 50, 52–54.

**Bus allocation:**
- `Wire` (18 SDA / 19 SCL) → MPU9250 (0x68) + LiDAR 1 (0x29)
- `Wire2` (25 SDA / 24 SCL) → LiDAR 2 (0x29, dedicated bus)

**Key pin moves from phase-3:**

| Signal | Was | Now | Reason |
|---|---|---|---|
| BTN1 (arcade) | pin 25 | **pin 0** | pin 25 became Wire2 SDA |
| Expression pedal | A10 (pin 24) | **A17 (pin 41)** | pin 24 became Wire2 SCL |
| ENC7 switch | pin 41 | **underside pad 51** | freed pin 41 for the pedal |
| ENC5 B | pin 13 | **pin 8** | pin 13 is the onboard LED, can't `INPUT_PULLUP` |
| LiDAR XSHUT (both) | pins 0 + 8 | **removed** | LiDAR 2 moved to Wire2; no XSHUT needed |

---

## MIDI Map (channel 1)

| CC | Source |
|----|--------|
| 20–26 | ENC1–7 rotation (127 = CW step, 0 = CCW step) |
| 27 / 28 | Joystick X / Y |
| 29 | Expression pedal |
| 30 | VL53L1X #1 distance (0–127 over 2–46 cm) |
| 36 | VL53L1X #2 distance (0–127 over 2–46 cm) |
| 31 / 32 / 33 | IMU accel X / Y / Z |
| 34 / 35 | FSR1 / FSR2 (squared curve) |
| 78 / 79 | Contact mic L / R envelope |
| 60–68 | Buttons 1–9 (127 press, 0 release) |
| 70–76 | Encoder buttons 1–7 |
| 77 | Joystick click |

---

## Firmware

`TEENSYY/TEENSYY.ino`. Same code as old NEPTR plus:
- Dual `VL53L1X tof1, tof2` on **separate I²C buses** via Pololu `setBus()` — no XSHUT, no address re-writes (see init snippet below)
- MPU9250 raw Wire reads (register-compatible with MPU6050 — same 0x6B/0x3B sequence), presence checked at boot (`mpuok` flag, loop guarded)
- `NUMPIXELS` = 144 (full-strip cut length)
- `pixels.setBrightness(15)` low cap, suitable for Teensy-powered ~96-LED rainbow (~250 mA)
- Debug serial print active by default — see `loop()` near the end. Swap the block for `printMidiValues()` (still defined) to restore the full MIDI dump.

Libraries: `Encoder`, `Bounce`, `MIDIUSB`, `Adafruit NeoPixel`, `VL53L1X` (Pololu), `Wire`.

### Arduino IDE

| Setting | Value |
|---------|-------|
| Board | Teensy 4.1 |
| USB Type | Serial + MIDI |
| CPU | 600 MHz |


### Dual VL53L1X init sketch (two-bus, no XSHUT)

```c
#include <Wire.h>
#include <VL53L1X.h>

VL53L1X tof1, tof2;

bool configureTOF(VL53L1X &sensor, TwoWire &bus, const char *label) {
  sensor.setBus(&bus);
  sensor.setTimeout(50);
  if (!sensor.init()) { Serial.print(label); Serial.println(" FAIL"); return false; }
  sensor.setDistanceMode(VL53L1X::Long);
  sensor.setMeasurementTimingBudget(33000);
  sensor.startContinuous(50);
  return true;
}

// in setup():
Wire.begin();   Wire.setClock(400000);     // pins 18/19 — MPU + LiDAR 1
Wire2.begin();  Wire2.setClock(400000);    // pins 25/24 — LiDAR 2
delay(100);

// MPU9250 wake (Wire, reg 0x6B = PWR_MGMT_1, value 0 = wake)
Wire.beginTransmission(0x68);
Wire.write(0x6B); Wire.write(0x00);
bool mpuok = (Wire.endTransmission(true) == 0);

bool tof1ok = configureTOF(tof1, Wire,  "TOF1");
bool tof2ok = configureTOF(tof2, Wire2, "TOF2");
```

Both VL53L1X stay at the default `0x29` — they're on different buses so there's no collision.

---

## Shell

Phase-3 prints carried over to `shell/`:

| File | Role |
|------|------|
| `shell/NEPTR_p3_HEADv3.3mf` | upper head |
| `shell/NEPTR_p3_BODYv5.3mf` | lower body |
| `shell/neptr_p3_shell_ref.png` | reference render |

Run the LED strip through existing internal channels — the 2.7mm width fits where the old discrete-pixel wire routing went. No `.f3d` source survived; re-model from the `.3mf` if you need parametric changes.

---

## Housing Implementation

All hole diameters include +0.3–0.5mm print clearance. "Panel hole" = the opening in the shell face. "Mount holes" = screw holes through shell for fastening.

---

### Teensy 4.1

| Dimension | Value |
|-----------|-------|
| PCB | 61 × 18mm |
| Mounting holes | 4× Ø2.54mm, M2 screws |
| Hole centers (long axis) | 55.9mm apart |
| Hole centers (short axis) | 12.7mm apart |
| Standoffs | 3–5mm height recommended |

---

### KY-040 Rotary Encoder (×7)

| Dimension | Value |
|-----------|-------|
| Module PCB | ~40 × 40mm |
| Shaft | 6mm D-shaft |
| Bushing OD | 7mm (M7×0.75 threaded) |
| **Panel hole** | **Ø7.5mm** |
| Knob cap | Ø15mm × 16.5mm tall |

Nut from inside tightens bushing. KY-040 module sits recessed behind panel; only shaft protrudes.

---

### 7mm Tactile Pushbutton — PBS-110 (×8)

| Dimension | Value |
|-----------|-------|
| Button body OD | 6.9mm |
| Cap OD | 6mm |
| **Panel hole** | **Ø7.5mm** |

Clip/nut retention from inside. No screw holes — thread on bushing holds it.

---

### Arcade Button 30mm — EG STARTS (×1, BTN9)

| Dimension | Value |
|-----------|-------|
| Button body OD | 30mm |
| **Panel hole** | **Ø28.5mm** |
| Retention | Snap-in clip from inside (no screws) |

Body is 30mm but mounts through a 28mm hole — the lip catches the panel face.

---

### KY-023 Analog XY Joystick

| Dimension | Value |
|-----------|-------|
| Module PCB | 34 × 26mm |
| Total height (shaft up) | 32mm |
| Joystick cap OD | ~25mm |
| Shaft OD | ~4mm |
| **Panel hole** | **Ø27mm** (clearance for cap travel in all directions) |
| **PCB mounting holes** | 4× Ø3mm, M3 compatible |
| **Hole centers (long)** | **28mm** apart |
| **Hole centers (short)** | **20mm** apart |
| Hole inset from edge | ~3mm from each corner |
| Mount | PCB from inside; 4× M3 screws through corner holes into heat-set inserts in shell floor |

---

### TOF400C VL53L1X ToF Sensor (×2)

| Dimension | Value |
|-----------|-------|
| Module PCB | 20 × 12mm (stadium/pill shape) |
| Sensor dome (oval black housing) | ~12 × 8mm, upper half of PCB |
| **Shell aperture** | **13 × 9mm oval/rect slot** (dome faces outward through shell wall) |
| **Mounting holes** | **2× Ø2mm (M2)**, one in each side ear |
| **Hole center-to-center** | **~16mm** (long axis) |
| Mount | 2× M2 screws from outside into heat-set inserts in shell wall |

Two sensors: one on each side of the enclosure (left/right facing outward), screwed through shell wall.

---

### MPU9250 IMU (GY-9250)

| Dimension | Value |
|-----------|-------|
| Module PCB | 15 × 25mm |
| Panel hole | None — mounted inside |
| Mount | Hotglue or M2 screws through pin header holes |

---

### FSR402 Force Sensor (×2)

| Dimension | Value |
|-----------|-------|
| Active area | 38.1 × 38.1mm |
| Total sensor body | 43.7 × 43.7mm |
| Tail | 6mm wide × 0.3mm thick |
| Panel hole | None — sandwiched between PCB/surface and shell face |

Tail exits through a **7mm × 1mm slot** at the shell edge.

---

### Piezo Disc 20mm (×2)

| Dimension | Value |
|-----------|-------|
| Disc OD | 20mm |
| Thickness | ~0.5mm |
| Mount | Epoxy/CA glue to inner shell wall |
| Wire exit | **Ø2.5mm hole** through shell wall near disc |

No panel-facing hole needed — disc faces inward against shell wall, responds to taps transmitted through the shell.

---

### PJ-307 3.5mm TRS Jack

| Dimension | Value |
|-----------|-------|
| Bushing OD | 6mm (M6×0.75 threaded) |
| **Panel hole** | **Ø6.5mm** |
| Mount | M6 nut from inside |

---

### WS2812B LED Strip (2.7mm)

| Dimension | Value |
|-----------|-------|
| Strip width | ~10mm (5050 SMD PCB) |
| Strip + backing height | ~3mm total |
| **Internal channel width** | **11mm minimum, 13mm recommended** |
| Channel depth | 5mm |

5050 SMD is wider than the original 2.7mm spec — seam channel shelf in Blender must be ≥11mm wide.

---

### USB-C Panel Mount — IP67 (Pi 5 power)

| Dimension | Value |
|-----------|-------|
| **Panel hole** | **Ø16.3mm** (16mm nominal + 0.3mm) |
| Mount | Integral flange + nut — no separate screws |

---

### USB-A Panel Mount (Pi 5 spare USB port)

| Dimension | Value |
|-----------|-------|
| Connector aperture | 14.5 × 6.5mm |
| **Panel hole** | **15 × 7mm rectangular** |
| Mounting ears | 2× M3 screws, ~25mm apart (center-to-center) |
| **Screw holes** | **Ø3.5mm**, 25mm apart, centered on connector |

---

### Ethernet Panel Mount — Cat6 RJ45

| Dimension | Value |
|-----------|-------|
| RJ45 connector face | 16 × 13.5mm |
| **Panel hole** | **16.5 × 14mm rectangular** |
| Mounting screws | 2× M3, included, ~30mm apart |
| **Screw holes** | **Ø3.5mm**, 30mm apart, centered on connector |

---

### Pi 5 + Touchscreen Assembly

Screen (MPI3508, 3.5" HDMI) snaps onto the Pi 5 via HDMI + USB — they are one rigid assembly. Mount the Pi; the screen comes along with it. Use the Pi 5's mounting holes to secure the whole unit down.

**Pi 5 PCB**

| Dimension | Value |
|-----------|-------|
| PCB | 85 × 56mm |
| **Mounting holes** | 4× Ø2.7mm, M2.5 screws |
| Hole centers (long axis) | **58mm** apart |
| Hole centers (short axis) | **49mm** apart |
| Hole inset from edge | 3.5mm from each side |
| Standoff height | 5mm recommended (clears bottom-side components) |

Holes are at all four corners, 3.5mm in from each edge on both axes. Standard RPi hole pattern — M2.5 brass standoffs screw into shell floor, Pi drops onto them and secures with M2.5 nuts or screws from above.

**Touchscreen (MPI3508)**

| Dimension | Value |
|-----------|-------|
| Screen PCB | ~97 × 60mm |
| Active display area | ~74 × 49mm |
| **Panel cutout** | **75 × 50mm** (1mm bezel overlap on all sides) |

Screen face sits flush at the panel cutout; Pi + screen assembly is recessed inside the body behind it. No separate screen mounting holes needed — the Pi's 4× M2.5 anchor points hold the entire assembly.

---

## Files

```
NEPTR phase4/
├── plan.md
├── PHASE4IMPLEMENTATION.md          ← Pi-side audio engine (DEMIURGE_OS)
├── live.conf                        ← DEMIURGE launch config
├── TEENSYY/
│   ├── TEENSYY.ino                  ← firmware to flash
│   ├── TEENSYY_phase3_reference/    ← phase-3 archive (own folder so Arduino doesn't compile it with the main sketch)
│   ├── lidar_test/                  ← minimal dual-VL53L1X sanity test
│   ├── busscan/                     ← I²C bus scanner (both Wire and Wire2)
│   ├── tof1_diag/                   ← LiDAR 1 isolation diag (XSHUT-era artifact)
│   └── tof2_diag/                   ← LiDAR 2 isolation diag (XSHUT-era artifact)
├── shell/
│   ├── NEPTR_p3_HEADv3.3mf
│   ├── NEPTR_p3_BODYv5.3mf
│   └── neptr_p3_shell_ref.png
├── chuck/ csound/ presets/ docs/
├── scripts/
│   ├── sync_to_pi.sh
│   ├── claude_midi.sh
│   ├── ... (other scripts)
│   └── neptr_visualizer.py          ← Mac pygame UI mirroring the Teensy serial stream live
└── neptrPhase4_UI.py
```
