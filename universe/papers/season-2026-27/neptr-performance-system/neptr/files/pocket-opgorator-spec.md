# Pocket OpGorator

**Project: EMGOR SYNTH | Date: 3/25/26 | Target Done: 7/1/26**

---

## Purpose

Custom pocket-operator-format sampler/synth. NeoTrellis 4×4 velocity-sensitive pads, 2 encoders, OLED display, USB-C external port (data + charging), built-in mic + stereo speakers, 3.5mm stereo I/O, SD card, MPU9250 IMU, VL53L1X ToF sensor, battery powered. Standalone or USB audio/MIDI.

**Form factor:** Main PCB true outline **75×140mm** (from the Edge_Cuts gerber). Custom organic **"pebble/lozenge"** 3D-printed PETG enclosure, ~**100(X) × 141(Y) × 46(Z) mm** at its widest/longest/deepest — a curved hourglass/peanut silhouette (width pinches to 77mm at the top, middle & bottom, bulging to 100mm at the quarter points). Between Pocket Operator (78×156×21mm) and iPhone-15 (72×148×8mm) in plan footprint; thicker because the internal battery + Daisy stack sits in a ~26mm cavity behind the PCB. (See the **Enclosure** section for the authoritative housing spec; the older PCB-Layout/KiCad sections still use the legacy 84×150 design grid and are not the housing reference.)

**Libraries:** DaisyDuino (electrosmith), Adafruit_GFX + Adafruit_SSD1306, Adafruit_NeoTrellis + Adafruit_seesaw, SparkFun VL53L1X Arduino Library (**NOT** VL53L0X), bolderflight/MPU9250, Encoder (Stoffregen), Bounce2, SD (built-in)

**USB:** External USB-C port (housing **south wall**) is a **USB-C female breakout board**, wired by short leads to a **micro-USB Type-B male** plug that goes into the Daisy Seed's **micro-USB port**. Daisy Seed itself is micro USB — *all* references in this doc treat the Daisy port as micro USB and the external port as USB-C (via the breakout). USB carries Audio + MIDI + programming. (Verified May 2026: even the current Rev7 Daisy Seed uses micro USB; the Electrosmith "you may also like" sidebar still suggests a micro-USB cable, and the official forum thread "Adapting USB-C into the micro USB on the Seed" confirms this.)

---

## Verification Status (verified against actual product listings and datasheets, May 2026)

| Item | Status | Notes |
|------|--------|-------|
| Daisy Seed = micro USB on all revisions | ✅ Verified | Electrosmith product page; forum thread "Adapting USB C into the micro USB on the Seed". Rev7 is the current ship; still micro USB. |
| Daisy Seed USB VBUS ↔ VIN isolation | ✅ Verified | Seed schematic (via Electrosmith forum "Seed power supply" thread): onboard **diodes D1 & D2 isolate USB VBUS from the 5V/VIN regulator rail.** VIN **cannot** backfeed voltage out the USB connector. → The single external USB-C port can safely share VBUS between Daisy micro-USB pin 1 AND TP4056 IN+; boost 5V on VIN never reaches the charger input. **No external Schottky needed.** Powering from USB + battery simultaneously is a supported condition (highest of the two sources feeds the regulator). |
| Daisy Seed VIN voltage range | ✅ Verified | Onboard regulator is a **TPS62172 buck**, rated to **17V** in (≈12V practical ceiling when USB is also connected). Earlier "3.3–6V" note was overly conservative. We feed **5V** from the XL3608, well within range. |
| PJ-307 5-pin pinout: pin 4 NC-to-TIP, pin 5 NC-to-RING | ✅ Verified | Confirmed by Amazon B07XX8YGDX customer review: "The input to the tip and ring are the outermost pins and they pass the signal to the two innermost pins only when no headphones are plugged in." Rated DC 30V / 1.0A. Mic auto-switch wiring works as-designed. |
| DWEII 2.42" SSD1309 OLED — driver, init, mode | ✅ Verified | Adafruit_SSD1306 library works (multiple Amazon reviews confirm). Defaults to SPI mode. Onboard boost present (SSD1309 doesn't have a charge pump, but the breakout has its own 12V boost). Community uses `SSD1306_SWITCHCAPVCC` — code updated to match. RES pin connection required for SPI init; ours is on D2 ✓. |
| ACEIRMC TOF400C VL53L1X — range and FOV | ✅ Verified | Working voltage 3–5V, working current 40mA max, FOV 27°, range 0.04–4m, **blind spot 0–4cm**. Code updated to filter `dist > 40` (was 30). I2C 0x29. |
| CYT1100 encoder — pin layout, shaft length, EC11E footprint compat | ✅ Verified | 20mm shaft, 5 pins (3-pin row + 2-pin row), 4 counts per detent, 20 detents/rev. EC11E footprint compatible. Standard "Encoder library" usage. |
| MAX4466 mic — supply voltage recommendation | ⚠️ Updated | Adafruit / Maxim explicitly recommend the quietest available rail (3.3V on Arduino). Doc updated to power MAX4466 from +3.3V, not +5V. Module dimensions ~20×15mm with electret capsule on one end (orient so capsule is centered under the 3mm front-face mic hole). |
| EG1218 switch current rating | ❌ Was wrong, FIXED | E-Switch datasheet: 200mA / 30VDC, body 11.6×4×7.4mm, **1.5mm pin pitch (not 2.5mm)**. The previous doc claimed 3A — totally wrong. Now used to drive a P-MOSFET load switch instead (microamps gate current). |
| VKLSVAN "Super Mini PAM8403" (B0DPMDPGR6) compatibility | ✅ Verified compatible | Re-checked from board photos: the listing's "USB Power Supply" title is misleading — there is NO USB connector on the board. It's a standard pin-pad PAM8403 with 5 input pads (+/L/R/5V/−) on the bottom edge and 4 speaker output pads (L+/L−/R−/R+) on the top edge. Wire normally; see `pcb-components.md` §6. |
| OLED connector edge position | ⚠️ Vendor-variant | Several Amazon reviews on B0B2R57SCJ report receiving units with the connector on the side rather than the short edge. Confirm yours has the 7-pin header on the short edge before locking J_OLED layout — if it's on the long edge, J_OLED needs to move to (x=37.5, y=30) running in +x direction. |

---

## Parts List

| Part | Qty | Link |
|------|-----|------|
| Daisy Seed (STM32H750, PCM3060 codec, **micro USB**) | 1 | [electro-smith.com/daisy/daisy](https://electro-smith.com/daisy/daisy) |
| Machine pin female headers, 1×40P 2.54mm breakable (InnoHHustle) | 1 pk | [a.co/d/0g6su2EM](https://a.co/d/0g6su2EM) |
| CYT1100 PCB-mount rotary encoder w/ push-switch (5-pin) | 2 | [a.co/d/0iIx9NnL](https://a.co/d/0iIx9NnL) |
| 2.42" 128×64 OLED SSD1309 (white, 7-pin SPI) | 1 | [a.co/d/04MArlWi](https://a.co/d/04MArlWi) |
| MPU9250 9-axis IMU module (I2C, 0x68 IMU + 0x0C mag) | 1 | [a.co/d/06vXOllL](https://a.co/d/06vXOllL) |
| ACEIRMC TOF400C VL53L1X ToF module (I2C, 0x29, ~4m) | 1 | [a.co/d/0bEPANZ4](https://a.co/d/0bEPANZ4) |
| PJ-307 3.5mm stereo switched jack (5-pin PCB mount) | 2 | [a.co/d/03WEDsMu](https://a.co/d/03WEDsMu) |
| SF45-65 Force Sensitive Resistor (45×45mm active, 50×65mm body) | 1 | [a.co/d/0dkBeHya](https://a.co/d/0dkBeHya) |
| MAX4466 Mic Preamp Module | 1 pk | [amazon.com/dp/B0D3CWM68X](https://www.amazon.com/dp/B0D3CWM68X) |
| NeoTrellis PCB (Adafruit #3954) | 1 | [adafruit.com/product/3954](https://www.adafruit.com/product/3954) — 60×60mm PCB |
| NeoTrellis Silicone (Adafruit #1611) | 1 | [adafruit.com/product/1611](https://www.adafruit.com/product/1611) — 60×60mm pad |
| TP4056 USB-C LiPo Charger (w/ DW01A) | 1 | [amazon.com/dp/B08FSRV7GS](https://www.amazon.com/dp/B08FSRV7GS) |
| JLJLUP 10000mAh 3.7V LiPo (LP1260110, BMS, PH2.0) | 1 | [a.co/d/04zlCEU5](https://a.co/d/04zlCEU5) — 60×112×12mm |
| XL3608-5V 2A Step-Up Boost Converter | 1 | [a.co/d/03kjqLwE](https://a.co/d/03kjqLwE) — select XL3608-5V variant |
| PAM8403 2×3W Stereo Class D Amp — VKLSVAN Super Mini (pin-pad variant) | 1 | [Amazon B0DPMDPGR6 — VKLSVAN 3-pack](https://www.amazon.com/dp/B0DPMDPGR6). The board has NO USB connector (the title is misleading). Bottom edge: 5 input pads (+/L/R/5V/−). Top edge: 4 speaker output pads (L+/L−/R−/R+). |
| uxcell 20×40mm 8Ω 2W rectangular speaker (5.5mm thick; side-firing into the "ears") | 2 | [a.co/d/07LGL4J69](https://a.co/d/07LGL4J69) |
| WWZMDiB SPI Micro SD module (6-pin, 3.3V/5V) | 1 | [a.co/d/073lXLqU](https://a.co/d/073lXLqU) |
| SanDisk Ultra 16GB microSDHC Class 10 (FAT32) | 1 | [a.co/d/0bnKyEuZ](https://a.co/d/0bnKyEuZ) |
| QSYZAIL 122-pc 2.54mm female header kit | 1 | [a.co/d/01YxhAwZ](https://a.co/d/01YxhAwZ) |
| 100nF ceramic caps | 12+ | [a.co/d/00Ju4G9m](https://a.co/d/00Ju4G9m) |
| 10uF electrolytic caps | 6+ | standard stock |
| 100uF electrolytic cap | 1 | standard stock |
| Resistor kit (incl. 10k, 100k) | 1 | [a.co/d/0bfYrMGL](https://a.co/d/0bfYrMGL) |
| **EG1218 SPDT slide switch + AO3401 P-channel MOSFET (load switch)** | 1 ea | EG1218 is only 200mA rated (E-Switch datasheet — I was wrong in the previous BOM). For 2A+ battery path we drive a P-MOSFET load switch from the EG1218 — switch carries only gate current (microamps), MOSFET handles the 2.6A peak. See Power Circuit. AO3401 (SOT-23, ~50mΩ Rds(on), 4A) is the cheap default; DMG3415U is interchangeable. EG1218 dimensions: 11.6×4×7.4mm body, 1.5mm pin pitch — use KiCad footprint `Button_Switch_THT:SW_Slide_1P2T_EG1218`. |
| USB-C breakout board (Type-C female, 16-pin basic breakout PCB) | 1 | [a.co/d/0dv4nUcr](https://a.co/d/0dv4nUcr) — external USB-C jack, mounted at the SOUTH-wall opening; pads wired by short leads to the micro-USB male below |
| Micro-USB Type-B male solder connector (5-pin, w/ black cover) | 1 | [a.co/d/09tX9iMK](https://a.co/d/09tX9iMK) — plugs into the Daisy Seed's micro-USB port; wired to the USB-C breakout |
| Polyester quilt batting / foam sheet ~2mm | 2 sheets | Any craft store — cut to fit; one between PCB and battery, one between battery and the back-cavity stack |
| Socket Head Cap Screws, M2-0.4 × 30mm, 304 stainless (30 pc) — housing front↔back fastening | 1 pk | [a.co/d/00Es7j9G](https://a.co/d/00Es7j9G) |
| M2 brass heat-set inserts (soldering-iron heat-set; melted into SOLID front-shell boss cylinders, no pilot bore) | 4+ | Standard stock — M2 |
| 3D Print Enclosure (custom PETG "pebble/lozenge" shell, printed in 2 halves) | 1 | Custom |

**Battery connector note:** JLJLUP ships with PH2.0 — cut the connector, solder leads directly to TP4056 B+/B− pads. Verify polarity (red = B+, black = B−) before soldering.

**USB-C connection note:** The external USB-C port is a **Type-C female breakout board** (16-pin basic breakout) mounted at the **SOUTH-wall** opening (Enclosure Cutouts #10). Its VBUS / GND / D+ / D− (and CC) pads are **wired by short flying leads** to a **micro-USB Type-B male solder connector**, which plugs into the **Daisy Seed's micro-USB port** (the Daisy is micro-USB on all revs). USB carries Audio + MIDI + programming/DFU. **Because this is a flexible wired hop — not a rigid panel-to-board tie — the Daisy Seed's location and port orientation are NOT constrained by the south-wall opening; the Daisy stays exactly where it's convenient.** The breakout board is fixed to the south wall (its curved cap may want a small flat landing — see Cutout #10 TBD); secure the micro-USB male into the Daisy with a dab of hot glue for strain relief.

**Power switch note:** EG1218 is **only 200mA rated** (verified against E-Switch datasheet). For the 2A+ battery-path duty, the EG1218 drives a P-MOSFET load switch (AO3401 or DMG3415U) — the slide switch sees only microamps of gate current; the MOSFET carries the load. This is the standard production load-switch pattern. EG1218 body 11.6×4×7.4mm, 1.5mm pin pitch (not 2.5mm — I had this wrong before).

**PAM8403 note:** The VKLSVAN "Super Mini PAM8403" (B0DPMDPGR6) is the verified part — despite the misleading title, it has NO USB on board, just standard pin-pad inputs. Wire normally per §6 of `pcb-components.md`. SHDN/MUTE are not exposed; that's fine since this design doesn't use firmware mute.

---

## Signal Flow

**Audio IN (mic auto-switch, hardware, zero firmware):**
MAX4466 mic → PJ-307 input jack pin 4/5 (NC-to-TIP/RING) → TIP/RING when no plug inserted → Daisy AUDIO_IN_L/R
When a 3.5mm plug is inserted, the NC switch contacts physically lift off TIP/RING and the external line takes over.

**Audio OUT:**
Daisy AUDIO_OUT_L + AUDIO_OUT_R → both: PJ-307 output jack TIP/RING (headphones) and PAM8403 L_IN/R_IN (speakers, via 100nF DC-blocking caps if PAM8403 module lacks input caps — most have them).
Stereo speakers driven by PAM8403 outputs.

**Speaker mute on headphone insertion (firmware-controlled, hardware-detected):**
PJ-307 output jack pin 4 (NC-to-TIP) → 10kΩ pullup to 3.3V + 100nF filter cap to GND → Daisy D19 (HP_DETECT). Daisy reads D19 every loop: LOW = no plug (TIP DC-bias dominates) → drive D18 (SHDN_CTRL) HIGH → PAM8403 enabled. HIGH = plug inserted (pin 4 floats, pullup wins) → drive D18 LOW → PAM8403 muted. Default boot state: SHDN_CTRL HIGH (speakers on).

Why this hybrid: the PJ-307's switch is referenced to TIP (not GND), so direct hardware SHDN control off the jack switch picks up audio AC on the gate and is unreliable. Filtering the switch contact through an RC and reading it as a digital input lets the Daisy do the polarity inversion cleanly. Adds two GPIOs and three passives.

**Sample storage:**
Daisy SPI1 → WWZMDiB SD card module (CS=D0, separate from OLED CS=D7).

**Controls:**
- NeoTrellis (16 buttons, 32 NeoPixel LEDs): I2C1 bus
- MPU9250 IMU (0x68) + VL53L1X ToF (0x29): I2C4 bus (D13 SCL / D14 SDA)
- 2× CYT1100 encoders: GPIO (A, B, SW per encoder = 6 pins total)
- FSR: Analog input (A0)

**USB:** External USB-C jack (breakout) → micro-USB male → Daisy Seed micro-USB → stereo audio interface (48kHz/24-bit) + MIDI device + programming.

**Modes:**
- **Standalone:** pads trigger internal synth/samples, mic → DSP → speakers/headphones
- **Recording:** mic → DSP → USB out to computer; computer playback → Daisy DAC for monitoring
- **Outboard:** computer → USB in → DSP effects → USB out
- **Controller:** MIDI-only, DSP muted

### PO / Volca Sync (software, no extra hardware)

3.5mm jacks double as sync I/O. ~15ms 1kHz click burst at 2 PPQN. Detected/generated by DSP.

| Mode | Direction | LEFT channel | RIGHT channel |
|------|-----------|--------------|---------------|
| SY0 | — | Audio L | Audio R |
| SY1 | Master | Click out | Audio (mono) |
| SY2 | Master | Audio (mono) | Click out |
| SY3 | Slave | Click in | Audio (mono) |
| SY4 | Slave | Audio (mono) | Click in |
| SY5 | Passthrough | Sync I/O | Audio I/O (mono chain) |

Any mode other than SY0/SY5 forces mono audio. USB-MIDI clock sync: use SY0.

SD card (SPI via WWZMDiB module) stores WAV samples, session files, recorded audio. 16GB FAT32 = ~24hrs 48kHz/24-bit stereo WAV.

---

## Daisy Seed — Key Specs

- **MCU:** STM32H750 @ 480MHz, 64MB SDRAM
- **Codec:** PCM3060 24-bit stereo, AC coupling on both in and out, ~1Vrms output — no external DAC/ADC or coupling caps needed
- **GPIO:** 31 user pins, I2C×2 user-accessible (I2C1, I2C4 — I2C2 is reserved by the onboard PCM3060 codec), SPI×3, **micro USB** (audio + MIDI + programming)
- **Dimensions:** 60.96mm × 25.4mm PCB, ~12mm tall seated on machine pin female headers
- **Mounting:** PCB bottom side (back-facing), socketed on machine pin female headers (removable)
- **Power:** VIN accepts ~3.5–17V (onboard **TPS62172 buck** regulator — verified via Seed schematic/forum; old "3.3–6V" note was too conservative). Feed it **5V** from the XL3608-5V output. Onboard **D1/D2 diodes isolate USB VBUS from the VIN/5V rail — VIN cannot backfeed to the USB connector**, so VBUS may be safely shared with TP4056 IN+.
- **Headers:** Machine pin 1×40P (InnoHHustle, breakable) — snap to 2× 20-pin rows

---

## Daisy Seed Pin Assignments

Verified against official pinout at [daisy.audio/tutorials/_a9_Getting_Started-I2C/](https://daisy.audio/tutorials/_a9_Getting_Started-I2C/) and the DaisyWiki CSV.

**I2C note:** I2C2 is reserved by the onboard PCM3060 codec — not available for user code. The two user-accessible buses are I2C1 (D11/D12) and I2C4 (D13/D14).

| Function | Daisy Pin | Notes |
|----------|-----------|-------|
| SD CS | D0 | |
| OLED DC | D1 | |
| OLED RST | D2 | |
| Encoder 1 A | D3 | Left encoder |
| Encoder 1 B | D4 | Left encoder |
| Encoder 1 SW | D5 | Left encoder click |
| Encoder 2 A | D6 | Right encoder |
| OLED CS | D7 | SPI1_NSS used as GPIO output |
| SPI SCK | D8 | SPI1, OLED + SD shared bus |
| SPI MISO | D9 | SPI1, SD card only |
| SPI MOSI | D10 | SPI1, OLED + SD shared bus |
| I2C1 SCL | D11 | NeoTrellis |
| I2C1 SDA | D12 | NeoTrellis |
| I2C4 SCL | D13 | MPU9250 + VL53L1X |
| I2C4 SDA | D14 | MPU9250 + VL53L1X |
| FSR | D15 (A0) | Analog |
| Encoder 2 B | D16 (A1) | Right encoder |
| Encoder 2 SW | D17 (A2) | Right encoder click |
| **SHDN_CTRL** | **D18** | **Output → PAM8403 SHDN (HIGH=on, LOW=mute)** |
| **HP_DETECT** | **D19** | **Input ← PJ-307 output pin 4 via RC filter** |
| VBAT monitor | D20 (A5) | 100K/100K divider from TP4056 OUT+ |
| VL53L1X XSHUT | — | Tie high via 10kΩ to 3.3V, no GPIO needed |
| VL53L1X GPIO1 | — | NC |
| Audio IN L | AUDIO_IN_L | From PJ-307 input jack TIP |
| Audio IN R | AUDIO_IN_R | From PJ-307 input jack RING |
| Audio OUT L | AUDIO_OUT_L | To PAM8403 L_IN + headphone TIP |
| Audio OUT R | AUDIO_OUT_R | To PAM8403 R_IN + headphone RING |
| VIN | 5V from XL3608 | |
| GND | GND | |

---

## Power Circuit

**Single USB-C external port architecture (one cable for data + charging):**

A **USB-C female breakout board** is mounted at the **SOUTH-wall** opening (Enclosure Cutouts #10); USB-C cables plug into it from outside the housing. Its pads are wired by short flying leads to a **micro-USB Type-B male connector** that plugs into the Daisy Seed's micro-USB port. The wired hop is flexible, so the **Daisy Seed's placement is independent of the south-wall port location.**

```
External USB-C jack (breakout, south wall) ── flying leads ── micro-USB male → Daisy Seed
                                                                │
                                              data (Audio + MIDI + DFU) flows over USB
                                                                │
                                  Daisy VBUS pad ── 30AWG wire ── TP4056 IN+ (battery charging path)
```

**Main power path (battery → load) with hard-cut power switch via P-MOSFET load switch:**

```
LiPo (3.0–4.2V)
   │
   ▼
TP4056 + DW01A   ─── always live: handles charging from USB, overdischarge cutoff
   │ (OUT+ = VBAT_RAW)
   ▼
P-MOSFET Q1 (AO3401)  ─── load switch
   source = VBAT_RAW
   drain  = VBAT_SW → XL3608 VIN+
   gate   = SW_GATE net, with 10kΩ pull-up (R_GATE) to source
   │
   ▼
EG1218 SPDT switch (200mA gate-only duty)
   Pin 2 (COM)  = SW_GATE
   Pin 1        = GND     ← MOSFET ON position (Vgs = -VBAT → conducts)
   Pin 3        = VBAT_RAW ← MOSFET OFF position (Vgs = 0 → cuts off)
   │
   ▼
XL3608-5V boost   ─── 5V rail, ~2A rated
   │
   ├──→ Daisy Seed VIN (5V)
   ├──→ PAM8403 VIN (5V)
   ├──→ NeoTrellis VIN (5V; onboard 3.3V LDO for seesaw, WS2812 on 5V)
   ├──→ MAX4466 VIN (3.3V) — see audio note below; mic is fed from Daisy 3V3 not 5V
   └──→ SD card module VIN (5V)
```

**Switch behavior:**
- **Switch ON (Pin 1–2 shorted):** SW_GATE pulled to GND. Vgs = 0 − VBAT_RAW ≈ −3.7V → P-MOSFET fully on. VBAT_RAW reaches XL3608 VIN+ through Rds(on) ~50mΩ (negligible drop). Boost runs. 5V rail live. Daisy + modules powered.
- **Switch OFF (Pin 2–3 shorted):** SW_GATE pulled to VBAT_RAW. Vgs = 0 → P-MOSFET cut off. XL3608 has no input. No 5V rail. Daisy off. PAM8403 SHDN floats LOW via R_SHDN pulldown → speakers definitively off. R_GATE pull-up keeps gate at a defined level in both positions.
- **USB charging works either way:** TP4056 is upstream of the MOSFET; USB → TP4056 IN+ → battery charge continues regardless of switch position.
- **Backfeed protection:** TP4056 is one-way; P-MOSFET body diode points from drain → source (battery side), which is the safe direction; XL3608 has internal diode on VIN; computer USB stays protected.
- **Daisy VIN → USB VBUS backfeed: blocked in-hardware (verified).** The Seed's onboard **D1/D2 isolation diodes** sit between USB VBUS and the 5V/VIN regulator rail, so the XL3608's 5V on Daisy VIN **cannot** appear on the Daisy's USB VBUS pin. Therefore the external USB-C VBUS node (shared between Daisy micro-USB pin 1 and TP4056 IN+) stays clean when running on battery — no phantom 5V on the charger input, no recirculating charge loop. **No external Schottky required.** Source: Seed schematic via Electrosmith forum "Seed power supply" thread.

**Why a MOSFET load switch (not a direct slide switch in the battery path):** Battery-side current is ~1.8A typical / 2.6A peak. No reasonable-size 2.54mm or 1.5mm-pitch slide switch is rated for that. The standard production solution is to let the slide switch drive a P-channel MOSFET gate (microamps) and let the MOSFET (4A+ rated, ~50mΩ Rds(on)) handle the current. AO3401 in SOT-23 dissipates 0.4W at 2A — well under its 0.5W package limit. The previous version of this doc claimed the EG1218 was 3A; that was wrong (E-Switch datasheet: 200mA / 30VDC).

**Why the switch is in the battery path (not on XL3608 EN):**
Putting the switch on EN (low-current path) only kills the 5V rail. If USB is plugged in, the Daisy can still draw power directly from its USB VBUS — making the OFF position non-authoritative. With the switch in the battery path, OFF = device truly off when running on battery. On USB, the Daisy stays alive via VBUS regardless (and that's fine — it's how programming/firmware updates work with the device "off").

**Key solder connections (bypassing module ports):**
- Daisy VBUS pad → TP4056 IN+ pad: one 30AWG wire
- All other connections via PCB traces / header wires as normal

**Module notes:**
- **XL3608:** output fixed 5V (select -5V variant, no trimpot); verify with multimeter before wiring downstream.
- **TP4056 w/ DW01A:** 1A charge rate, overdischarge cutoff ~2.5V.
- **JLJLUP 10000mAh (LP1260110):** PH2.0 → cut connector, solder leads directly to TP4056 B+/B−. Verify polarity. 60×112×12mm. 37Wh capacity label-confirmed (volume × 460 Wh/L = 37.1 Wh ✓). ~5.5h moderate use, ~3–3.5h heavy use. At 1.15A/5V typical draw: battery-side current ≈ (5×1.15)/(3.7×0.85) = 1.83A = 0.18C — no brownout risk.
- **EG1218 SPDT slide switch:** 10.2×4×4mm body, 3mm slider above body, 3-pin 2.5mm pitch. 3A rated — handles ~2.6A peak battery-path current comfortably. Mounted on PCB top side, slider knob protrudes through the front-shell slot at PCB (38, 5.125) → shell (40.5, 5.625), in the front-face dip. See the Enclosure Cutouts (Locked) table.

**5V rail bypass:** 100nF + 10uF at every module VCC pin. NeoTrellis: 100uF bulk cap on VCC. XL3608 output: 10uF bulk cap.

**SHDN default-low protection:** 10kΩ from PAM8403 SHDN to GND. When Daisy is off / unprogrammed / in reset, SHDN floats low → amplifier muted. Prevents pop/screech during power events.

**VBAT divider:** TP4056 OUT+ → 100kΩ → D20 (A5) → 100kΩ → GND (+ 100nF cap to GND for noise filtering).

---

## Power Budget (at 5V)

| Component | Current (typical) |
|-----------|-------------------|
| Daisy Seed | 250mA |
| NeoTrellis (80% brightness cap in firmware) | ~400mA typical, 1120mA max |
| OLED | 20mA |
| PAM8403 (moderate volume) | 400mA |
| MPU9250 | 4mA |
| VL53L1X | 20mA |
| MAX4466 | 15mA |
| SD card module | 50mA |
| XL3608 quiescent | 5mA |
| **Total typical** | **~1.15A @ 5V** |
| **Total peak** | **~1.9A @ 5V** |

XL3608-5V rated 2A — adequate at 5V output. Battery-side equivalent: 1.83A typical, 2.6A peak — well within EG1218 3A rating.

NeoTrellis brightness capped at 80% in firmware to stay within rail budget.

---

## Audio Wiring

### Input Jack (PJ-307, hardware mic auto-switch)

```
PJ-307 INPUT JACK
   Pin 1 TIP        ──→ Daisy AUDIO_IN_L
   Pin 2 RING       ──→ Daisy AUDIO_IN_R
   Pin 3 SLEEVE     ──→ GND
   Pin 4 (NC→TIP)   ──→ MAX4466 OUT
   Pin 5 (NC→RING)  ──→ MAX4466 OUT     (mono mic feeds both channels)
```

**Behavior:**
- No plug: pins 4 and 5 are mechanically pressed against TIP and RING by the jack's spring contacts → MAX4466 OUT reaches both Daisy ADCs. Mic is live.
- Plug inserted: the inserted plug's TIP/RING physically lift the spring contacts off pins 4/5 → MAX4466 OUT is disconnected from TIP/RING. External signal takes over.

PCM3060 handles AC coupling internally; no series caps needed.

### Output Jack (PJ-307) + PAM8403 + Speakers + Firmware-controlled mute

```
PJ-307 OUTPUT JACK
   Pin 1 TIP        ──→ Daisy AUDIO_OUT_L ──→ PAM8403 L_IN
   Pin 2 RING       ──→ Daisy AUDIO_OUT_R ──→ PAM8403 R_IN
   Pin 3 SLEEVE     ──→ GND
   Pin 4 (NC→TIP)   ──→ R_HPDET (10kΩ to 3.3V) ──┬──→ Daisy D19 (HP_DETECT)
                                                  └──→ C_HPDET (100nF to GND)
   Pin 5            ──  NC

PAM8403
   L_IN+   ←── Daisy AUDIO_OUT_L (via PJ-307 TIP node)
   L_IN−   ←── GND
   R_IN+   ←── Daisy AUDIO_OUT_R (via PJ-307 RING node)
   R_IN−   ←── GND
   SHDN    ←── Daisy D18 (SHDN_CTRL) + R_SHDN 10kΩ pulldown to GND
   L_OUT+/− → Left speaker (uxcell 20×40mm 8Ω 2W)
   R_OUT+/− → Right speaker
   VIN     ←── 5V rail
   GND     ←── GND
```

**Mute logic (firmware, runs every loop iteration):**
```
HP_DETECT == LOW   → no plug → digitalWrite(SHDN_CTRL, HIGH) → speakers ON
HP_DETECT == HIGH  → plug inserted → digitalWrite(SHDN_CTRL, LOW) → speakers OFF
```

Why DC works here: when no plug, pin 4 is shorted to TIP. TIP carries AC audio centered on ~0V DC (Daisy outputs are AC-coupled). The 1µF cap forms an RC lowpass with the 10kΩ pullup at ~16Hz, suppressing all audible audio AC; the DC level on HP_DETECT settles near 0V (TIP's source impedance is much lower than 10kΩ) → reads LOW. When plug inserted, pin 4 floats; only the pullup pulls HP_DETECT → reads HIGH. Firmware also polls D19 on a 50ms cadence with state-change debounce, so a single noisy sample can't flip SHDN.

**PAM8403 input notes:** The VKLSVAN Super Mini PAM8403 used here has its IN− pins tied to GND internally on the module (single-ended input from L and R pads). Bottom edge of the module: + / L / R / 5V / − — both + and 5V are VCC duplicates. No USB connector on the board despite the listing title.

**MAX4466 power rail:** drive the mic from **+3.3V**, not +5V. The MAX4466 op-amp datasheet (and Adafruit's product notes) explicitly recommend the quietest available rail for lowest noise floor; on Daisy that's the 3V3 LDO output, which is regulated linearly and has far less switching noise than the +5V boost rail.

**Speaker HPF in firmware:** the audio callback applies a 1-pole 250Hz HPF on the DSP→DAC path. 20×40mm drivers can't reproduce below ~300Hz cleanly, and the HPF prevents Xmax-exceed at high volume. The HPF is always on; headphones get it too, but 250Hz is well below hearing damage threshold and the cut is mild (-6dB/oct).

### PAM8403 module pinout — VERIFY before laying out

The original BOM specified two channel-separate 5-pin headers (J_PAM8403_L + J_PAM8403_R). That's non-standard. The PAM8403 modules typically sold on Amazon (a.co/d/09upoQPh and similar) are *one of three* layouts; before laying out the PCB, verify yours against this list:

| Variant | Input header | Output | Note |
|---------|--------------|--------|------|
| A (common, 6-pin DIP) | GND, L_IN, R_IN, VCC, MUTE/SHDN, GND | screw terminals (4) | Use single 6-pin header J_AMP |
| B (5-pin) | VCC, GND, L_IN, R_IN, SHDN | 4 solder pads | Use single 5-pin header J_AMP |
| C (8-pin, less common) | L_IN+, L_IN−, GND, VCC, GND, R_IN+, R_IN−, SHDN | 4 solder pads | Use single 8-pin header J_AMP |

PCB design uses a single combined header `J_AMP` with pin count matched to your variant. If you've already bought the module, count the input pins on it — that's your answer. The doc and schematic below assume the 5-pin variant (B); adjust if needed.

---

## PCB Layout

**Board:** **75mm (X, width) × 140mm (Y, length)**, portrait orientation, double-sided, 1.6mm FR4. True outline from the Edge_Cuts gerber. (Replaces the legacy 84×150mm design grid.)

> **SOURCE OF TRUTH:** Component **feature** positions below are now **CALIPER-LOCKED to the housing "Enclosure Cutouts (Locked)" table** (Cutouts #1–10). The ¼-grid columns are only a **conceptual starting reference** — locked components may **not** sit exactly on the idealized quarter lines. Where the two disagree, the Enclosure Cutouts table wins.
>
> **Frame:** all coordinates use PCB **TOP-LEFT corner** as origin — X from left edge 0→75, Y from top edge 0→140.
>
> ⚠️ **Feature-center vs KiCad footprint origin:** the X/Y values below are the **visible-feature centers** (shaft, window, capsule, slider) that the housing holes align to. A KiCad **footprint origin** may sit a few mm off the feature center (e.g. an encoder pin-array centroid vs its shaft, a module header row vs its sensor window). **Verify the origin offset in KiCad** for every part and place so the *feature* lands on the locked coordinate.
>
> ℹ️ **USB-C is a wired breakout — Daisy placement is NOT tied to it.** The external USB-C port lives on the **SOUTH wall (Y=140 end)** as a **USB-C female breakout board** (Enclosure Cutouts #10), wired by short flying leads to a **micro-USB Type-B male** that plugs into the Daisy. Because that's a flexible wired hop (not a rigid panel-to-board connector), **the Daisy Seed's location and orientation are independent of the USB-C opening** — place the Daisy wherever it fits best in the rear cavity. (The old "Daisy USB at y=19 from top, all coords derived from it" constraint no longer applies; the Daisy is simply free to be positioned for fit.)

### ¼-Grid X-Position System (Quadrant Logic) — *conceptual reference only*

On the **75mm-wide** board the 4 equal vertical columns are **18.75mm** each. The quarter lines below are a starting reference; **final positions are caliper-locked to the Enclosure Cutouts table**, so most parts deviate from these lines.

| Column | x | Grid position |
|--------|---|---------------|
| Left (¼) | **18.75mm** | board width × 0.25 |
| Center (²⁄₄) | **37.5mm** | board width × 0.50 |
| Right (¾) | **56.25mm** | board width × 0.75 |

18.75mm from left edge → 18.75mm to center → 18.75mm to right line → 18.75mm to right edge. (Was 21/42/63 on the legacy 84mm board.) **Note:** the locked encoders sit at X 18.73 / 57.9 — close to but not on the ¼/¾ lines — and the mic/switch are well off-grid (mic X≈11, switch centered X≈38). The Enclosure Cutouts table is authoritative.

### Row 0 — NORTH EDGE (Y=0 wall) — ports

| Port | Center X | Notes |
|------|----------|-------|
| Audio INPUT (PJ-307 3.5mm) | **15.05mm** | NORTH wall. Barrel center **7.3mm above PCB back face**. Cutout #3, Ø6.9. |
| Audio OUTPUT (PJ-307 3.5mm) | **59.95mm** | NORTH wall. Barrel center **7.3mm above PCB back face**. Cutout #4, Ø6.9 (in/out symmetric). |

> **USB-C is on the SOUTH wall** (Y=140), via the wired breakout — see the note above and Enclosure Cutout #10. PJ-307 jack bodies extend ~14.3mm into the PCB from the north edge; keep the Daisy/OLED clear of that band.

### Row 1 — Mic + Power Switch (≈ Y 0–9mm, very top)

Both features sit at the **very top** of the board (not in a y≈22 band as in the legacy layout). Mic is up near the **top-left**; the power switch is **centered** at the top.

| Element | Center (X, Y) | Notes |
|---------|---------------|-------|
| MAX4466 mic | **(11, 8.2)** | Top side. Ø10 feature (Cutout #8). Capsule faces UP; top face here is **15.3mm above PCB back**, like the OLED. Verify module body fits given the top-left corner / mounting hole. |
| Power switch (EG1218) | **(38, 5.125)** | Top side, **centered at the very top**. 7×4.65mm slot (Cutout #9). Sits in the front-face **dip**. (Legacy layout had it at x=63 — moved to center-top.) Slider protrudes through the housing slot. |

### Row 2 — OLED (≈ Y 19.75–60.75mm)

| Element | Center (X, Y) | Notes |
|---------|---------------|-------|
| SSD1309 2.42" OLED | **(37.5, 40.25)** | Top side. Visible window **62.5(X)×41(Y)**, spans **Y 19.75→60.75**, X 6.25→68.75 (Cutout #1). 7-pin through-hole header on left short edge, direct solder, no standoffs. Verify footprint origin vs window center in KiCad. |

OLED window spans Y 19.75–60.75; clears the north jack bodies (which reach ~Y14.3 into the board) by ~5.4mm. Verify final clearance in placement.

### Row 3 — Encoders / VL53L1X ToF (≈ Y 60–73mm)

| Element | Center (X, Y) | Notes |
|---------|---------------|-------|
| Encoder L (CYT1100) | **(18.73, 62.17)** | Shaft center, Ø6 (Cutout #5). PCB-mount, direct solder. |
| VL53L1X / Lidar ToF (TOF400C) | window **(37.05, 69.43)** | Window center; stadium **10(X)×6.5(Y)**, OPEN hole (Cutout #7). Flat mount, sensor window faces up. |
| Encoder R (CYT1100) | **(57.9, 62.17)** | Shaft center, Ø6 (Cutout #6). **NOT a mirror of L** — X is not symmetric about board center. |

> **Encoder shaft origin note:** encoder feature centers are the **shaft** centers (18.73 / 57.9). The KiCad footprint origin (pin centroid) differs — **verify the shaft-to-origin offset in KiCad** and place so each shaft lands on its locked coordinate.

### Row 4 — NeoTrellis (≈ Y 77.5–132.5mm)

| Element | Notes |
|---------|-------|
| NeoTrellis 4×4 | 16 pads, centers at **X{15,30,45,60} × Y{82.5,97.5,112.5,127.5}**, **15mm pitch**, overall **55×55mm**, grid **centered in X (10mm margins each side)**, bottom edge **7.5mm off the PCB bottom** (grid reaches Y≈132.5). (Cutout #2. Legacy "60×60 at y90–150" is superseded.) |

**FSR placement:** SF45-65 (45×65mm flexible film, 45×45mm active) sits sandwiched between the NeoTrellis PCB bottom and the main PCB top surface. NeoTrellis button presses compress it. FSR wires route from under the NeoTrellis edge to J_FSR header on PCB **bottom side, x≈71mm, y≈95mm** (right edge, inside NeoTrellis x-zone — TBD, confirm against final layout).

### PCB Bottom Side, y from top edge

> ℹ️ **Daisy Seed placement is free** — the USB-C reaches it via the wired breakout + micro-USB male (see USB note), so the Daisy is not tied to any wall port. Position it for best fit in the rear cavity. Bottom-side module coordinates below are approximate placeholders on the 75×140 board — caliper-confirm.

| Component | x | y | Notes |
|-----------|---|---|-------|
| Daisy Seed | place for fit | place for fit | Portrait, 60.96×25.4mm PCB on machine-pin female headers, 12mm tall in cavity. USB reaches it via the wired breakout + micro-USB male, so position is **free** (not tied to any wall port) — place for best fit in the rear cavity and confirm socket-row X positions. |
| MPU9250 IMU | x≈37.5mm | y≈65mm | PCB center, inside Daisy cavity. Critical placement for accurate gyro (far from speaker magnets and XL3608 inductor). Confirm against final Daisy position. |
| J_TRELLIS header | x≈4mm | y≈95mm | Bottom side, near left edge inside NeoTrellis y-zone; NeoTrellis wires route around PCB edge to here. TBD. |
| J_FSR header | x≈71mm | y≈95mm | Bottom side, near right edge inside NeoTrellis y-zone; FSR wires route from under NeoTrellis. TBD. |
| TP4056 | x≈15mm | y≈100mm | Bottom-left area. TBD. |
| XL3608-5V | x≈60mm | y≈100mm | Bottom-right area, near TP4056. TBD. |
| PAM8403 | x≈15mm | y≈125mm | Bottom-left, speaker pads adjacent. TBD. |
| WWZMDiB SD module | x≈60mm | y≈125mm | Bottom-right, edge-accessible for card insertion. TBD. |

---

## Enclosure (Housing — Authoritative Spec)

> **This is the authoritative housing design** from the completed housing-design session. It supersedes any older housing text elsewhere in this doc (the legacy PCB-Layout / KiCad sections still reference the old 84×150 design grid and ~92×158×50 enclosure — those are kept for the PCB workflow but are **not** the housing reference). The housing design is **mid-flight**; every open value is marked **TBD**.

### Overall Concept

Custom organic **"pebble / lozenge"** enclosure, 3D-printed **PETG**, super-rounded edges, clean product-grade finish, **no text/logo**. Modeled in Blender via lofted cross-section profiles. Widest at the middle, tapering symmetrically to smaller flat-ish caps at the top and bottom ends.

### Coordinate Convention (used throughout all housing sections)

- **X** = width (left ↔ right). **Y** = length (top ↕ bottom, portrait). **Z** = depth (front ↔ back).
- **Y=0** = **TOP / NORTH** end = the audio-jacks wall. **Y=140** = **BOTTOM / SOUTH** end = the USB-C wall.
- **Z front** = **TOP FACE** = the user-facing control face (OLED / pads / encoders). **Z back** = rear.
- **"North face"** = top end wall (Y=0). **"South face"** = bottom end wall (Y=140).
- All hole positions below are given relative to the **true 75(X)×140(Y) PCB**, origin at the **PCB top-left corner** (X from left 0→75, Y from top 0→140), measured to each hole **center**.
- **PCB → shell frame conversion:** the PCB sits **centered** in the shell — add **+2.5mm X** and **+0.5mm Y** to any PCB coordinate to get its shell-frame coordinate.

### Outer Form & Dimensions

- **Outer size:** ~**100mm (X, max width at the ¼ & ¾ bulges) × 141mm (Y, length) × 46mm (Z, max depth)**. (Replaces the old "~92×158×50mm".)
- **Form — curved "hourglass / peanut" silhouette (X width along Y):** smooth-curving through **77mm at the top (Y0) → 100mm at ¼ (Y≈35) → 77mm at the middle (Y≈70.5) → 100mm at ¾ (Y≈106) → 77mm at the bottom (Y141)**. Two bulges at the **quarter-points** (equal spacing), with pinches at top / middle / bottom; **all transitions are smooth curves** — this is THE key body curve. *(Supersedes the earlier "widest in the middle ~80mm" description, which was the opposite shape.)*
- **Cross-section:** flat front face + a slightly narrower flat back face, side edges rounding **as close to a semicircle as possible while still clearing the components** (most dramatic curves). Max depth **~46mm**, curving **more dramatically toward the bottom** and easing thinner at the top/bottom ends.
- The **flat front** is kept **≥~62mm wide** so it can host the controls (easy at the 100mm bulges; at the 77mm pinches the PCB is 75mm wide with ~1mm side margin, and the flat front still clears the centered 55mm pad / 62.5mm OLED windows).
- **Wall:** 2.5mm PETG shell (after the solid form is built), with local thickening (~3mm) at the parting seam and at the insert bosses.

### PCB Reference & Cavity

- True PCB outline = **75mm (X) × 140mm (Y)** (from the Edge_Cuts gerber). Replaces the old "84×150mm" in the housing context.
- The PCB is **centered in X and Y**. In **Z it sits toward the FRONT**: the front (top) face stands **~11–16mm above the PCB back face** depending on region (see **Front-Face Height Map**), leaving a **~26mm cavity behind the PCB** within the **46mm** total depth. Behind the PCB: a **~22mm component gap** (Daisy etc.) then the **battery — 110mm (Y) × 61mm (X) × 12mm (Z)** — plus thin foam pads. Treat all these Z figures as **rough ratios to proportion into the 46mm real total depth**; the build **hugs the PCB closely** (nothing extends far beyond it) and **curves more at the bottom**. ("Top/bottom face of PCB" accounts for the **1.8mm** PCB thickness.)

### Front-Face Height Map (Stage-1 form)

The front/top face is **not flat** — it is a **smooth surface that rises and falls to meet each hole's height**, interpolating continuously between them (the switch "dip" is just one instance). Each height is **where the hole / outer top face sits, measured from the PCB back face** — components stick out further; we model to the hole / top-face. Accounts for the 1.8mm PCB thickness:

| Region | Top-face height above PCB back face |
|---|---|
| Power switch (top center) | **11.15mm** (≈11, local dip) |
| NeoTrellis pads (lower) | **13mm** |
| OLED (upper-mid) | **15.3mm** |
| Mic (top-left) | **15.3mm** (same as OLED) |
| Lidar + encoders (mid row) | **16mm** (high point) |

So the surface **humps to a peak at the lidar/encoder row (16mm)**, eases down to OLED/mic (15.3), NeoTrellis (13), and **dips at the switch (11.15)**; the switch dip ramps back over ~6.5mm each side. The **TOP↔NORTH edge fillet is capped ~2mm** — ⚠️ **always remember the mic hole is jammed into the top-left corner: ~3mm from the PCB top edge and ~1mm from the mic sub-PCB edge**, so no edge roll / wall thickening / housing encroachment is possible there. **(These supersede the earlier 18.25mm figure entirely; total body depth is 46mm.)**

### Shell Split

Single solid model, then cut into a **front half + back half** ("almost halves") at the **mid-girth parting line**. Each half is exported separately.

---

## Fastening — M2 Screws + Heat-Set Inserts

**Fastening: 4× M2-0.4 × 30mm socket-head cap screws (304 stainless) into M2 brass heat-set inserts.** (Replaces the old "M2×8mm button-head + 4 corner bosses" scheme.)

- The **4× long screws enter from the BACK shell**, run up the empty **CORNER channels** (clear of the battery), pass through the main PCB's M2 mounting holes, and thread into **blind M2 brass heat-set inserts** held in **full-pillar bosses in the FRONT shell**. The insert bosses do **not** break the visible front surface.
- **Screw:** M2-0.4 × 30mm socket head cap, 304 stainless — [a.co/d/00Es7j9G](https://a.co/d/00Es7j9G) (30 pc pack). Verified fit: ~0.5mm back-wall remainder + ~24mm corner channel + 1.8mm PCB + ~3.5–4mm insert engagement; the tip buries ~10mm short of the front face.
- **Inserts:** matching **M2 brass heat-set inserts** (soldering-iron heat-set) are required — see boss subsection below.
- **Back screw holes are COUNTERBORED** so the socket-cap heads sit **flush** (no bumps on the back).
- **4 screw / insert-cylinder positions = the PCB's 4 corner mounting holes (LOCKED):**

| Hole | PCB (X,Y) | Shell (X,Y) = +2.5 / +0.5 |
|------|-----------|---------------------------|
| Top-left | (4, 4) | (6.5, 4.5) |
| Top-right | (71, 4) | (73.5, 4.5) |
| Bottom-left | (4, 136) | (6.5, 136.5) |
| Bottom-right | (71, 136) | (73.5, 136.5) |

  *(Assumed M2 mounting-hole positions on the 75×140 PCB — verify against the physical board, but locked for the housing. Back-face screw holes + counterbores sit at these same X,Y; the front-shell solid insert cylinders descend at these same X,Y.)*
- **PCB Z (LOCKED):** front (top) face is **11–16mm above the PCB back face** (per Height Map); the PCB **back face sits ~30mm in from the back/bottom outer face** (≈27mm rear cavity), i.e. **toward the front** within the 46mm depth — tight, proportioned from the component sizes.

### Insert Bosses (Front Shell) — Critical Subsection

- The **front shell carries 4× SOLID PETG boss CYLINDERS that descend from the top/front face** (extra printed filament / material) that the **M2 brass heat-set inserts are MELTED into** (soldering-iron heat-set).
- Each boss is a **straight pillar along the Z (screw) axis**, positioned so its **center is PRECISELY above / in-line with a main-PCB mounting hole** — so the back screw passes through the PCB hole straight up into the insert.
- **CRITICAL — bosses are driven by PCB hole coordinates, NOT by the curved skin:** boss positions are derived from the **PCB mounting-hole X/Y coordinates only** (then +2.5X / +0.5Y to shell frame) — **NOT** by the curved outer skin. The body's curvature must **NOT** shift or skew the bosses; spacing and alignment between the 4 bosses stays **exact/perfect** regardless of how the outer surface curves. **Extra filament fills the gap** between the curved inner shell wall and each boss, and the boss's **PCB-facing end is flat and square to the screw axis** even though the skin is curved.
- **No conflict with the NeoTrellis:** the bosses must **NOT** collide with the NeoTrellis PCB (~60×60mm, centered, sitting between the front shell and the main PCB in the pad zone). Route the bosses to the **4 corners** where the NeoTrellis does not reach.
  - **Bottom cylinders vs NeoTrellis — TIGHT but doable (LOCKED):** the NeoTrellis PCB (~60×60mm, centered on the pad grid → ~X7.5–67.5, Y75–135) sits just inboard of the bottom holes at **X4 / X71**. The bottom cylinders clear it by only **~1mm in X**, so make the **two bottom cylinders Ø5mm** (not 6) so they tuck just outboard of the NeoTrellis corners. The **top two cylinders (Y4) have ample room**. Verify the ~1mm against the physical NeoTrellis.
- **Boss sizing:** outer **Ø ~5–6mm**, **SOLID — NO pre-drilled bore or cavity.** Per the build method, the M2 brass heat-set insert is **melted straight into the solid cylinder** with a soldering iron (the melt forms its own pocket); the flat boss end-face + iron-driven insert self-centers, so no pilot is needed. **Gusset / connect** each boss to the front inner wall so it is rigid and not floating on the curved surface. Keep each cylinder **SHORT — no more than ~6mm long** (just enough to hold the insert at its bottom / PCB-facing end), positioned so the insert sits **~2–5mm above the PCB front face** where the M2×30 screw tip lands (~3–4mm engagement). If a corner's front-face is tall enough that a 6mm cylinder leaves the insert too high to bite, step that screw to **M2×35** rather than lengthen the cylinder.

---

## Speakers / "Ears" — Side-Firing  *(Stage 2 of the build)*

Replaces the old "rear-firing 20×40mm speakers behind battery, back grilles" scheme. **2× mirrored, on the upper LEFT and RIGHT sides.**

**Speakers** (each):
- **20×40mm face, 5.5mm thick.** Orientation: **40mm runs up-and-down (Y), 20mm front-to-back (Z), 5.5mm thickness across (X).**
- Fires **straight out the side (±X), 90° "like a box."** The housing is designed for **90° (perpendicular) firing**; any slight internal cant the builder adds is handled at mounting and is out of scope for the shell.
- Mounted with its **top-outer corner exactly on the PCB's top-side corner** ("cube point") → it spans **Y0→40 (center ~Y20)**, outer face flush to the PCB side edge, in the rear-ish side region (behind the PCB front plane, beside the battery) — so the **speaker is NOT housed inside the ear**.
- Behind a **side grille** (below).

**Ears** (the redirect feature — **external only, no speaker inside**):
- A smooth **flowing curve** swelling out of the upper side, **~6–7mm proud** of the 80mm body width `[tunable]`. **A curve, not a symmetric scoop** — it sweeps, it doesn't bowl.
- Positioned **a fair bit BELOW the speaker's center** (curve mass ~**Y28–48** `[tunable]`), sweeping **upward** toward the speaker so it nudges the side-firing wavefront **a bit toward the front/user**. (Honest note: a passive external curve gives a *modest* forward bias, not a true 90° redirect.)
- A **fillet-blended swelling of the shell wall** (≥2.5mm everywhere, no thin lips, self-supporting print angles) — **never an appendage that could snap off.**
- Lands on the **FRONT shell**; the **mid-girth split JOGS down below each ear** so the ear is one seamless piece.

**Side grille** (Stage-3 cutout, pattern locked here):
- Opening zone **~20mm (Z) × 40mm (Y)** on the side wall, over the speaker face.
- **DIAMOND lattice** pattern — **maximize open area** for airflow/output **while remaining fully structurally sound** (PETG ribs ~1.2–1.5mm between diamonds).

**Remaining TBD:** exact speaker/ear Y center (vibe-locked to the ranges above — caliper/refine), and the diamond-grille rib/hole dimensions (finalize at Stage 3).

---

## Layer Stack (front → back, portrait orientation)

| Layer | Component | Thickness / Note |
|-------|-----------|------------------|
| Front shell (TOP face) | 3D-printed PETG, 2.5mm wall + boss pillars | ~2.5mm wall |
| NeoTrellis | ~60×60mm silicone pad + PCB (in the pad zone) | between front shell & main PCB |
| Main PCB | 75×140mm FR4 | 1.8mm |
| — (front face reference) | Front (top) face is **11–16mm above the PCB back face**, per region (OLED/mic 15.3, lidar/enc 16, NeoTrellis 13, switch 11.15) — see Front-Face Height Map | — |
| Rear cavity | Battery + Daisy stack | **~24mm** behind the PCB back face |
| Cotton/foam pad | Insulation + vibration isolation | ~2mm |
| Battery | JLJLUP 60×112×12mm LiPo | 12mm |
| Cotton/foam pad | Insulation + vibration isolation | ~2mm |
| Back shell (rear face) | 3D-printed PETG, counterbored screw holes | ~2.5mm wall |

- **Speakers are NOT in this back-cavity stack** — they sit **near the top, on the far left/right sides, firing sideways into the "ears"** (see the Speakers / "Ears" section).
- The PCB sits **toward the front**; the **~26mm rear cavity** holds the battery + Daisy stack (within the 46mm total depth).

---

## Enclosure Cutouts (Locked)

Each cutout is modeled as a **separate "cutter" object** in a CUTTERS collection and **boolean-subtracted last**. Positions are **caliper-derived**. "Center (PCB)" is relative to the 75×140 PCB (origin top-left); "Center (shell)" adds **+2.5mm X / +0.5mm Y**.

| # | Cutout | Face | Shape & size | Center (PCB) | Center (shell +2.5X/+0.5Y) | Notes |
|---|--------|------|--------------|--------------|----------------------------|-------|
| 1 | OLED window | TOP face | Rectangle 62.5(X)×41(Y) | (37.5, 40.25) | (40.0, 40.75) | Spans PCB X 6.25→68.75, Y 19.75→60.75 |
| 2 | NeoTrellis grid | TOP face | 16× **square** holes 10×10mm, sharp corners, 5mm gaps, 15mm pitch, overall 55×55mm | centers X{15,30,45,60} × Y{82.5,97.5,112.5,127.5} | X{17.5,32.5,47.5,62.5} × Y{83,98,113,128} | Grid centered in X (10mm margins); bottom edge 7.5mm off PCB bottom |
| 3 | Audio INPUT jack | **NORTH** wall | Ø6.9 round | X 15.05 | X 17.55 | 7.3mm above PCB back face; left side |
| 4 | Audio OUTPUT jack | **NORTH** wall | Ø6.9 round | X 59.95 | X 62.45 | 7.3mm above PCB back face; right side (in/out symmetric) |
| 5 | Encoder L shaft | TOP face | Ø6 round | (18.73, 62.17) | (21.23, 62.67) | |
| 6 | Encoder R shaft | TOP face | Ø6 round | (57.9, 62.17) | (60.4, 62.67) | NOT a mirror of L |
| 7 | Lidar / ToF window | TOP face | Stadium/obround 10(X)×6.5(Y), rounded ends, **OPEN hole** | (37.05, 69.43) | (39.55, 69.93) | ToF can't see through PETG — must be open |
| 8 | Mic | TOP face | Ø10 round | (11, 8.2) | (13.5, 8.7) | Top face **15.3mm above PCB back**, same as the OLED. **Jammed in the top-left corner: ~3mm from the PCB top edge, ~1mm from the mic sub-PCB edge** — no shell encroachment / edge roll here. (X=11 locked; height is a separate axis.) |
| 9 | Power switch | TOP face | Rectangle 7(X)×4.65(Y) | (38, 5.125) | (40.5, 5.625) | Sits in the front-face **dip** |
| 10 | USB-C | **SOUTH** wall | Stadium 10(X)×3.3(vertical), rounded ends | X 36 | X 38.5 | **BOTTOM/BACK side of the PCB** (wires to the Daisy under the board) → sits **LOWER than the top-side jacks, NOT coplanar.** Opening **bottom edge 11.5mm below the PCB TOP face → center ≈9.85 below top face (= 8.05 below the PCB back face, using the 1.8mm thickness)**, ~mid-depth (~47% up), clear of the front corner. Opening 1mm outboard of the PCB's USB-C connector. Small **FLAT LANDING PAD** on the curved south cap so the plug seats square. |

---

## Blender Modeling Methodology (Build Process)

- **Three stages, in order:** **STAGE 1 = BODY** (the complete solid pebble shape, no ears, no holes) → **STAGE 2 = EARS** (add the side speaker-scoop protrusions) → **STAGE 3 = HOLES** (all cutouts, subtracted last).
- Body via **LOFTED cross-section profiles** at several Y stations, skinned to **one watertight solid**.
- **Wall 2.5mm PETG** (shell after the solid form), with **local thickening (~3mm)** at the parting seam and bosses.
- **Backup method = duplicate-and-hide.** Before *every* change, **copy the current work object and hide the copy** — the hidden duplicate is the backup. Name them in sequence (e.g. `body_v01`, `body_v02`, `ears_v01`, …) so any step is recoverable by un-hiding. Keep hidden backups out of the way (a dedicated hidden collection is fine).
- **Each hole = its own named "cutter" object** (its own duplicate-and-hide backup), in a **CUTTERS collection**.
- **CONFIRM each hole location (caliper-verified) BEFORE any boolean.** Subtract cutters **LAST**. **Units = mm.**

---

## Housing — TBD / In Progress

The housing design is **mid-flight**. Open items:

- **Ears (Stage 2 — defined):** massing locked (see Speakers / "Ears"). Remaining = exact speaker/ear **Y center** refinement + the **diamond-grille rib/hole dimensions** (finalize at Stage 3).
- **PCB Z placement:** front-face heights are locked (11–16mm, see Height Map) and total depth = 46mm; the PCB's exact Z is a rough ratio (toward the front) — refine the rear cavity once the Stage-1 solid exists.
- **Screws / cylinders:** positions LOCKED at the 4 PCB corner holes; bottom cylinders Ø5 to clear the NeoTrellis (~1mm) — verify the ~1mm on the physical board.

---

## KiCad PCB Workflow

### Step 1 — Project Setup

1. KiCad 7/8 → **File → New Project** → `Pocket-OpGorator`
2. **PCB Editor → File → Board Setup:**
   - Board thickness: 1.6mm | Copper layers: 2 (F.Cu, B.Cu)
   - Min clearance: 0.2mm | Min trace: 0.2mm | Min via: 0.8mm dia / 0.4mm drill
   - Placement grid: 1mm | Routing grid: 0.1mm

### Step 2 — Schematic

Net labels throughout. Power symbols: `GND`, `+5V` (boost rail), `+3.3V` (Daisy 3V3 out), `VBAT` (LiPo+), `VBAT_SW` (post-switch). Add `PWR_FLAG` on `GND` and `+5V`.

#### Daisy Seed
Two `Connector:Conn_01x20_Female` symbols (`J_DAISY_L`, `J_DAISY_R`). Label per Daisy Seed pinout. Machine pin female header footprint, 2× 20-pin rows, 2.54mm pitch.

#### Module headers

| Ref | Symbol | Pins | Key nets |
|-----|--------|------|----------|
| J_MAX4466 | Conn_01x03_Female | 3 | OUT→MIC_OUT, GND, VCC→**+3.3V** (Adafruit explicitly recommends the quietest available supply rail — Daisy 3V3 — for lowest noise; module accepts 2.4–5.5V) |
| J_MPU9250 | Conn_01x10_Female | 10 | VCC→+3.3V, GND, SCL→I2C4_SCL, SDA→I2C4_SDA; pins 5–10 NC |
| J_SDCARD | Conn_01x06_Female | 6 | GND, VCC→+5V, MISO→SPI_MISO, MOSI→SPI_MOSI, SCK→SPI_SCK, CS→SD_CS |
| J_AMP | Conn_01x05_Female (5-pin variant; 6-pin Conn_01x06 if you have variant A) | 5 or 6 | VIN→+5V, GND, L_IN→AUDIO_OUT_L, R_IN→AUDIO_OUT_R, SHDN→SHDN_NET |
| J_TP4056 | Conn_01x04_Female | 4 | OUT+→VBAT_RAW, B+→VBAT, B−→GND, OUT−→GND |
| J_XL3608_IN | Conn_01x02_Female | 2 | VIN+→VBAT_SW, GND |
| J_XL3608_OUT | Conn_01x02_Female | 2 | VOUT+→+5V, GND |
| J_OLED | Conn_01x07_Female | 7 | GND, VCC→+3.3V, SCK→SPI_SCK, DIN→SPI_MOSI, RES→OLED_RST, DC→OLED_DC, CS→OLED_CS |
| J_TRELLIS | Conn_01x04_Female | 4 | +5V, GND, SCL→I2C1_SCL, SDA→I2C1_SDA |
| J_TOP_SENSOR | Conn_01x06_Female | 6 | VCC→+3.3V, GND, SCL→I2C4_SCL, SDA→I2C4_SDA, GPIO1→NC, XSHUT→tie-high net |
| J_FSR | Conn_01x03_Female | 3 | +3.3V, FSR_ANA, GND |
| J_AUX | Conn_01x04_Female | 4 | +3.3V, GND, spare GPIO×2 |

#### Discrete components

- **SW1 (EG1218 SPDT slide switch):** Pin 2 (COM) → SW_GATE net. Pin 1 → GND. Pin 3 → VBAT_RAW. SW1 drives the P-MOSFET gate only — it does not carry load current.
- **Q1 (AO3401 P-MOSFET, SOT-23):** Source → VBAT_RAW, Drain → VBAT_SW (→ XL3608 VIN+), Gate → SW_GATE. KiCad symbol `Transistor_FET:AO3401`; footprint `Package_TO_SOT_SMD:SOT-23`.
- **R_GATE (10kΩ):** Pull-up from SW_GATE → VBAT_RAW. Ensures gate is defined when SW1 is mid-throw.
- **ENC1, ENC2** (`Device:RotaryEncoder_Switch`): ENC1 A/B/SW → D3/D4/D5; ENC2 A/B/SW → D6/D16/D17. 100nF from each A, B, SW to GND.
- **J_IN, J_OUT** (`Connector:AudioJack3_SwitchT`): SLEEVE→GND. J_IN pin 4 + pin 5 → MIC_OUT net (MAX4466 OUT). J_OUT pin 4 → HP_DETECT net.
- **R_HPDET** (10kΩ): pullup HP_DETECT → +3.3V.
- **C_HPDET** (1µF ceramic or electrolytic): HP_DETECT → GND. RC cutoff with R_HPDET = 16Hz, so all audible audio AC is suppressed; only the DC TIP-bias / floating-pin transition reaches D19.
- **R_SHDN** (10kΩ): SHDN_NET → GND (default-low pulldown; Daisy D18 drives high to enable amp).
- **R_XSHUT** (10kΩ): VL53L1X XSHUT pullup to 3.3V — tied permanently high; no GPIO needed.
- **R_FSR** (10kΩ): FSR divider (FSR_ANA → R → GND + 100nF).
- **R_VBAT1, R_VBAT2** (100kΩ ea): VBAT divider to D20 (A5), 100nF to GND.
- **C_BYPASS**: 100nF + 10uF per module VCC.
- **C_TRELLIS** (100uF): NeoTrellis +5V bulk.
- **C_BOOST** (10uF): XL3608 VOUT+ bulk.

#### ERC
Acceptable unconnected: MPU9250 header pins 5–10, VL53L1X GPIO1 (NC), SW1 pin 3 (intentional NC). VL53L1X XSHUT has R_XSHUT pullup and is NOT unconnected. PAM8403 SHDN has both Daisy drive AND R_SHDN pulldown — not unconnected. Fix all other errors.

---

### Step 3 — Footprint Assignment

| Reference | KiCad Footprint |
|-----------|----------------|
| J_DAISY_L, J_DAISY_R | `Connector_PinSocket_2.54mm:PinSocket_1x20_P2.54mm_Vertical` |
| J_MAX4466 | `Connector_PinSocket_2.54mm:PinSocket_1x03_P2.54mm_Vertical` |
| J_MPU9250 | `Connector_PinSocket_2.54mm:PinSocket_1x10_P2.54mm_Vertical` |
| J_SDCARD | `Connector_PinSocket_2.54mm:PinSocket_1x06_P2.54mm_Vertical` |
| J_AMP | `Connector_PinSocket_2.54mm:PinSocket_1x05_P2.54mm_Vertical` (5-pin variant; swap to 06 or 08 for variants A/C) |
| J_TP4056 | `Connector_PinSocket_2.54mm:PinSocket_1x04_P2.54mm_Vertical` |
| J_XL3608_IN, J_XL3608_OUT | `Connector_PinSocket_2.54mm:PinSocket_1x02_P2.54mm_Vertical` |
| J_OLED | `Connector_PinSocket_2.54mm:PinSocket_1x07_P2.54mm_Vertical` — on the left short edge of the OLED window (window center (37.5, 40.25), spans Y 19.75–60.75); 7 pads at 2.54mm pitch in +y direction. Verify header X/Y against the window in KiCad. |
| J_TRELLIS | `Connector_PinSocket_2.54mm:PinSocket_1x04_P2.54mm_Vertical` — bottom side, x≈4mm, y≈95mm (TBD) |
| J_TOP_SENSOR | `Connector_PinSocket_2.54mm:PinSocket_1x06_P2.54mm_Vertical` |
| J_FSR | `Connector_PinSocket_2.54mm:PinSocket_1x03_P2.54mm_Vertical` — bottom side, x≈71mm, y≈95mm (TBD; was x=80 on legacy 84mm board) |
| SW1 (EG1218) | `Button_Switch_THT:SW_Slide_1P2T_EG1218` — 3-pin 1.5mm pitch (verified against E-Switch datasheet; this is NOT the 2.5mm pitch of the CK OS102 series) |
| Q1 (AO3401 P-MOSFET) | `Package_TO_SOT_SMD:SOT-23` |
| J_AUX | `Connector_PinSocket_2.54mm:PinSocket_1x04_P2.54mm_Vertical` |
| ENC1, ENC2 | `Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm` — CYT1100 footprint-compatible (2.54mm encoder-pin pitch, 5mm switch-pin pitch, 5mm row spacing, 20mm shaft) |
| J_IN, J_OUT | `Connector_Audio:Jack_3.5mm_PJ307_Horizontal` |
| R (THT) | `Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal` |
| C_100nF (THT) | `Capacitor_THT:C_Disc_D3.0mm_W2.0mm_P2.50mm` |
| C_10uF, C_100uF (electrolytic) | `Capacitor_THT:CP_Radial_D5.0mm_P2.50mm` |
| TP_GND, TP_5V, TP_3V3 | `TestPoint:TestPoint_Pad_2.0x2.0mm` |

---

### Step 4 — Board Outline

**Edge.Cuts:** **75mm (X) × 140mm (Y)** rectangle (true outline from the Edge_Cuts gerber). Origin = TOP-LEFT corner. No notches. (Replaces the legacy 84×150mm.)

> ℹ️ **USB-C on SOUTH wall:** the external USB-C opening is on the **Y=140 (south) end** (Enclosure Cutout #10), as a wired breakout board — not the north/top edge. No on-board USB connector and no Edge_Cuts relief are required (the breakout mounts to the housing wall, not the PCB), and the Daisy placement is independent of it (see the USB note in PCB Layout).

**M2 mounting holes** — add 4× NPTH **2.2mm** drill holes (5mm dia copper-free keepout each) at the four corners, approximately:

| Hole | (X, Y) | Notes |
|------|--------|-------|
| Top-left | **(4, 4)** | LOCKED (verify on board) |
| Top-right | **(71, 4)** | LOCKED (verify on board) |
| Bottom-left | **(4, 136)** | LOCKED — bottom cylinder Ø5 to clear NeoTrellis (~1mm) |
| Bottom-right | **(71, 136)** | LOCKED — bottom cylinder Ø5 to clear NeoTrellis (~1mm) |

> **CRITICAL — holes = front-shell insert bosses:** these 4 holes **MUST coincide with the front-shell heat-set INSERT BOSSES**. The 4× M2×30mm screws pass from the **back** shell, through these PCB holes, and thread into the M2 brass heat-set inserts in the front-shell boss pillars. Boss positions are derived from these PCB hole X/Y (then +2.5X/+0.5Y to shell frame) — so the PCB holes are the master reference. See the **Fastening** + **Insert Bosses** housing sections.
>
> ⚠️ **Bottom-hole vs NeoTrellis clearance:** the two **bottom** holes (~Y136) sit just below the NeoTrellis grid (which reaches **Y≈132.5**) — only **a few mm** of clearance. **Caliper-confirm (TBD)** before locking the bottom hole / boss positions.
>
> All four positions are **TBD** until caliper-confirmed against the real board.

---

### Step 5 — Component Placement

Coordinates per the PCB Layout section above and **caliper-locked to the "Enclosure Cutouts (Locked)" table** (source of truth). All values are **feature centers** on the 75×140 board, origin top-left.

> ⚠️ **Footprint origin vs feature center:** the coordinates below are the **visible-feature centers** (shaft / window / capsule / slider). Each KiCad **footprint origin** may differ — **verify the offset in KiCad** and place so the *feature* lands on its locked coordinate.

- **J_IN (Audio INPUT, NORTH wall):** center X=**15.05mm**, barrel 7.3mm above PCB back face (Cutout #3).
- **J_OUT (Audio OUTPUT, NORTH wall):** center X=**59.95mm**, barrel 7.3mm above PCB back face (Cutout #4).
- **MAX4466 (top side):** **(11, 8.2)**, Ø10 feature. Mic capsule faces UP; top face here is 15.3mm above PCB back. (X=11 locked; height is a separate axis, Cutout #8.)
- **SW1 EG1218 (top side):** **(38, 5.125)**, 7×4.65mm slot, centered at the very top in the front-face dip (Cutout #9). Slider protrudes through front-shell slot.
- **OLED SSD1309 (top side):** window center **(37.5, 40.25)**, visible window 62.5×41, spans Y 19.75→60.75, X 6.25→68.75 (Cutout #1). J_OLED 7-pin header on left short edge, running in +y direction. Direct-solder, no standoffs.
- **ENC L (top side):** shaft center **(18.73, 62.17)**, Ø6 (Cutout #5). **ENC R:** shaft center **(57.9, 62.17)**, Ø6 (Cutout #6) — **not a mirror of L**.
- **VL53L1X / ToF (top side):** window center **(37.05, 69.43)**, stadium 10×6.5 OPEN hole (Cutout #7). Sensor window faces UP.
- **NeoTrellis zone (top side):** 16 pads at X{15,30,45,60} × Y{82.5,97.5,112.5,127.5}, 15mm pitch, 55×55 overall, centered in X, bottom edge 7.5mm off PCB bottom (Cutout #2). Keep clear of tall THT below.
- **J_TRELLIS (bottom side):** x≈4mm, y≈95mm (TBD).
- **J_FSR (bottom side):** x≈71mm, y≈95mm (TBD — was x=80 on the legacy 84mm board).
- **Daisy Seed (bottom side):** placement **free** — USB reaches it via the wired breakout + micro-USB male, so it's not tied to any wall port; position for best fit in the rear cavity. 60.96×25.4mm PCB on machine-pin headers, 12mm tall.
- **MPU9250 (bottom side):** x≈37.5mm, y≈65mm — PCB center, inside Daisy cavity (confirm against final Daisy position).
- **Power modules (bottom side):** TP4056 (x≈15, y≈100), XL3608 (x≈60, y≈100), PAM8403 (x≈15, y≈125), SD module (x≈60, y≈125) — all TBD on the 75×140 board. Bypass caps within 2–3mm of each VCC pad.
- **M2 mounting holes:** NPTH 2.2mm at **(4,4), (71,4), (4,136), (71,136)** (LOCKED — verify on board); 5mm copper-free keepout each. **These MUST coincide with the front-shell insert cylinders** (M2×30 screws pass from back through these holes into the cylinders). Bottom cylinders Ø5mm to clear the NeoTrellis (~1mm in X).

---

### Step 6 — Routing

#### Net Classes

| Class | Trace | Nets |
|-------|-------|------|
| Default | 0.25mm | All signal nets |
| Power | 1.0mm | +5V, GND, VBAT, VBAT_RAW, VBAT_SW |
| Power_3V3 | 0.5mm | +3.3V |

#### Order
1. Power: +5V from XL3608 to all modules; GND star from Daisy GND. Battery path: TP4056 OUT+ → SW1 → XL3608 VIN+. Wide traces.
2. I2C1 (SCL→D11, SDA→D12 → J_TRELLIS). I2C4 (SCL→D13, SDA→D14 → J_TOP_SENSOR + J_MPU9250). Short, away from XL3608 inductor.
3. SPI (SCK/MOSI/MISO → J_OLED + J_SDCARD, separate CS lines).
4. Audio: AUDIO_OUT_L/R → J_AMP + J_OUT TIP/RING. MIC_OUT → J_IN pins 4/5.
5. Mute: D18 → R_SHDN node → SHDN; J_OUT pin 4 → R_HPDET + C_HPDET node → D19.
6. Encoder A/B/SW; FSR analog; VBAT divider; AUX GPIO.

#### Ground Pour
Zones on F.Cu and B.Cu (GND, clearance 0.3mm, min width 0.3mm). Fill with **B**. Stitching vias (0.8mm drill, 1.6mm pad) every ~10mm.

---

### Step 7 — Silkscreen & Test Points

- Label every header on F.Silkscreen. Pin-1 dot/triangle on all headers.
- Label SW1 with ON/OFF on the appropriate sides of the slider.
- Test point pads on GND, +5V, +3.3V, VBAT. Label each.
- Board name: `POCKET OPGORATOR v0.3`

---

### Step 8 — DRC & Gerber Export

1. **Inspect → DRC → Run DRC.** Fix all errors. Acceptable unconnected: MPU9250 pins 5–10, VL53L1X GPIO1, SW1 pin 3.
2. **File → Fabrication Outputs → Gerbers:** F.Cu, B.Cu, F.Silkscreen, B.Silkscreen, F.Mask, B.Mask, Edge.Cuts. JLCPCB preset (Protel extensions, separate drill file).
3. **File → Fabrication Outputs → Drill Files:** Excellon, PTH + NPTH combined.
4. Zip Gerbers. JLCPCB/PCBWay: 2-layer, 1.6mm FR4, HASL, 5 copies min.

---

## PCB Footprint & Pinout Reference

### Through-Hole Critical Dimensions

| Part | Drill | Pitch | Body | Notes |
|------|-------|-------|------|-------|
| PJ-307 3.5mm jack | 1.2mm signal / 1.4mm GND tabs | 2.5mm signal pins | 14.3×12×8mm | Panel hole 6.0mm dia. KiCad: `Connector_Audio:Jack_3.5mm_PJ307_Horizontal`. Body extends ~14.3mm into PCB from the NORTH edge — keep the OLED window (starts Y19.75) and the Daisy clear of that band. |
| CYT1100 encoder | 0.8mm signal / 1.5mm legs | 2.54mm (A–GND–B) / 5.0mm (SW row) | 12.5×13.4×13mm body | Shaft 20mm. KiCad: `Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm` — footprint-compatible with CYT1100. Shaft hole 7.0mm. |
| EG1218 SPDT switch | 1.0mm | 2.5mm (3 pins) | 10.2×4×4mm body, 3mm slider, 7mm total height | 3A rated. Panel slot 4×8mm. KiCad: `Button_Switch_THT:SW_Slide_1P2T_CK_OS102011MS2Q` (same footprint, EG1218 fits comfortably inside the larger courtyard). |
| VL53L1X TOF400C | n/a (header) | 2.54mm | ~25×12mm module | Mounts on 6-pin header. 25mm fits in the 26mm clearance between the two encoder body edges at x=27.5 and x=49.5. |

**PJ-307 pin map (5-pin THT):**

| Pin | Function |
|-----|----------|
| 1 | TIP (L channel / headphone) |
| 2 | RING (R channel / headphone) |
| 3 | SLEEVE (GND) |
| 4 | Switch NC — normally pressed against TIP by spring contact; lifts off when plug inserted |
| 5 | Switch NC — normally pressed against RING; lifts off when plug inserted |

**Use:**
- **J_IN:** pins 4 + 5 → MIC_OUT (so mono MAX4466 feeds both stereo channels when no plug; both contacts release on plug insertion).
- **J_OUT:** pin 4 → HP_DETECT (via R_HPDET pullup + C_HPDET filter). When plug inserted, pin 4 floats off TIP → pullup wins → Daisy reads HIGH → firmware drops SHDN. Pin 5 NC.

---

### Module Physical Pin Order

**DWEII 2.42" OLED SSD1309 — 71×43.5mm**

| Pin | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|-----|---|---|---|---|---|---|---|
| Silkscreen | GND | VCC | SCK | SDA | RES | DC | CS |
| Daisy | GND | 3.3V | D8 (SPI1_SCK) | D10 (SPI1_MOSI) | D2 (RST) | D1 (DC) | D7 (CS) |

SPI mode default. Driver SSD1309 is SSD1306-compatible (Adafruit_SSD1306 with SSD1306_128_64 constructor). Most 2.42" SSD1309 modules have an onboard boost converter for the OLED panel VCC; `SSD1306_EXTERNALVCC` is the safe init parameter (tells driver not to enable the SSD1306's internal pump, which the SSD1309 doesn't have). Verify your module's onboard boost is functional before final assembly.

OLED PCB margin to main PCB edges: ~2mm each side (71mm module on the 75mm board) — tighter than the legacy 84mm grid; confirm routing/silkscreen room and the window center (37.5, 40.25) against the Enclosure Cutouts table.

---

**MPU9250 GY-9250 — 25×15mm**

10-pin single-row, L→R:

| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|
| VCC | GND | SCL | SDA | EDA | ECL | ADO | INT | NCS | FSYNC |
| 3.3V | GND | D13 | D14 | — | — | float=0x68 | — | — | — |

Pins 1–4 used. ADO float = 0x68. AK8963 magnetometer accessible via I2C pass-through at 0x0C (bolderflight handles automatically). Confirm WHO_AM_I = 0x71 (real MPU9250); 0x73 = ICM-20948 disguise, needs different library.

---

**WWZMDiB SD Card Module — ~25×15mm**

| 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|
| GND | VCC | MISO | MOSI | SCK | CS |
| GND | 5V | D9 | D10 | D8 | D0 |

3.3V/5V compatible (onboard LDO). SPI interface.

---

**PAM8403 Stereo Amp — 2×3W, 2.5–5V (verify pinout for your module)**

Assumed 5-pin input header (variant B from Audio Wiring section):

| Pin | 1 | 2 | 3 | 4 | 5 |
|-----|---|---|---|---|---|
| Silkscreen | VIN | GND | L_IN | R_IN | SHDN |
| Net | +5V | GND | AUDIO_OUT_L | AUDIO_OUT_R | SHDN_NET |

Speaker outputs: 4 solder pads, L+/L−/R+/R−, separate from the input header. ~710mA at max volume, 2×3W into 8Ω at 5V — within uxcell 2W rating at moderate volume.

If your module is variant A (6-pin) or C (8-pin), swap `J_AMP` footprint to match. Net assignments stay the same; just add/remove GND pins.

---

**TP4056 USB-C w/ DW01A — ~25×17mm**

4-pin 2.54mm header, right short edge (top→bottom):

| 1 | 2 | 3 | 4 |
|---|---|---|---|
| OUT+ | B+ | B− | OUT− |
| VBAT_RAW → SW1 pin 1 | LiPo B+ | LiPo B− | GND |

USB-C port on TP4056 is **unused** in this design (charging happens via the soldered wire from Daisy VBUS → TP4056 IN+). You can leave it unpopulated or short its CC resistors as the board ships from the factory.

---

**XL3608-5V Boost — 24.4×13.7mm**

2-pin 2.54mm header rows on both short edges:

| Left (output) | Right (input) |
|---------------|---------------|
| VOUT+ → +5V rail | VIN+ → VBAT_SW (from SW1 pin 2) |
| GND | GND |

Voltage selection jumper on bottom long edge: bridge **5V** pad. Verify output with multimeter before wiring downstream. **EN pin is not used** — left at factory-default (pulled high internally on most XL3608 boards). All hard switching happens on VIN.

---

**NeoTrellis → J_TRELLIS (Adafruit #3954 — 60×60mm)**

Remove the factory JST connector. Solder wires directly to NeoTrellis side copper pads. Route around the main PCB edge to J_TRELLIS female header on PCB bottom side.

| J_TRELLIS pin | Signal | Daisy |
|---------------|--------|-------|
| 1 | +5V | 5V rail |
| 2 | GND | GND |
| 3 | SCL | D11 |
| 4 | SDA | D12 |

NeoTrellis INT not wired by default — poll in firmware.

---

**VL53L1X → J_TOP_SENSOR (ACEIRMC TOF400C — ~25×12mm)**

| 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|
| VCC | GND | SCL | SDA | GPIO1 | XSHUT |
| 3.3V | GND | D13 | D14 | NC | 10kΩ tie-high to 3.3V |

I2C address 0x29. Top side, soldered flat, sensor window faces UP through the stadium 10×6.5 OPEN cutout at window center **(37.05, 69.43)** (Cutout #7).

---

**MAX4466 → J_MAX4466**

| 1 | 2 | 3 |
|---|---|---|
| OUT | GND | VCC |
| MIC_OUT (→ J_IN pin 4 + pin 5) | GND | +3.3V |

3-pin header on the module edge. Mic capsule on top of module faces UP through the Ø10 front-face cutout at **(11, 8.2)** (Cutout #8); top face here is 15.3mm above the PCB back. Module mounts top-side near the top-left corner.

---

**FSR → J_FSR (SF45-65 — 50×65mm body, 45×65mm film, 45×45mm active)**

Divider: 3.3V → FSR → A0 → 10kΩ → GND (+ 100nF to GND). J_FSR is a 3-pin 2.54mm header on **PCB bottom side at x≈71mm, y≈95mm** (TBD on the 75×140 board; was x=80 on the legacy 84mm grid). FSR sits between NeoTrellis PCB bottom and main PCB top surface; wires route from under NeoTrellis edge.

---

### Speaker Dimensions (2× uxcell 20×40mm 8Ω 2W)

Housing placement is **side-firing into the "ears"** (near the top, far left/right, firing sideways) — **not** rear-firing behind the battery. See the **Speakers / "Ears"** section for the authoritative housing treatment (now **defined — Stage 2 of the build**).

| Parameter | Value |
|-----------|-------|
| Footprint | 20×40mm, **5.5mm thick** |
| Mounting | near the **very top**, on the **far left/right sides**, firing **straight out sideways** into the ear scoops |
| Solder tab spacing | ~14mm center-to-center |
| Sound escape | "bigger" side-wall sound holes over each driver, biased up/forward by the ear scoop lip — sizes/pattern **TBD** |

---

## Velocity (FSR)

SF45-65 sits between NeoTrellis PCB bottom and main PCB top surface. NeoTrellis button presses compress it. Reads **summed pressure across all 16 pads** (same approach as original Pocket Operators — not per-pad velocity). Velocity captured at note-on; channel aftertouch sent continuously via USB MIDI. Calibrate threshold in firmware (currently 160/4095 ≈ 4% of 12-bit range, matching the original 40/1023 ratio).

---

## Motion + Distance Sensors

**MPU9250** (bottom side, PCB center x≈37.5 y≈65, I2C4 @ 0x68 / 0x0C):
- Gyro/accel: tilt/shake/rotation → MIDI CC or DSP params at ~50Hz (ConfigSrd=19)
- Magnetometer: heading as expression source. Expect hard/soft-iron offsets from speaker magnets + XL3608 inductor. Run hard-iron calibration on fully-assembled device; store offsets to SD. Apply ~1–2Hz lowpass on heading output.

**VL53L1X** (top side, window center (37.05, 69.43), I2C4 @ 0x29): hand-distance theremin control, sensor window faces up. ~30Hz polling, 30–1500mm useful range. Map to filter cutoff, volume, pitch, or any DSP param.

Both sensors share I2C4 cleanly (no address conflict). XSHUT tied high via 10kΩ to 3.3V.
