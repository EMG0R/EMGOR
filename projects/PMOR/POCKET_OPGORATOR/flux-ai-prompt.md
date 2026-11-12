# Pocket OpGorator — PCB Reference (v0.6)

**One doc for everything.** Layout decisions, component specs, dimensions, pinouts, Amazon links, netlist, and build notes. Last verified May 2026.

---

## Board Overview

- **Dimensions:** 84 × 150 mm portrait
- **Origin:** top-left corner (0, 0); +x = right, +y = down
- **¼-grid columns:** x = 21 (left), x = 42 (center), x = 63 (right)
- **M2 mounting holes:** NPTH 2.2 mm drill at (4,4), (80,4), (4,146), (80,146)
- **Layers used:** F.Cu (top/user-facing), B.Cu (bottom/interior), F.SilkS, B.SilkS, Edge.Cuts
- **Footprint injection script:** `kicks/inject_footprints.py` (text-injection into .kicad_pcb)

---

## Side Assignments

- **F.Cu = top = user-facing** (seen through front shell)
- **B.Cu = bottom = enclosure interior** (bulky/internal stuff)

| Component | Side | Why |
|---|---|---|
| OLED (J_OLED) | F.Cu | User reads display through front-shell window |
| Encoders (ENC1, ENC2) | F.Cu | Knobs poke through front shell |
| VL53L1X ToF (J_TOP_SENSOR) | F.Cu | Lens needs to face front |
| MAX4466 mic (J_MAX4466) | F.Cu | Capsule must face front-face mic hole |
| Power switch (SW1) | F.Cu | Slider operates from the front |
| Audio jacks (J_IN, J_OUT) | **B.Cu** | Mount from underside; barrels exit through top-edge housing wall |
| Daisy Seed (J_DAISY_L/R) | **B.Cu** | Sits in cavity between PCB and battery |
| MPU9250 (J_MPU9250) | **B.Cu** | Too large for F.Cu — see compromise note under coordinates |
| PAM8403 amp (J_AMP_IN/OUT) | **B.Cu** | Speaker-wire routing on back |
| SD card (J_SDCARD) | **B.Cu** | Card slot accessible through back shell |
| TPS61088 boost (4 pads) | **B.Cu** | Bulky power module |
| TP4056 charger (J_TP4056) | **B.Cu** | Battery management with the battery |
| NeoTrellis cable (J_TRELLIS) | **B.Cu** | Input header — bottom area, out of the way |
| FSR cable (J_FSR) | **B.Cu** | Same — just an input |
| Aux GPIO (J_AUX) | **B.Cu** | Future expansion |
| Discrete passives (R, C) | **B.Cu** | SMD on back, near parent nets |
| Test points | **B.Cu** | Bench-debug from back |

**B.Cu mirror note:** footprints on B.Cu are geometrically mirrored vs F.Cu. Pin numbers stay electrically correct; KiCad handles the mirror automatically when you flip to back layer. Netlist pin numbers in this doc refer to the *module's own silkscreen labels*, not PCB footprint geometry.

---

## Placement Coordinates

### F.Cu (top / user-facing)

| Ref | KiCad (at x y rot) | Board center | Notes |
|---|---|---|---|
| J_IN | (21.0, 1.8, 270) B.Cu custom | x=21, barrel at y=−3.5 | Audio IN jack — B.Cu mount, barrel exits top edge |
| J_OUT | (63.0, 1.8, 270) B.Cu custom | x=63, barrel at y=−3.5 | Audio OUT jack — same |
| SW1 | (21.0, 22.0) F.Cu | (21, 22) | SS12F44H power switch |
| J_MAX4466 | (63.0, 22.0, 270) F.Cu | (63, 22) | 1×3 header, pins in +x |
| J_OLED | (34.38, 31.5, 270) F.Cu | center x=42 | 1×7 header, pin 1 (GND) leftmost |
| ENC1 | (13.5, 79.5) F.Cu | shaft at (21, 82) | Left encoder — origin offset from shaft |
| J_TOP_SENSOR | (35.65, 82.0, 270) F.Cu | center x=42 | 1×6 header, pins in +x |
| ENC2 | (55.5, 79.5) F.Cu | shaft at (63, 82) | Right encoder |

### B.Cu (bottom / interior)

| Ref | KiCad (at x y rot) | Notes |
|---|---|---|
| J_DAISY_L | (29.3, 25.35) B.Cu | 1×20, pin 1 at top |
| J_DAISY_R | (54.7, 25.35) B.Cu | 1×20, pin 1 at top |
| J_MPU9250 | (49.5, 90.0) B.Cu | 1×10, pin 1 at top — see compromise note |
| J_TRELLIS | (4.0, 95.0) B.Cu | 1×4 |
| J_FSR | (80.0, 95.0) B.Cu | 1×3 |
| J_TP4056 | (15.0, 95.0) B.Cu | 1×4 |
| BOOST_VOUT_pad | (36.5, 86.5) B.Cu | 5mm pad / 2.5mm drill |
| BOOST_VIN_pad | (73.5, 86.5) B.Cu | 5mm pad / 2.5mm drill |
| BOOST_GND_VOUT_pad | (36.5, 103.5) B.Cu | 5mm pad / 2.5mm drill |
| BOOST_GND_VIN_pad | (73.5, 103.5) B.Cu | 5mm pad / 2.5mm drill |
| J_AMP_IN | (15.0, 122.0, 270) B.Cu | 1×5, PAM8403 input edge |
| J_AMP_OUT | (15.0, 128.0, 270) B.Cu | 1×4, PAM8403 output edge |
| J_SDCARD | (55.0, 122.0, 270) B.Cu | 1×6 |
| *(speakers)* | — | Wires soldered directly to PAM8403 L+/L−/R+/R− — no PCB pads |
| *(battery)* | — | Wires soldered directly to TP4056 B+/B− — no PCB pads |
| R_FSR 10kΩ 0805 | (78.0, 100.0) | |
| R_VBAT1 100kΩ 0805 | (18.0, 88.0) | |
| R_VBAT2 100kΩ 0805 | (20.0, 90.0) | |
| C_BULK_5V 470µF/10V THT D8/P3.5 | (5.0, 80.0) | |
| TP_GND / TP_5V / TP_3V3 / TP_VBAT | x=4, y=50/53/56/59 | |

### MPU9250 placement compromise

User wanted IMU on F.Cu with encoders/lidar. Physical reality: F.Cu is fully packed — top jacks (y=0–14), mic/switch row (y=15–28), OLED (y=30–73.5), encoders/lidar (y=76–88), NeoTrellis (y=90–150). No 25×15 mm module fits in any gap. Side margins (x=0–12 and x=72–84) are only 12 mm wide. IMU placed on B.Cu at (49.5, 90.0) — directly behind the encoder/lidar zone. Tilt/orientation Z-axis sign flips vs front-mount, correctable in firmware calibration.

---

## Hole Sizes

| Type | Drill | Pad | Notes |
|---|---|---|---|
| Module/header signal pins | 1.0 mm | 1.8 mm copper | All standard 2.54 mm headers |
| Audio jack signal pins | 1.2 mm | 1.8 mm copper | PJ-307 pins 1–5 |
| Audio jack GND mounting tabs | 1.4 mm | 1.8×2.0 mm oval | PJ-307 tabs |
| TPS61088 corner pads | 2.5 mm | 5.0 mm copper | Double as mechanical mount |
| M2 board mounting holes | 2.2 mm | NPTH, no copper | 4 corners |

---

## Symmetry Rules

Lock these to exact symmetry (not "close enough"):

- Audio jacks: J_IN x=21, J_OUT x=63 — ±21 mm from center x=42
- Encoders: ENC1 x=21, ENC2 x=63 — same
- Mic + switch row: J_MAX4466 x=63, SW1 x=21
- OLED: centered at x=42
- VL53L1X: centered at x=42
- M2 holes: 4× exactly 4 mm from each corner
- USB-C cutout: centered at x=42 on top edge
- Daisy sockets: x=29.3 and x=54.7 (±12.7 mm from center, 25.4 mm apart)

---

## Module Silkscreen Outlines

Board-level `gr_rect` on silk layers — visual assembly aid, no electrical function:

| Module | Layer | Rect (x1,y1 → x2,y2) | Actual size |
|---|---|---|---|
| SSD1309 OLED | F.SilkS | 6.5,30.0 → 77.5,73.5 | 71×43.5 mm |
| MAX4466 mic | F.SilkS | 58.5,22.0 → 72.5,43.0 | ~20.8×13.8 mm |
| MPU9250 IMU | B.SilkS | 34.1,88.7 → 49.5,114.2 | 25.5×15.4 mm |
| TP4056 charger | B.SilkS | −13.0,95.0 → 15.0,112.0 | 28×17 mm (USB-C overhangs left edge) |
| PAM8403 amp | B.SilkS | 9.5,116.0 → 30.5,134.0 | 21×18 mm |
| TPS61088 boost | B.SilkS | 35.0,84.0 → 75.0,106.0 | 40×22 mm |
| SD card | B.SilkS | 40.4,122.0 → 82.4,146.0 | 42×24 mm |

---

## Netlist

- **VBAT_RAW:** J_TP4056 pin 1 (OUT+) → SW1 pin 1 → R_VBAT1
- **SW_OUT:** SW1 pin 2 (COM) → BOOST_VIN_pad
- **+5V:** BOOST_VOUT_pad → J_DAISY_R pin 2, J_AMP_IN pins 1 & 4, J_TRELLIS pin 1, J_SDCARD pin 2, C_BULK_5V+
- **+3V3:** J_DAISY_R pins 3 & 20 → J_MAX4466 pin 3, J_MPU9250 pin 10, J_TOP_SENSOR pin 1, J_FSR pin 1, J_OLED pin 2
- **GND:** J_DAISY_R pin 1 → all other GND
- **AUDIO_IN_L/R:** J_DAISY_L pins 16/17 → J_IN pins 1/2 (TIP/RING)
- **AUDIO_OUT_L/R:** J_DAISY_L pins 18/19 → J_OUT pins 1/2 AND J_AMP_IN pins 2/3
- **MIC_OUT:** J_MAX4466 pin 1 → J_IN pins 4 & 5 (auto-switch to mic when no plug)
- **Speaker:** PAM8403 L+/L−/R+/R− → wires soldered directly to speaker tabs (no PCB pads)
- **HP_DETECT:** J_OUT pin 4 → D6 (left col 7th from bottom, INPUT_PULLUP)
- **SHDN_CTRL:** D5 (left col 6th from bottom, OUTPUT) → PAM8403 SHDN chip pin (bodge wire — not on module header)
- **I2C1:** J_DAISY_L pins 13/12 (SDA/SCL) → J_TRELLIS pin 3 (SDA) / pin 4 (SCL). NeoTrellis header top→bottom: pin1=+5V, pin2=GND, pin3=SDA, pin4=SCL (user-verified)
- **I2C (MPU9250 + VL53L1X bus):** J_DAISY_L pins 14/15 (board-labeled D13/D14) → J_MPU9250 pins 8/7 AND J_TOP_SENSOR pins 4/3. ⚠ Use `I2C_1` peripheral (NOT I2C_4) with `GetPin(13)` SCL / `GetPin(14)` SDA. libDaisy applies AF4 to all I2C peripherals; AF4 on PB6/PB7 (D13/D14) routes to I2C1, not I2C4 despite the board label. I2C_4 never works on these pins with libDaisy. Both sensors confirmed working on this bus: MPU9250 @ 0xD0 (WHO_AM_I=0x71), VL53L1X @ 0x52 (model_id=0xEACC). VL53L1X uses 16-bit register addresses (mem_address_size=2). ⚠ SDA/SCL labels reversed on user's physical VL53L1X module.
- **SPI_SD (soft-SPI):** right col 6th from bottom (D28/CS, lowest) → J_SDCARD CS; right col 7th from bottom (D27/MOSI) → J_SDCARD MOSI; right col 8th from bottom (D26/CLK) → J_SDCARD CLK; right col 9th from bottom (D25/MISO) → J_SDCARD MISO

---

## Component Reference

All components going on or connecting to the main PCB. Dimensions measured/verified May 2026.

---

### 1. Daisy Seed Rev7

**Source:** [Electrosmith product page](https://electro-smith.com/products/daisy-seed) | [Datasheet PDF](https://www.electrokit.com/upload/product/41020/41020659/Daisy_Seed_datasheet_v1.0.5.pdf) | [Official pinout CSV](https://github.com/electro-smith/DaisyWiki/blob/master/resources/Daisy_Seed_Pinout.csv)

| Spec | Value |
|---|---|
| PCB dimensions | **60.96 × 25.4 × 1.6 mm** (2.4″ × 1.0″) |
| Pins | 40 total — 2 rows × 20, 2.54 mm pitch, 25.4 mm row-to-row |
| USB | **Micro USB** on one short edge (Rev7 and all current revisions) |
| MCU | STM32H750 @ 480 MHz |
| Codec | PCM3060 stereo 24-bit, AC-coupled |
| Memory | 64 MB SDRAM, 8 MB QSPI flash |
| 3.3V out | Pin 38 (digital), Pin 21 (analog) |

**Pin map — LEFT ROW (pins 1–20):**

| Pin | Daisy label | Used for |
|---|---|---|
| 1 | USB_HS_ID | ENC1_A (D0) |
| 2 | SDMMC1_D3 | ENC1_B (D1) |
| 3 | SDMMC1_D2 | ENC1_SW (D2, INPUT_PULLUP) |
| 4 | SDMMC1_D1 | OLED_DC (D3) |
| 5 | SDMMC1_D0 | OLED_RST (D4) |
| 6 | SDMMC1_CMD | SHDN_CTRL (D5, OUTPUT — PAM8403 SHDN active LOW, HIGH=amp on) |
| 7 | SDMMC1_CK | HP_DETECT (D6, INPUT_PULLUP — J_OUT pin 4; LOW=plug in, HIGH=no plug) |
| 8 | SPI1_NSS | OLED_CS |
| 9 | SPI1_SCK | SPI_SCK (OLED only — SD SCK routed from right col pin 29) |
| 10 | SPI1_MISO | (freed — SD MISO routed from right col pin 26) |
| 11 | SPI1_MOSI | SPI_MOSI (OLED only — SD MOSI routed from right col pin 25) |
| 12 | I2C1_SCL | I2C1_SCL (NeoTrellis) |
| 13 | I2C1_SDA | I2C1_SDA (NeoTrellis) |
| 14 | USART1_TX / I2C alt | I2C4_SCL (MPU9250 + ToF) |
| 15 | USART1_RX / I2C alt | I2C4_SDA (MPU9250 + ToF) |
| **16** | **AUDIO IN L** | **AUDIO_IN_L → J_IN TIP** |
| **17** | **AUDIO IN R** | **AUDIO_IN_R → J_IN RING** |
| **18** | **AUDIO OUT L** | **AUDIO_OUT_L → J_OUT TIP + PAM8403 L_IN** |
| **19** | **AUDIO OUT R** | **AUDIO_OUT_R → J_OUT RING + PAM8403 R_IN** |
| 20 | AGND | GND |

**Pin map — RIGHT ROW (pins 21–40, bottom-right up to USB end):**

| Pin | Daisy label | Used for |
|---|---|---|
| 21 | 3V3A | +3V3 analog rail |
| 22 | ADC_INP10 (PC0) | FSR_ANA |
| 23 | ADC_INP15 (PA3) | ENC2_B |
| 24 | ADC_INP5 (PB1) | ENC2_SW |
| 25 | PA7 (SPI1_MOSI) | unused — right col duplicate of left col SPI_MOSI |
| 26 | PA6 (SPI1_MISO) | unused — right col duplicate of left col SPI_MISO |
| 27 | ADC_INP11 (PC1) | VBAT_MON |
| 28 | ADC1_INP4 (PC4) | unused (freed from SD_CS) |
| 29 | PA5 (SPI1_SCK) | unused — right col duplicate of left col SPI_SCK |
| 30 | PA4 (SPI1_NSS) | ⚠ DO NOT USE — same MCU pin as OLED_CS (left col pin 8 / D7) |
| 31 | PA1 | ENC2_A (D24) |
| 32 | — | SD_MISO (D25) — soft-SPI, right col 12th from top |
| 33 | — | SD_CLK  (D26) — soft-SPI, right col 13th from top |
| 34 | — | SD_MOSI (D27) — soft-SPI, right col 14th from top |
| 35 | — | SD_CS   (D28) — soft-SPI, right col 15th from top |
| 36–37 | — | unused |
| 38 | 3V3D | +3V3 digital rail |
| 39 | VIN | +5V rail input |
| 40 | GND | GND star point |

**Physical notes:** Pin 1 is at the micro USB corner. Pins 1–20 go DOWN one long edge; pins 21–40 go UP the opposite long edge starting from the OPPOSITE corner. Pin 40 (GND) is directly across from pin 1. With Daisy mounted USB-end-up (toward board top at y≈17), pins 16–20 (audio) are at the BOTTOM of the left socket row.

---

### 2. DWEII 2.42″ SSD1309 OLED

**Source:** [Amazon B0B2R57SCJ](https://www.amazon.com/dp/B0B2R57SCJ)

| Spec | Value |
|---|---|
| PCB dimensions | **71.0 × 43.5 × 1.6 mm** |
| Active display area | 55.01 × 27.49 mm, 128×64 pixels |
| Header location | **Top long edge** — 7-pin row, 2.54 mm pitch |
| Pin 1 | GND, leftmost when viewing from front |
| Pin order (L→R front view) | **1:GND, 2:VCC, 3:SCK, 4:SDA(DIN), 5:RES, 6:DC, 7:CS** |
| Default mode | SPI (R5 bridged at factory) |
| Driver IC | SSD1309 (onboard 12V boost for OLED panel) |
| Library | Adafruit_SSD1306 + Adafruit_GFX (works for SSD1309) |

⚠ **Variant warning:** DWEII ships two layouts — (a) top-long-edge header (this one) and (b) short-edge header. Confirm yours before locking footprint.

---

### 3. ACEIRMC TOF400C VL53L1X

**Source:** [Amazon B0DC6M6G7W](https://www.amazon.com/dp/B0DC6M6G7W)

| Spec | Value |
|---|---|
| PCB dimensions | **~13 × 18 mm** |
| Sensor lens | Upper-right corner of PCB (offset, not centered) |
| Header | Bottom long edge, 6 pins, 2.54 mm pitch |
| Pin order (1→6, L→R per user's silkscreen) | **1:VIN, 2:GND, 3:SDA, 4:SCL, 5:INT, 6:SHUT** |
| Supply | 3.0–5.0 V |
| Range | 40 mm min (blind spot) – 4000 mm max |
| I2C address | 0x29 |

⚠ **SDA/SCL are REVERSED from typical published pinout** — verified against user's physical module. Firmware: distance filter should use `dist > 40` for blind spot.

---

### 4. CYT1100 Rotary Encoder

**Source:** [Amazon B07DM2YMT4](https://www.amazon.com/dp/B07DM2YMT4) — Cylewet 5-pack

| Spec | Value |
|---|---|
| Body | ~12.5 × 13.4 × 13 mm |
| Shaft | 6 mm D-shaft × 20 mm long |
| Pins | 3-pin row (A–GND–B, 2.54 mm pitch) + 2-pin SW row (5 mm pitch), rows 5 mm apart |
| Detents | 20 per rotation |
| KiCad footprint | `Rotary_Encoder:RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm` |

**Origin offset:** shaft center at local (7.5, 2.5). To place shaft at board position (21,82): footprint origin at (13.5, 79.5). Same logic for ENC2 shaft at (63,82) → origin (55.5, 79.5).

---

### 5. MAX4466 Electret Mic Preamp

**Source:** [Amazon B0D3CWM68X](https://www.amazon.com/dp/B0D3CWM68X) (Adafruit 1063 clone)

| Spec | Value |
|---|---|
| PCB dimensions | **20.8 × 13.8 × 7.5 mm** (height includes capsule) |
| Capsule location | Short edge OPPOSITE the 3-pin header |
| Pin order | **1:OUT, 2:GND, 3:VCC** |
| Pin pitch | 2.54 mm |
| Supply | 2.4–5.5 V (use 3.3 V for lowest noise) |
| Output | AC-coupled, ~1.65 V DC bias at 3.3 V |

**Layout:** capsule end must face the front-face mic hole. Module center at (63,22); capsule is ~10 mm offset toward −y from center. Orient so capsule faces front.

---

### 6. PAM8403 Stereo Amp (VKLSVAN Super Mini)

**Source:** [Amazon B0DPMDPGR6](https://www.amazon.com/dp/B0DPMDPGR6) — 3-pack

| Spec | Value |
|---|---|
| PCB dimensions | **~21 × 18 × 3.4 mm** |
| Input edge (5 pads) | `+`(VCC), `L`(L_IN), `R`(R_IN), `5V`(VCC dup), `−`(GND) |
| Output edge (4 pads) | `L+`, `L−`, `R−`, `R+` (BTL speaker outputs) |
| Pin pitch | 2.54 mm |
| Supply | 2.5–5.5 V DC |
| Output @ 5V/4Ω | 3 W per channel |
| SHDN/MUTE | Not on module header — wire directly to PAM8403 IC SHDN pin (active LOW). Controlled by D5 (SHDN_CTRL). |

| PAM8403 pad | Net |
|---|---|
| Input `+` or `5V` | +5V |
| Input `L` | AUDIO_OUT_L |
| Input `R` | AUDIO_OUT_R |
| Input `−` | GND |
| Output `L+` / `L−` | SPK_L+ / SPK_L− |
| Output `R+` / `R−` | SPK_R+ / SPK_R− |

⚠ **PAM8403 row spacing note:** J_AMP_IN at y=122, J_AMP_OUT at y=128 = 6 mm apart. Real module pin rows are ~13–15 mm apart (body is 18 mm tall). The B.SilkS outline (y=116–134) shows the discrepancy. Connector positions may need adjustment once physical module is measured.

---

### 7. PJ-307 3.5mm Stereo Audio Jack

**Source:** [Amazon B07XX8YGDX](https://www.amazon.com/dp/B07XX8YGDX) — bnafes 15-pack

| Spec | Value |
|---|---|
| Body dimensions | **14.3 × 12 × 8 mm** |
| Barrel protrusion past PCB edge | ~4 mm |
| Panel hole | 6.0 mm diameter |
| Signal pin drill | **1.2 mm** |
| GND tab drill | **1.4 mm** |
| Pin pitch | 2.5 mm (signal pins) |

**Pin map:**

| Pin | Function |
|---|---|
| 1 | TIP (left audio / signal+) |
| 2 | RING (right audio) |
| 3 | SLEEVE (GND) |
| 4 | NC-to-TIP switch (spring-contacts pin 1 when no plug) |
| 5 | NC-to-RING switch (spring-contacts pin 2 when no plug) |
| G | Two GND mounting tabs — tie to GND |

**Physical layout (7 holes total):**
```
   [barrel exits this side — top of board, y < 0]
              ↓
   ┌──────────────────────┐
   │  ●(GND tab)  ●(GND tab)  │  ← 1.4 mm drill
   │                          │
   │        ●(pin 5 SW_R)     │  ← middle pin, 1 row toward barrel
   │                          │
   │  ●(1-T) ●(2-R) ●(3-S) ●(4-SW_T)  │  ← 4-pin row, 2.5mm pitch
   └──────────────────────┘
              ↑
   [back of jack, deeper into PCB]
```

**KiCad footprint:** `Connector_Audio:Jack_3.5mm_PJ307_Horizontal` does **NOT** exist in KiCad 9 standard library. Custom inline footprint `custom:PJ307_THT_Horizontal` written in `inject_footprints.py`. Always placed B.Cu rot=270 so barrel exits top edge. At fp_y=1.8: barrel board_y = 1.8 − 5.3 = −3.5 mm (3.5 mm past y=0 board edge) ✓.

**J_IN use:** pins 4 & 5 → MIC_OUT (mono mic feeds both channels when no plug; contacts release on insertion).
**J_OUT use:** pin 4 → HP_DETECT (D6, left col 7th from bottom, INPUT_PULLUP). Low when plug inserted (switch closes to TIP/audio), high when no plug. Firmware debounces 200 ms and drives SHDN_CTRL (D5) to mute PAM8403 when headphones are in. Pin 5 NC.

---

### 8. TP4056 USB-C LiPo Charger w/ DW01A

**Source:** [Amazon B08FSRV7GS](https://www.amazon.com/dp/B08FSRV7GS)

| Spec | Value |
|---|---|
| PCB dimensions | **28 × 17 × 3.2 mm** (29 mm with USB-C overhang) |
| Module variant | HW-373 V1.2.1 (silkscreen confirmed) |
| 4-pin header | Short edge OPPOSITE USB-C connector |
| Pin order (1→4, top→bottom per user's silkscreen) | **1:OUT+, 2:B+, 3:B−, 4:OUT−** |
| Pin pitch | 2.54 mm |
| Charge current | 1.0 A (Rprog = 1.2 kΩ) |
| Overdischarge cutoff | 2.4 V ±0.1 V (DW01A) |
| Overcurrent trip | ~3 A (FS8205A) |

⚠ Many USB-C TP4056 clones omit 5.1 kΩ CC pull-downs → only charge from USB-A→C cables, not C→C. If C→C charging needed, add 5.1 kΩ resistors on CC1/CC2.

---

### 9. TPS61088 5V Boost Converter (JXJ V1.10)

**Source:** [Amazon B0G25JWWKR](https://www.amazon.com/dp/B0G25JWWKR) — EC Buying 5-pack | [TI TPS61088 datasheet](https://www.ti.com/product/TPS61088)

| Spec | Value |
|---|---|
| PCB dimensions | **40 × 22 mm** |
| Connection style | **4 corner PTH pads** — no pin header |
| Corner pad labels (back view) | top-left=VOUT, bottom-left=GND, top-right=VIN, bottom-right=GND |
| Corner pad spacing | ~37 mm horizontal × ~17 mm vertical |
| Pad hole diameter | ~2.5 mm |
| Vin range | 2.7–10 V |
| Output | 5V / 9V / 12V / ADJ via solder-jumper (ships 5V bridged) |
| Output current @ 5V from 3.7V | Up to ~5 A continuous |
| Switching freq | ~1 MHz |
| Efficiency | up to 92% |
| Trimpot | **NONE** — voltage set by solder-jumper only |

**Host PCB footprint:** 4× PTH pads at (36.5,86.5), (73.5,86.5), (36.5,103.5), (73.5,103.5) — 5mm copper pad / 2.5mm drill, B.Cu.

**Pre-install check (mandatory):** power VIN pad only, multimeter on VOUT → confirm 4.9–5.1 V before connecting load.

**IMU clearance:** keep MPU9250 ≥8–10 mm from TPS61088 inductor (located near module center-left). Route I2C orthogonal to inductor H-field axis.

---

### 10. ~~AO3401 P-channel MOSFET~~ — REMOVED

Removed in v0.5. Q1 (battery-path load switch) replaced by 3A SS12F44H slide switch. Q2 (PAM8403 gate MOSFET) removed. Amp mute is now handled via PAM8403 SHDN pin driven directly by D5 (SHDN_CTRL) — a bodge wire from the Daisy left col to the IC pin. No MOSFETs in this design.

---

### 11. WWZMDiB Micro SD Module

**Source:** [Amazon B0B779R5TZ](https://www.amazon.com/dp/B0B779R5TZ)

| Spec | Value |
|---|---|
| PCB dimensions | **~42 × 24 mm** |
| Header | Single row along long edge OPPOSITE card slot, 6 pins, 2.54 mm pitch |
| Pin order (1→6) | **1:GND, 2:VCC, 3:MISO, 4:MOSI, 5:SCK, 6:CS** |
| Supply | 4.5–5.5 V (onboard AMS1117-3.3 LDO) |
| Level shifter | 74LVC125A (5V signals → 3.3V for SD card) |
| Corner holes | Four M2 (2.2 mm) |
| **SD card format** | **FAT32** (not exFAT, not NTFS). Mac: Disk Utility → MS-DOS (FAT), or `diskutil eraseDisk FAT32 SDCARD /dev/diskN` |
| **libDaisy note** | libDaisy `SdmmcHandler` only supports hardware SDMMC1 on fixed pins D1–D6 (which conflict with OLED/encoder/amp on this PCB). SD card is routed to soft-SPI (D25=MISO, D26=CLK, D27=MOSI, D28=CS) — requires bit-bang SPI, not `SdmmcHandler` |

---

### 12. GY-9250 MPU9250 IMU

**Source:** [Amazon B0FR8WQKC3](https://www.amazon.com/dp/B0FR8WQKC3)

| Spec | Value |
|---|---|
| PCB dimensions | **~25.5 × 15.4 × 1.2 mm** |
| IC position | Offset toward the long edge OPPOSITE the header (not centered) |
| Header | One long edge, 10 pins, 2.54 mm pitch |
| Pin order (1→10, L→R per user's silkscreen) | **1:FSYNC, 2:NCS, 3:AD0, 4:INT, 5:EDA, 6:ECL, 7:SDA, 8:SCL, 9:GND, 10:VCC** |
| I2C address (AD0 float) | 0x68 |
| Supply | 3–5 V (onboard 3.3V LDO) |
| WHO_AM_I (genuine MPU9250) | 0x71 |

⚠ **Pin order is REVERSED from typical GY-9250 docs** (most show VCC at pin 1). This physical module has VCC at pin 10 — verified against user's module.

**Minimum wiring (I2C):** only pins 7–10 needed (SDA, SCL, GND, VCC). Pins 1–6 not used in this design.

---

### 13. SS12D10 SPDT Slide Switch

**Source:** [Gebildet 40PCS SS12D10 + SS12F15 pack](https://a.co/d/0a7CDlU3) — use the SS12D10 (3A ones), ignore the SS12F15 (0.5A, too weak)

| Spec | Value |
|---|---|
| Pins | 3 (SPDT, 1P2T) |
| Rating | **3 A @ 250 VAC** — handles battery power path |
| Pin pitch | Measure physical switch — likely 2mm or 2.54mm (verify before making footprint) |
| KiCad footprint | Custom — 3 through-hole pads at measured pitch, 1.0mm drill, 1.8mm pad |

**Wiring:** Pin 1 → VBAT_RAW | Pin 2 (COM) → SW_OUT → TPS61088 VIN | Pin 3 → NC (open = device OFF). USB charging works in both positions (TP4056 is upstream).

**Why not SS12F15 (other switch in pack):** 0.5A rated — way undersized for 3A battery-path current. Only use SS12D10.

---

### 14. USB-C + Micro-USB Single-Port Charging + Data Solution

**Replaces the Sovunis rigid adapter.** One external USB-C port handles charging (→ TP4056) + data/audio/programming (→ Daisy) simultaneously.

#### 14a. USB-C Female Breakout Board

**Source:** [Amazon a.co/d/0dv4nUcr](https://a.co/d/0dv4nUcr) — *Type-C Breakout Board, 16-pin female, 10-pack* (the part actually used)

| Spec | Value |
|---|---|
| Connector | USB-C female, full 16-pin breakout |
| Pads exposed | VBUS, GND, D+, D−, CC1, CC2 |
| CC resistors | 5.1 kΩ on CC1 + CC2 (onboard) — enables USB-C to USB-C cables |
| Chips | None — pure passthrough, no conversion IC |

**Mount:** routed to the external USB-C opening in the housing (legacy PCB-grid position x=42mm). Hot-glue or press-fit. **Housing note:** the authoritative pebble-shell enclosure places the external USB-C opening on the **SOUTH wall** (not the top edge) — see the **Enclosure** / **Enclosure Cutouts** sections of `pocket-OpGorator.md` for the current housing spec; this file is the PCB/KiCad reference and is not the housing source of truth.

**Wiring:**
```
VBUS → TP4056 IN+   (charging)
VBUS → micro-USB pin 1  (Daisy VBUS detection — required for USB enumeration)
D+   → micro-USB pin 3
D−   → micro-USB pin 2
GND  → shared GND + micro-USB pin 5
CC1/CC2 → leave as-is (resistors already onboard)
```

> **Backfeed-safe — verified June 2026.** Sharing VBUS between TP4056 IN+ and the Daisy's micro-USB pin 1 is safe: the Daisy Seed has onboard **D1/D2 isolation diodes** between USB VBUS and the VIN/5V regulator rail, so the boost's 5V on VIN can never push back out the USB pin onto this VBUS node (no phantom 5V into the charger, no recirculating loop). **No external Schottky needed.** VBUS *must* still reach Daisy pin 1 — it's required for USB enumeration (the STM32 senses VBUS to detect the host). Powering from USB + battery at once is a supported condition; the regulator takes the higher of the two sources. Source: Seed schematic via Electrosmith forum "Seed power supply" thread.

#### 14b. Micro-USB 5-Pin Male Solder Connector

**Source:** [Amazon a.co/d/09tX9iMK](https://a.co/d/09tX9iMK) — *20PCS Micro USB Type-B Solder Connector, 5-pin male jack port w/ black cover* (the part actually used)

| Spec | Value |
|---|---|
| Type | Micro-USB B male, 5-pin solder tabs |
| Pitch | ~1.37 mm |
| Plugs into | Daisy Seed micro-USB female port |

**Pinout (standardized, never varies):**
| Pin | Signal | Connect to |
|-----|--------|------------|
| 1 | VBUS | USB-C breakout VBUS |
| 2 | D− | USB-C breakout D− |
| 3 | D+ | USB-C breakout D+ |
| 4 | ID | NC (leave unconnected) |
| 5 | GND | shared GND |

Solder 4 thin wires (skip pin 4), plug into Daisy. Use flux, fine tip, steady hands — pins are ~1.37 mm pitch.

---

### 15. Externals (NeoTrellis, FSR, speakers, battery)

These mount outside the main PCB — connect via wire headers or solder pads.

**Adafruit NeoTrellis 4×4 (#3954)**
- PCB: 60×60×~3 mm
- 16 buttons + 32 NeoPixels, I2C @ 0x2E
- J_TRELLIS: +5V, GND, SDA, SCL (4 wires)
- **Cable pin order top→bottom: power (+5V), GND, SDA, SCL** (user-verified physical wiring)

**Adafruit NeoTrellis silicone (#1611)**
- 60×60 mm silicone elastomer pad, sits on top of NeoTrellis PCB

**SF45-65 FSR**
- [Amazon a.co/d/0dkBeHya](https://a.co/d/0dkBeHya)
- Body 50×65 mm, sensing 45×45 mm
- Resistance: ~10 MΩ no press → ~250 Ω heavy press
- J_FSR: +3.3V, FSR_ANA (with 10 kΩ to GND divider), GND

**uxcell 20×40 mm 8Ω 2W speakers ×2**
- [Amazon a.co/d/07LGL4J69](https://a.co/d/07LGL4J69)
- Body ~22×42×7 mm; solder tab spacing ~14 mm c-to-c
- Connect to SPK_L+/L−, SPK_R+/R− pads

**JLJLUP 10000 mAh LiPo (LP1260110)**
- [Amazon a.co/d/04zlCEU5](https://a.co/d/04zlCEU5)
- **60 × 112 × 12 mm**, 3.7 V, 37 Wh, PH2.0 connector
- Solder directly to TP4056 B+/B− (cut PH2.0, strip wires)

---

### 16. Silicone Rubber Insulating Sheet

**Source:** [Amazon B096N2Q6GM](https://www.amazon.com/dp/B096N2Q6GM)

| Spec | Value |
|---|---|
| Material | Silicone rubber |
| Color | Black |
| Thickness | **1/32″ = ~0.8 mm** |
| Sheet size | 9 × 12 inches |
| Durometer | 60A (moderately firm, slightly compressible) |
| Adhesive | None — cut and place; use double-sided tape strip if needed |

**Use:** cut a rectangle to place between battery and B.Cu underside, and/or between PCB and bare metal housing surfaces. 0.8 mm added per layer. Cut with scissors or sharp craft knife against a metal ruler — score-and-snap doesn't work on silicone.

---

## Layout Decisions — v0.6 Reasoning

1. **Audio jacks to B.Cu.** Front stays clean; barrels still exit top housing wall. Mechanically equivalent, visually cleaner front face.

2. **MPU9250 compromised to B.Cu.** User wanted it on F.Cu with encoders/lidar. No room (25×15 mm module, F.Cu is fully packed). Placed at (49.5, 90.0) B.Cu — directly behind the encoder/lidar zone. Firmware Z-axis sign flip correctable in calibration.

3. **Module silkscreen outlines.** Visual assembly aid for every breakout that plugs in. Zero electrical complexity.

4. **B.Cu footprints mirrored.** KiCad handles automatically; netlist uses component silkscreen pin numbers throughout.

5. **PJ-307 custom footprint.** `Jack_3.5mm_PJ307_Horizontal` not in KiCad 9 library. Custom `make_pj307_block()` in inject script: 4-pin row (T,R,S,SW_R) + 1 middle pin (SW_T) + 2 GND tabs. B.Cu rot=270 so barrel exits top edge.

6. **Headers oriented per module real estate.** SD module is 42×24 mm; header position must match so module sits inside its silkscreen outline.

7. **Front = user-touchable, back = service/internal.** Clear mental model for assembly and housing design.

8. **NeoTrellis + FSR headers at bottom area (y≈95).** Just cable connection points — don't deserve front-face real estate.

9. **Consistent hole sizes within each category.** Fab cost reduction, simpler assembly.

10. **True symmetry where almost-symmetric.** Locked to exact offsets for jacks, encoders, mic+switch, OLED, ToF, M2 holes, Daisy sockets.

11. **Daisy pin spacing exact.** 2.54 mm pitch, 25.4 mm row-to-row — Daisy must slot in cleanly.

12. **TPS61088 via 4 corner PTH pads.** Matches actual module interface — no pin header on this module.

---

## Firmware Architecture — Audio + Storage

### Memory hierarchy
- **SDRAM (64MB):** primary audio workspace. All sample recording and playback operates here exclusively — SD card never touches the audio hot path. At 44.1kHz stereo 16-bit: ~6 minutes of stereo audio per session.
- **SRAM (512KB):** audio callback buffers, DSP scratch, UI state.
- **SD card (soft-SPI, D25–D28):** persistent backing store. Slow (~500 KB/s bit-bang). Never written from inside the audio ISR.
- **QSPI flash (8MB):** firmware. Any leftover space can hold a small persistent config (survives SD removal), but not large enough for audio.

### SD sync — "always saving" model (NOT YET IMPLEMENTED — design intent only)

Goal: every state change is queued for SD flush automatically, like a continuous git commit. User never manually saves. UI shows one of two states:

- **SAVED** (green / solid icon) — SD card matches RAM, safe to power off
- **SAVING** (amber / animated icon) — unflushed data in queue, do not power off

**How it works:**
1. Audio callback writes samples to SDRAM ring buffer, marks dirty sectors.
2. Main loop drains dirty queue: flushes one 512-byte sector per iteration (~1ms per sector at 500 KB/s). This keeps up with stereo recording (176 KB/s) with ~3× headroom.
3. A `sync_state` flag: `CLEAN` when flush_head == write_head, `DIRTY` otherwise. OLED corner indicator reflects this.
4. Session metadata (BPM, sample slot assignments, loop points, pitch, velocity layers, effects state) written as a small binary file (`session.bin`, <4KB) on every meaningful state change, debounced 2s.
5. On power-on: read `session.bin` → stream SDRAM dump back from SD (show `LOADING…` on OLED).
6. Power-off: if `sync_state == DIRTY`, show `SAVING…` with progress bar and drain queue before cutting power (worst case ~12 sec for a full 6-min session, but background flushing keeps this short in practice).

**Key constraint:** bit-bang SD (500 KB/s) is just fast enough for continuous stereo recording + flush. Multi-track simultaneous record would need FreeRTOS or hardware SDMMC. For v1: single stereo record + playback from SDRAM is the target.

**Future:** if SD becomes a bottleneck, the only PCB-level fix is a respin that routes D1–D6 (hardware SDMMC1, 12 MB/s 4-bit) to the SD card. That requires rerouting encoder + OLED control pins to other GPIOs.

---

## What's Locked vs Open

**Locked:**
- All side assignments
- All XY placements (MPU9250 compromised per note)
- Daisy pin pitch and row spacing
- Netlist
- Hole sizes
- Module outline coordinates
- Symmetry rules

**Still open:**
- PJ-307 custom footprint vs physical jack — verify pin layout before fab
- Daisy Seed micro-USB clearance for the micro-USB male plug (test fit)
- C_BULK_5V final placement relative to boost output
- Speaker pad spacing vs uxcell tab spacing (~14 mm c-to-c)
- Heat insert holes for housing screws (the 4 corner M2 PCB mounting holes; housing uses M2×30 socket-cap screws from the back into front-shell heat-set inserts — see `pocket-OpGorator.md` Fastening section)

---

## Clearances and Gotchas

| Component | Constraint | Why |
|---|---|---|
| MPU9250 | ≥8–10 mm from TPS61088 inductor | Magnetic interference |
| MPU9250 | Far from speaker magnets | Same |
| MAX4466 | Far from TPS61088 switching node | EMI into mic preamp |
| MAX4466 | Power from +3V3 not +5V | Lowest noise per datasheet |
| TPS61088 | Verify 5V output before connecting load | Factory solder bridge may be wrong |
| SS12F44H | Carries battery current directly | EG1218 (200mA) would fail |
| TP4056 | Verify silkscreen pin order | Clones vary |
| OLED | Confirm top-long-edge header variant | DWEII ships two layouts |
| PAM8403 | SHDN not on module header | Bodge wire from D5 direct to PAM8403 IC SHDN pin required for mute |
| Daisy | Audio pins 16–19 (not 33–38) | Per official Electrosmith CSV |
| VL53L1X | SDA/SCL reversed on this module | Verified against user's physical board |
| USB breakout + micro-USB male | Wired hop, not rigid | USB-C breakout on SOUTH wall → flying leads → micro-USB male into Daisy; Daisy placement is free. Housing spec in `pocket-OpGorator.md` |
| Audio jacks | Barrel at board_y = −3.5 mm | fp_y=1.8, barrel local offset −5.3 mm |
| PAM8403 headers | 6mm apart in script vs ~15mm real | Silkscreen outline flags this; needs physical verification |

---

## Script Notes — inject_footprints.py

**v3.2 (current):** 22 footprints + 7 silk outlines, 0 failures.
- Added `make_pj307_block` (custom PJ-307, not in KiCad library)
- Added `make_gr_rect` and 7 module silkscreen outlines
- J_MPU9250 y: 63.57 → 90.0
- J_AMP_OUT(1×6) → J_AMP_IN(1×5); J_AMP_PWR(1×2) → J_AMP_OUT(1×4)

**B.Cu+rot=270 math:** `board_x = fp_x + local_y`, `board_y = fp_y + local_x`. Mirror B.Cu (−lx,ly) then rotate 270° CCW.

**v3.1:** SS12F44H custom block; 4× TPS61088 corner PTH pads.

**v3.0:** Initial 22-footprint placement.
