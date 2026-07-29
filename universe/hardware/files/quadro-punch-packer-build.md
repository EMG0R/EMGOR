# Quadro Punch Packer — 10" Hemisphere Speaker (v2: Coaxial)

**Project: EMGOR SYNTH | Date: 3/25/26 | Updated: 5/8/26 | Target Done: 7/1/26**

---

## Build Status — DELETE BEFORE FINAL DOC

**Last session stopped at:** JFET master volume stage built dead-bug style. Signal chain wiring in progress. 4PDT toggle switch ordered. Charging circuit issue diagnosed and fix ordered.

**Completed:**
- Battery pack (3S, 3× Samsung 30Q)
- BMS wired (all 4 balance taps + output pads)
- XL4015 replaced with DPS3003 digital CC/CV module (see charging circuit notes)
- 2Ω inrush resistor removed (no longer needed with DPS3003)
- Rocker switch wired
- Power bus established (resistor assembly removed, 1000µF cap across bus)
- All 3 amp boards powered with decoupling caps (100µF + 100nF each)
- Sub amp powered via barrel jack pins
- AMS1117 5V rail wired
- BT module powered and paired successfully
- J201 JFET master volume stage built (dead-bug style, no perfboard)
- Drivers wired with crossover filtering

**Charging circuit issue discovered this session:**
XL4015 current limit pot is defective — cannot be adjusted, stuck at ~3A. At 3A into dead/low batteries, XL4015 hits overcurrent protection and blinks/restarts in a loop. Fix: wire 5W 2Ω resistor in series on XL4015 output to limit inrush on dead batteries. Add 1N5822 Schottky diode to block back-current from battery into XL4015 when unplugged. New XL4015 board ordered — set current to **2.5A**. Verified safe: Samsung 30Q rated 4A max charge, BMS rated 20A. At 2.5A (~31W input) system outpaces full-volume draw (~20W) — can play at max volume indefinitely while plugged in.

**Parts ordered (not yet arrived):**
- mxuteuk MTS-402 4PDT mini toggle switch (ON/ON, 12 terminal) — quad/stereo switching
- 5W 2Ω wirewound cement resistors (10-pack) — charging inrush limiter
- 1N5822 Schottky diodes — back-current protection on charge line
- New XL4015 CC/CV buck module (3-pack) — replace defective board

**TODO — complete finishing plan (in order):**

**Step 1 — Charging fix (parts arriving same day):**
1. Swap XL4015 — set voltage to 12.6V, current to **2.5A** before connecting anything, verify both with multimeter
2. Wire charging path: XL4015 out+ → 1N5822 diode (stripe toward BMS) → 2Ω 5W resistor → BMS charge in+
3. Plug in USB-C, confirm no blinking, confirm charging

**Step 2 — Power on test:**
1. Rocker switch on — confirm ~11.1V at power bus
2. Confirm ~5V at BT module
3. Confirm BT appears as "MH-M18" and pairs

**Step 3 — Signal chain:**
1. Wire jack signals + BT (via 8.2kΩ) into 4 switch commons
2. Wire 4 quad outputs → 4 JFET Drain junctions
3. Bridge stereo outputs — Row 1+3 → split to Drain 1+3, Row 2+4 → split to Drain 2+4
4. From each Drain junction: wire to amp input + wire through 8.2kΩ to sub node
5. Sub node → sub amp L+R tied together, GND → star ground

**Step 4 — Audio test:**
1. BT connects and plays through all 4 coaxials + sub
2. Pot sweeps volume smoothly on all channels
3. Toggle switch — quad sounds separated, stereo sounds blended
4. Both jacks pass signal
5. Sub LP crossover knob set to ~400Hz

**Step 5 — Final check before sealing:**
1. Play at full volume while plugged in — battery holds or gains charge
2. 5 minutes at full volume — no heat, buzz, or distortion
3. Power and signal wires routed on opposite sides, no bundles
4. Seal enclosure

### Crossover caps — back-to-back method
Not using bipolar/non-polarized caps. Using 2× 220µF standard polarized caps per coaxial wired negative-to-negative = ~110µF non-polarized equivalent. 8 caps total from BEEYUIHF kit. See Crossover Values section for wiring diagram.

---

## Purpose

10" hemisphere portable speaker redesigned around coaxial drivers. 4" sub (base, firing down) + 4x 3.5" coaxial 2-way drivers (dome, firing outward at 35° above horizontal) for full-range 360° coverage. No separate tweeter — each coaxial has a built-in HF driver with internal crossover. Sub fires into floor for boundary bass reinforcement. Sub amp has built-in adjustable LP crossover; passive HP caps on coaxials at 400Hz. Quad input (two 1/8" jacks = 4 channels) or stereo merge (4PDT latching button). Bluetooth 4.2 always on and always mixed in stereo — plays alongside jacks, not interrupted by them. Chime on connect/disconnect only, no voice prompts. 3S Li-ion battery (11.1V), USB-C PD charging (works with MacBook charger brick). Fits Bambu X1C build volume (256mm) with 8mm solid walls.

---

## Hemisphere Geometry (CCRMA Placement Math)

### Build Volume Constraint

Bambu X1C build volume: 256 x 256 x 256mm. Hemisphere fits with 1mm margin per side.

| Parameter | Value |
|-----------|-------|
| Outer diameter | 254mm (10.0") |
| Wall thickness | 8mm solid PLA/PETG |
| Inner diameter | 238mm |
| Inner radius (R) | 119mm |
| Hemisphere height | 127mm outer / 119mm inner |
| Total inner volume | 3.53L |

### Driver Placement (CCRMA Equal Solid-Angle Coverage)

4 coaxial drivers at equal azimuthal spacing (90° apart: N/E/S/W) on a single ring at polar angle θ from the apex.

**Optimal angle: θ = 54.7° from apex (arctan(√2))**

This is the cube-vertex angle — the mathematically optimal polar angle for 4 equally-spaced drivers on a hemisphere. Maximizes solid-angle coverage uniformity per driver.

```
Driver axis: 54.7° from vertical = 35.3° above horizontal
```

### Center-to-Center Spacing

```
Two points on sphere radius R at same polar angle θ, separated by 90° azimuth:

cos(α) = sin²(θ) × cos(90°) + cos²(θ)
cos(α) = cos²(54.7°) = 0.333

α = arccos(0.333) = 70.5° = 1.231 rad

Great circle distance = R × α = 119 × 1.231 = 146.5mm
```

**3.5" coaxial frame: ~89mm → gap between adjacent frames: 57.5mm** — plenty of room for 8mm walls and structural ribbing between drivers. Even 4" drivers (102mm frame) would fit with a 44.5mm gap.

### Driver Positions (relative to center of base)

```
Height above equator:  R × cos(54.7°) = 119 × 0.577 = 68.7mm
Horizontal radius:     R × sin(54.7°) = 119 × 0.816 = 97.1mm

N:  (  0.0,  97.1, 68.7) mm
E:  ( 97.1,   0.0, 68.7) mm
S:  (  0.0, -97.1, 68.7) mm
W:  (-97.1,   0.0, 68.7) mm
```

### Surface Curvature at Driver Cutout

For a 78mm driver cutout (typical 3.5" coaxial):

```
Sag = R - √(R² - (d/2)²)
    = 119 - √(119² - 39²)
    = 119 - 112.4
    = 6.6mm
```

Each driver position needs a flat mounting pad printed into the dome (~78mm diameter, 6.6mm sag compensation). Straightforward in CAD.

### Internal Baffle (Sub Chamber)

Horizontal baffle at 65mm above base separates sub chamber from coaxial air space:

```
Sub chamber volume:  πh²(3R - h)/3 = π(65²)(292)/3 ≈ 1.29L sealed
Upper volume:        3.53 - 1.29 ≈ 2.24L shared (coaxials, infinite baffle)
Baffle diameter:     2 × √(R² - (R-h)²) = 2 × √(119² - 54²) = 212mm
```

---

## Signal Flow

```
  1/8" Jack A ──┐
                ├──── [4PDT TOGGLE]  QUAD:   Jack A L/R → Ch1/Ch2 Drain nodes
  1/8" Jack B ──┘                   STEREO: L+L / R+R summed via 8.2kΩ pairs
                                             ↓
                              [JFET DRAIN NODES ×4]  ←── BT bypasses switch here
                                             │
  MH-M18 BT → AD828 preamp ──── (bypasses 4PDT entirely) ────────────────────┘
    BT L → 8.2kΩ → Ch1 Drain node                  ↑ always on, always stereo
    BT L → 8.2kΩ → Ch3 Drain node
    BT R → 8.2kΩ → Ch2 Drain node
    BT R → 8.2kΩ → Ch4 Drain node

  JFET VOLUME STAGE (per channel ×4):
    signal in → [8.2kΩ] → Drain node → signal out
                           J201: D=node, S=GND, G=[1MΩ]=wiper
    10kΩ pot: CW=+5V, CCW=GND, wiper → all 4 gates via 1MΩ each

  FROM DRAIN NODES:
    Ch1 → Amp1 L input                Ch3 → Amp2 L input
    Ch2 → Amp1 R input                Ch4 → Amp2 R input
    Ch1+Ch2+Ch3+Ch4 → 8.2kΩ each → mono sum node → Sub Amp L+R (tied)

  AMPLIFIERS:
    Amp1 (TPA3116D2 stereo, bass+treble knobs):
      L → [2×220µF NP HP XO ~363Hz] → Coax N
      R → [2×220µF NP HP XO ~363Hz] → Coax S
    Amp2 (TPA3116D2 stereo, bass+treble knobs):
      L → [2×220µF NP HP XO ~363Hz] → Coax E
      R → [2×220µF NP HP XO ~363Hz] → Coax W
    Sub Amp (TPA3116 mono, built-in LP XO knob ~400Hz):
      → Sub (no external crossover, LP is inside amp board)

  CROSSOVERS:
    Sub:      LP @ ~400Hz — built-in adjustable knob on sub amp board
    Coaxials: HP @ ~363Hz — 2×220µF back-to-back (≈110µF NP) per driver, passive after amp
    Internal: each coaxial's built-in tweeter crossover @ ~3–5kHz
```

**BT always bypasses the 4PDT switch.** Signal goes: MH-M18 → AD828 preamp (5× gain, +14dB) → 8.2kΩ resistors → JFET Drain nodes directly. BT is stereo in both QUAD and STEREO modes. Plugging in jacks adds signal on top — does not cut BT.

**Sub summing taps post-JFET** so the sub tracks the master volume knob automatically. All 4 Drain node outputs feed through 8.2kΩ each to one mono node, which drives the sub amp.

**Resistors throughout are 8.2kΩ** (substituted for 10kΩ spec — works fine).

**Frequency control:** Bass/treble knobs on each TPA3116 board provide analog EQ. Sub bass knob adjusts low-end independently. Combined with the passive crossover, this gives full frequency range control without active crossovers.

---

## Parts List

### Drivers

| Part | Qty | Notes | Link |
|------|-----|-------|------|
| BLACK DIAMOND DIA-35.2 3.5" Coaxial | 4 (2 pairs) | 4Ω, 30W RMS, 2-way, paper cone + mylar tweeter | [Amazon](https://www.amazon.com/BLACK-DIAMOND-Coaxial-Speaker-Pair/dp/B09KM99B1W) |
| Dayton Audio RS100-4 (4" woofer) | 1 | 4Ω, 30W, sealed sub — base-mounted, fires down | [Amazon](https://a.co/d/0gEn163K) |

**Coaxial selection criteria (if substituting):** 3.5" 2-way, 4Ω impedance, ≥85dB sensitivity, ≤150g per driver, 87-92mm frame OD. Alternatives: BOSS CH3220B, Pyle PL31BK, or search "3.5 inch coaxial car speaker 4 ohm."

### Amplifiers

| Part | Qty | Notes | Link |
|------|-----|-------|------|
| Acxico TPA3116D2 2x80W stereo w/ bass+treble knobs | 2 | Coaxial amps. DC 12-24V. At 11.1V into 4Ω: ~12-14W/ch clean. Bass + treble knobs for dialing in tone | [Amazon](https://www.amazon.com/Acxico-TPA3116D2-Digital-Amplifier-Regulating/dp/B082PJ8LSY) |
| TPA3116 100W mono sub amp w/ built-in LP crossover | 1 | Sub amp. DC 12-24V. Has adjustable bass crossover frequency + bass volume knobs. Built-in LP filter eliminates need for external crossover inductor | [Amazon](https://www.amazon.com/TPA3116-Subwoofer-Amplifier-Preamplification-Crossover/dp/B0DBVBFCNX) |

No separate tweeter amp needed. Coaxials handle their own tweeter internally. Sub amp has built-in low-pass crossover — set the frequency knob to ~400Hz to match the coaxial HP crossover.

**Power delivery at 3S (11.1V nominal):**

```
Per coaxial channel:  P = V² / (2 × R) × η = (11.1²) / (2 × 4) × 0.9 ≈ 13.9W
Sub (bridged mono):   P ≈ 2 × 13.9 ≈ 27.8W (limited by knob to ~15-20W typical)
Total system peak:    4 × 13.9 + 20 ≈ 76W available
```

**Note:** These boards are rated for 12-24V but the TPA3116D2 chip operates from 4.5-26V. At 3S nominal (11.1V) and even at low charge (9V), the chip runs fine — just with proportionally less max power.

### Bluetooth

| Part | Qty | Notes | Link |
|------|-----|-------|------|
| MH-M18 Bluetooth 4.2 audio receiver | 1 | Chime on connect/disconnect (no voice prompts), stereo line-level out, auto-reconnect, 20m range, 3.7-5V | [Amazon](https://www.amazon.com/HiLetgo-Wireless-Bluetooth-Receiver-Lossless/dp/B07W4PJ469) |
| HiLetgo MP1584EN 3A Mini DC-DC Buck Module (5-pack) | 1 | Steps 3S battery (9-12.6V) down to 5V for BT module and pot rail. Adjustable output — set to 5.00V with multimeter BEFORE connecting anything downstream. Has built-in overcurrent protection + thermal shutdown. **Do NOT use AMS1117 here** — AMS1117 is a linear regulator, drops 6V as heat at this input voltage, dies on any short, has no real overcurrent protection. Burnt out 5 AMS1117s and 2 BT modules before switching. MP1584EN is a switching buck converter, runs cool, survives abuse | [Amazon](https://a.co/d/00x9Qr1B) |
| AD828 Stereo Preamp Board w/ volume control | 1 | Boosts MH-M18 output (~200-400mV) to line level (~1-2V) needed to drive TPA3116 to full power. Default gain ~5x (14dB). Runs on 3.8-15V — power from AMS1117 5V rail (linear, clean). DO NOT power from main 11.1V bus (switching noise). Wire: BT L/R → AD828 in, AD828 out → 8.2kΩ mixing resistors → amp inputs | [Amazon](https://a.co/d/0h0UNYgZ) |

BT module powered from regulated 5V rail (AMS1117). Audio L/R → AD828 preamp (also on 5V rail) → 8.2kΩ mixing resistors → amp inputs. The AD828 preamp is required because MH-M18 outputs ~200-400mV but TPA3116 needs ~1V to reach full power. BT is always on — appears as "MH-M18" on power-up, auto-reconnects to last paired device.

### Battery & Power

| Part | Qty | Notes | Link |
|------|-----|-------|------|
| Samsung 30Q 18650 cells (3000mAh) | 3 | 3S1P: 11.1V, 3000mAh, 33.3Wh. Buy 4-pack, keep 1 spare | [Amazon](https://a.co/d/0eLTxoHt) |
| 3S 18650 BMS w/ balance + protection | 1 | 12.6V, overcharge/overdischarge/overcurrent/short protection | [Amazon](https://www.amazon.com/AEDIKO-Lithium-Protection-Over-Discharge-Over-Current/dp/B09MLXFH81) |
| USB-C PD trigger board (15V fixed) | 1 | Requests 15V from any USB-C PD charger (MacBook brick, phone fast charger, PD car charger) | [Amazon](https://www.amazon.com/JacobsParts-Voltage-Trigger-Module-Type-C/dp/B08NFKV2LD) |
| DPS3003 Digital CC/CV Buck Module | 1 | Set to 12.6V / 1.9A via buttons on digital display. Steps 15V PD down to 12.6V for 3S charging. **Replaced XL4015** — XL4015 had blind trim pots impossible to calibrate without a multimeter, pot wipers get damaged from excessive adjustment, cannot verify voltage/current visually. DPS3003 shows exact voltage and current on display, set with buttons, no multimeter needed. | Search "DPS3003 buck converter" |

**Charging circuit:**

```
USB-C PD wall brick (dedicated charger — not a laptop/hub port)
  → PD trigger board (negotiates 15V from charger)
  → DPS3003 CC/CV module (set to 12.6V / 1.5A via digital display)
  → 1N5822 Schottky diode (stripe toward BMS — blocks back-current)
  → 3S BMS (handles cell balancing + protection)
  → 3x 18650 cells

Charge time: ~2 hours at 1.5A
```

**DPS3003 settings:**
- Voltage: **12.6V**
- Current: **1.5A** (0.5C for 3000mAh — sweet spot for cell longevity and converter stability)
- S-INI: **ON** — output enables automatically when charger is plugged in, no button press needed (hold SET to enter settings menu, scroll to S-INI, set to ON)

**Charging behavior to expect:**
- CC phase: current holds steady at 1.5A while voltage climbs to 12.6V
- CV phase: voltage holds at 12.6V, current tapers down automatically
- A faint high-pitch whine during CV taper (current below ~0.5A) is normal — it's the DPS3003 buck converter entering pulse-skip mode at light load, not a problem
- Battery is done when current drops to ~0.1–0.15A
- DPS3003 display shows output voltage (always ~12.6V), not battery voltage — watch current, not voltage, to track charge progress

**BMS lockout on deeply discharged pack:** If the pack sat discharged long enough for cells to drop below ~2.5V each, the BMS cuts output entirely and the DPS3003 will flash OEP and restart in a loop. Fix: drop DPS3003 current to 0.1A and let it trickle until the BMS unlatches, then raise back to 1.5A. Once the BMS resets it charges normally.

**Why DPS3003 over XL4015:** The XL4015 used blind trim pots — no display, no way to verify voltage or current without a multimeter. Pot wipers are fragile and get damaged from excessive turning. Lost calibration = unknown output voltage = cells won't charge. One bad session with no multimeter can render it completely unusable. DPS3003 has a digital display showing exact voltage and current in real time. Set with buttons. No tools needed. Worth the extra $5.

**Why no 2Ω resistor anymore:** The resistor was a workaround for the XL4015's slow CC response — dead batteries looked like a short and the XL4015 would hit its internal overcurrent protection before the CC loop responded. DPS3003 has proper, fast CC control that limits to exactly 1.5A from the first millisecond. No inrush spike, no protection trip, resistor not needed.

**Why keep the 1N5822 diode:** Prevents battery from back-feeding through the DPS3003 when the charger is unplugged. Cheap insurance, always keep it.

**Charger source — dedicated wall brick only:** Use a dedicated USB-C PD wall brick, not a USB-C port on a laptop or powered hub. Laptop ports share available power with the host device and can't reliably sustain the full 15V draw at 1.5A. When input power falls short, the DPS3003 flashes OEP and restarts in a loop. A dedicated wall brick has no competing load and delivers stable 15V. Any USB-C PD fast charger works — phone bricks, laptop bricks, car PD chargers. Does NOT work from basic 5V-only USB ports.

**Why 3S over 2S:**

```
2S (7.4V) into 4Ω:  ~6W/ch — underpowered, driver potential wasted
3S (11.1V) into 4Ω: ~14W/ch — matches 30W RMS driver rating, 90%+ amp efficiency
4S (14.8V) into 4Ω: ~25W/ch — overkill, exceeds small coaxial comfort zone
```

3S is the efficiency sweet spot: enough voltage to drive the TPA3116 properly without exceeding driver power handling. Class D efficiency is highest when supply voltage closely matches needed output swing.

### Input / Switching / Controls

| Part | Qty | Notes | Link |
|------|-----|-------|------|
| 3.5mm TRS jack (panel-mount) | 2 | Input jacks | [Amazon](https://a.co/d/098v7rsj) |
| mxuteuk MTS-402 4PDT mini toggle switch (ON/ON) | 1 (2-pack) | Quad/stereo mode toggle. 12 terminal, 2 position, 5A 125V | [Amazon](https://a.co/d/0cyumxV9) |
| Rocker switch (on/off) | 1 | Main power | [Amazon](https://a.co/d/0jhcTguL) |
| 10kΩ A-taper pot | 1 | Master volume knob — search "A10K potentiometer" | Search "A10K potentiometer" |
| J201 JFET transistor | 4 (buy 20-pack) | Master volume circuit, one per channel | [Amazon](https://a.co/d/0hYvX7N8) |

### Passive Components (Crossovers + Summing + Noise Suppression)

| Part | Qty | Notes | Link |
|------|-----|-------|------|
| 220µF polarized electrolytic cap | 8 | Crossover caps — 2 per coaxial wired back-to-back | Already have (BEEYUIHF kit) |
| 8.2kΩ 1/4W resistor | 16 | 4× BT mix + 4× stereo sum + 4× sub sum + 4× JFET signal series — 8.2kΩ substituted for 10kΩ, works fine | From kit |
| 1MΩ 1/4W resistor | 4 | JFET gate resistors (one per channel) | From kit |
| 10Ω 1W resistor | 1 | Main power filter — must be 1W rated | From kit |
| 100nF ceramic cap assortment | 1 | HF decoupling | [Amazon](https://a.co/d/00Ju4G9m) |
| Electrolytic cap assortment 1µF–1000µF | 1 | Power decoupling | [Amazon](https://www.amazon.com/BOJACK-Electrolytic-Capacitor-Assortment-0-1uF-1000uF/dp/B07PBQXQNQ) |
| Axial ferrite bead 3.5×6×0.8mm (RH3.5X6X0.8) | 4 (100-pack) | Amp power rail noise suppression — slip onto positive power wire to each amp board | [Amazon](https://a.co/d/0b1OZaW4) |
| 5W 2Ω wirewound cement resistor | 1 (10-pack) | Charging inrush limiter — in series on XL4015 output, prevents overcurrent trip on dead batteries | Search "5W 2R wirewound resistor" |
| 1N5822 Schottky diode | 1 (20-pack) | Blocks back-current from battery into XL4015 when not charging — stripe toward BMS | Search "1N5822 Schottky diode" |

### Enclosure & Hardware

| Part | Qty | Notes | Link |
|------|-----|-------|------|
| IEMAI Crystal Transparent Smoke Gray PETG 1.75mm 1kg | ~1.4 spools | 8mm solid walls, ~1.3kg shell. Print dome-up, base on bed. Translucent smoke gray — internals partly visible, plan wire routing accordingly | [Amazon](https://a.co/d/01U2GBmH) |
| Rubber feet (adhesive, 15-20mm tall) | 4 | Lifts base for sub airflow — taller than typical to avoid chuffing | [Amazon](https://a.co/d/05Hw71ch) |
| Speaker grille mesh | as needed | Steel or 3D printed covers for coaxial cutouts | [Amazon](https://www.amazon.com/uxcell-Speaker-Decorative-Subwoofer-Circle/dp/B07GZVKF4T) |
| Closed-cell foam gasket tape (self-adhesive) | 1 roll | Seals driver frames to enclosure surface — apply to driver frame before screwing down, compresses airtight | [Amazon](https://a.co/d/04kdLdgT) |
| Hot glue sticks | 1 pack | Seal all interior seams, wire pass-throughs into sub chamber, around driver edges | Already have or any hardware store |
| Screw + nut assortment kit | 1 pack | Mount drivers to enclosure — used with threaded heat inserts (below). **TODO: measure driver mounting holes to confirm correct size** | [Amazon](https://a.co/d/0cWvuDzq) |
| Threaded heat inserts | 1 pack | Heat-set into PETG enclosure walls for clean, reusable threaded mounting points — drivers, baffle, panel-mount components. Install with soldering iron at ~200°C | [Amazon](https://a.co/d/0a97q7Gx) |
| XT30 battery connector (male + female pair) | 1 pack | Quick-disconnect between battery pack and BMS — allows battery removal for air travel | [Amazon](https://a.co/d/0iCo65bV) |
| Battery swap accessory | 1 | For hot-swappable battery pack | [Amazon](https://a.co/d/0isvuEXH) |
| Silicone sealant (clear) | 1 tube | Backup seal for enclosure halves join seam and any gaps hot glue can't reach | Search "clear silicone sealant" |
| Polyfill stuffing | 1 bag | Acoustic damping for sub chamber — loosely fill ~50-70% of the 1.29L sub volume. Tricks the sub into "seeing" a larger box, lowers effective f3, reduces standing waves | [Amazon](https://a.co/d/08qdWjJ9) |

### Wire

| Part | Use | Wire type |
|------|-----|-----------|
| Battery → BMS → rocker switch → power bus | Power | 18 AWG |
| Power bus → each amp board | Power | 18 AWG |
| XL4015 → BMS charge input | Power | 18 AWG |
| Amp outputs → crossover caps → drivers | Speaker | 18 AWG |
| Amp3 → sub driver | Speaker | 18 AWG |
| Jacks → 4PDT switch | Signal | Arduino wire or thin |
| 4PDT switch → JFET volume stage | Signal | Arduino wire or thin |
| JFET stage → amp inputs | Signal | Arduino wire or thin |
| BT module L/R → summing network | Signal | Arduino wire or thin |
| Pot wiper → JFET gates | Signal | Arduino wire or thin |

**Wire lengths — cut to exact point-to-point distance + 3cm slack for resoldering. Do not leave excess coiled up.**

| Run | Approximate length |
|-----|--------------------|
| Battery → BMS | 8cm |
| BMS → rocker switch | 10cm |
| Rocker switch → power bus | 8cm |
| Power bus → each amp board | 8–10cm |
| AMS1117 → BT module VCC | 6cm |
| XL4015 → BMS charge | 8cm |
| Amp1/2 → coaxial drivers (N/S/E/W) | 18–22cm (must reach up dome) |
| Amp3 → sub driver | 10cm |
| Jacks → 4PDT switch | 6cm |
| Switch → JFET stage | 6cm |
| JFET stage → amp inputs | 6–8cm |
| BT → summing network | 6cm |
| Pot wiper → JFET gates | 8cm |

**Routing rules — keeps noise out without shielded cable:**
- Power wires and signal wires on opposite sides of the enclosure interior — never bundled together
- Where they must cross, cross at 90° — never run parallel
- Keep signal wires away from amp board edges specifically (biggest noise source)
- 2–5cm separation minimum when running alongside each other
- No coiled slack — a coil acts as an inductor and picks up interference

### Consumables (likely already have)

| Part | Notes |
|------|-------|
| Standoffs + screws (M3) | Mount amp boards off shell walls for cooling |
| Nickel strips + kapton tape | Battery pack assembly (or buy pre-tabbed cells) |
| Solder, heat shrink, zip ties | Standard build supplies |

---

## Crossover Values

### Sub: Active LP — Built Into Sub Amp Board

The sub amp board has a **built-in adjustable low-pass crossover** with a frequency knob. Set it to ~400Hz. This is an active crossover (filters the signal before amplification), which is more efficient and cleaner than a passive inductor after the amp.

No external crossover components needed for the sub. Just turn the knob.

### Coaxials: 1st Order Passive HP @ 400Hz (4Ω) — build 4

Target: 100µF non-polarized in series between each amp output and coaxial driver.

```
C = 1 / (2π × f × R) = 1 / (2π × 400 × 4) = 1 / 10053 = 99.5µF → 100µF standard
```

**Making non-polarized caps from standard polarized caps (back-to-back method):**

A polarized electrolytic cap will fail if audio signal reverses its polarity — which AC audio always does. Wiring two polarized caps back-to-back cancels their polarity restriction, creating a non-polarized equivalent at half the capacitance.

```
Use: 2× 220µF polarized per coaxial → ~110µF non-polarized (close enough to 100µF)

Wiring (one crossover unit):

  amp output ──┤+ 220µF –├──┤– 220µF +├── coaxial driver +
               cap A          cap B

Connect: cap A negative terminal → cap B negative terminal (neg-to-neg)
Cap A positive → amp output
Cap B positive → driver +
```

Step by step for each of the 4 crossovers:
1. Take 2× 220µF caps from BEEYUIHF kit. Mark the negative leg (shorter leg, stripe on body).
2. Twist the two negative legs together and solder them to each other. Insulate with heat shrink.
3. The remaining free positive leg of cap A goes to the amp output wire.
4. The remaining free positive leg of cap B goes to the coaxial driver + terminal.
5. Driver – terminal goes directly to amp ground output. No cap on the negative wire.
6. Repeat for all 4 coaxials — 8 caps total.

**Why this works:** Each cap blocks DC in one direction. Back-to-back, one cap always has correct polarity no matter which way the AC swings. The series combination halves the capacitance (220µF ÷ 2 = 110µF) which is close enough to the 100µF target — crossover frequency shifts from 400Hz to ~363Hz, inaudible difference.

**Total crossover parts: 8× 220µF from your BEEYUIHF kit.** No separate purchase needed.

This keeps deep bass out of the coaxials and lets the sub handle everything below ~400Hz. The 1st order slope (6dB/octave) is gentle — the coaxials still get some bass, which is fine since car coaxials are designed to handle full-range. Use the coaxial amp's bass knob to fine-tune overlap with the sub.

Each coaxial's internal crossover (~3-5kHz) handles the woofer-to-tweeter transition within the driver. No external tweeter crossover needed.

---

## Quad / Stereo Switching

4PDT latching button wiring:

```
QUAD (button out):
  Jack A tip (L)  --> Amp1 input L
  Jack A ring (R) --> Amp1 input R
  Jack B tip (L)  --> Amp2 input L
  Jack B ring (R) --> Amp2 input R

STEREO (button in):
  Jack A tip + Jack B tip  --> 10k+10k sum --> Amp1 L AND Amp2 L
  Jack A ring + Jack B ring --> 10k+10k sum --> Amp1 R AND Amp2 R
```

**BT always stereo — bypasses the switch entirely:**

```
BT L --> 10k --> Amp1 L input
BT L --> 10k --> Amp2 L input
BT R --> 10k --> Amp1 R input
BT R --> 10k --> Amp2 R input
```

BT stays stereo in both quad and stereo modes. Plugging in jacks adds signal on top — doesn't cut BT. Everything plays together.

**Sub summing:** All 4 amp input signals (post-switch, post-BT-mix) summed through 4x 10k resistors to one node → feeds Amp3 (sub) input.

**Driver pairing (quad mode):** Opposite pairs for even spatial coverage:

```
Amp1 L → North coaxial    Amp1 R → South coaxial
Amp2 L → East coaxial     Amp2 R → West coaxial
```

---

## JFET Master Volume

One pot, one knob. The pot wiper generates a single control voltage that connects to all 4 JFET gates simultaneously. Each JFET sits in series with one signal channel and acts as a voltage-controlled resistor — turn the pot and all channels attenuate together. Channels stay electrically separate, nothing merges.

### Circuit

```
  [10kΩ pot]
  CCW → GND
  CW  → +5V (AMS1117 rail)
  wiper → all 4 JFET gates via individual 1MΩ resistors

  Per channel (×4):

  signal in ──[10kΩ]──┬──── signal out → amp input
                      │
                  [J201 D→S]
                      │
                     GND

  J201 Gate ←──[1MΩ]──── pot wiper (same wiper, all 4 channels)
  J201 Source → GND
```

Sub amp input is summed from the 4 post-JFET nodes — sub automatically tracks the master volume knob.

### J201 Pin Order

Flat side of J201 facing you, legs down: **Gate — Source — Drain** (left to right).

### Parts

| Part | Qty |
|------|-----|
| 10kΩ A-taper pot | 1 |
| J201 JFET | 4 |
| 8.2kΩ 1/4W resistor | 4 (from kit — 10kΩ substituted with 8.2kΩ, works fine) |
| 1MΩ 1/4W resistor | 4 (from kit) |

### Build Style
Built dead-bug (no perfboard) — components float and are held by solder joints. Hot glue over the whole assembly once tested to lock in place mechanically.

### Placement in Signal Chain

```
jacks/BT → [4PDT switch] → [JFET volume stage] → amp inputs → amps → speakers
                                     ↑
                               [1 pot, 1 knob]
```

---

## Ground Noise Suppression

- **Star ground**: Separate ground wire from each amp + BT regulator back to one point at battery negative. No daisy-chain.
- **Ferrite beads**: One inline on each amp positive power wire.
- **Decoupling**: 100µF electrolytic + 100nF ceramic cap pair at each amp power input.
- **Main power filter**: 10Ω resistor + 1000µF cap after power switch.
- **Signal wires**: Keep power wires away from audio wires. Use shielded wire for input jack runs.
- **5V rail**: AMS1117 LDO provides clean regulated 5V to BT module — isolated from amp switching noise.

---

## Wiring Guide

### Rules Before You Start

- **Power wires first, signal wires last.** Complete all power runs, test voltage at each point, then run signal wires. Easier to troubleshoot and keeps signal wires away from work-in-progress power connections.
- **Cut wire to exact point-to-point length + 3cm slack.** No extra coiled wire. A coil acts as an inductor and picks up interference.
- **Power wires one side of enclosure, signal wires the other.** Never bundle them together.
- **Where power and signal must cross, cross at 90°.** Never run parallel.
- **Star ground.** Every ground wire runs back to one point at battery negative. Never daisy-chain grounds from board to board.
- **Tin your wires and pads before joining.** Pre-tin both sides, then touch together with iron briefly. Cold joints cause noise and failures.
- **Heat shrink everything** that could touch another wire or pad.

### Order of Operations

**Stage 1 — Power rails (18 AWG)**
1. Solder battery pack in 3S series. Wrap in kapton tape. Confirm 11.1V across pack.
2. BMS: B+ to pack positive, B- to pack negative, balance taps to cell junctions.
3. BMS P+ → ferrite bead → rocker switch → 10Ω 1W resistor → power bus (solder joint or bus wire). Place 1000µF cap from power bus to star ground point.
4. BMS P- → star ground point.
5. Power bus → Amp1 positive (with ferrite bead inline, 100µF + 100nF cap at amp terminal) — ~8cm
6. Power bus → Amp2 positive (same treatment) — ~8cm
7. Power bus → Amp3 (sub amp) positive (same treatment) — ~8cm
8. Power bus → AMS1117 input — ~6cm
9. AMS1117 output → BT module VCC — ~6cm
10. Each amp ground → individual wire → star ground point. Do not share.
11. AMS1117 ground → star ground point.
12. **Test:** power on, confirm ~11.1V at each amp power terminal, ~5V at BT module VCC. Power off.

**Stage 2 — Charging circuit (18 AWG)**
1. USB-C PD trigger board USB-C port → panel mount hole in enclosure wall.
2. PD board output → DPS3003 input — ~8cm
3. **Before connecting output:** power on DPS3003 and set voltage to **12.6V** and current to **1.9A** using the buttons. Confirm on the display. No multimeter needed.
4. DPS3003 output+ → 1N5822 Schottky diode (stripe/cathode facing toward BMS) → BMS charge input+ — ~8cm total
5. DPS3003 ground → star ground point.
6. **Test:** plug in USB-C PD charger, confirm DPS3003 display reads 12.6V and current starts flowing. No 2Ω resistor needed — DPS3003 CC handles inrush.

**Stage 3 — JFET master volume stage (Arduino wire, dead-bug style)**
Built dead-bug — no perfboard, components float held by solder joints. Hot glue over assembly when done.

J201 pin order — flat side down on table, legs down, looking at rounded back: **Drain — Source — Gate** (left to right)

1. Place 4× J201 JFETs. Flat side all facing same direction.
2. All 4 Source legs (middle) → wire to star ground.
3. Per channel: signal input wire → 8.2kΩ resistor → Drain (left leg). Signal output wire also from Drain — both connect at the same Drain junction point.
4. Per channel: Gate leg (right) → 1MΩ resistor → pot wiper node.
5. All 4 × 1MΩ resistors meet at pot wiper node — run one wire to pot wiper.
6. Pot CCW terminal → ground. Pot CW terminal → AMS1117 5V rail.
7. Mount pot on enclosure wall where knob is accessible.
8. **Test with multimeter:** wiper should sweep 0–5V as you turn pot. No power needed yet.

**Stage 4 — Signal wiring (Arduino wire)**
Keep all signal wires on the opposite side of the enclosure from power wires.

Signal chain order: Jacks → 4PDT switch → [BT sums in here] → JFET Drain junction → amp inputs. Sub summing taps off the JFET Drain junctions after attenuation.

1. Jack A tip (L) and ring (R) → 4PDT switch middle pins (rows 1+2) — ~6cm
2. Jack B tip (L) and ring (R) → 4PDT switch middle pins (rows 3+4) — ~6cm
3. 4PDT switch — quad position outer pins: each row straight to its own JFET Drain junction
4. 4PDT switch — stereo position outer pins: bridge row 1 outer to row 3 outer (L merge), row 2 outer to row 4 outer (R merge)
5. BT module L → 8.2kΩ → Ch1 Drain junction AND 8.2kΩ → Ch3 Drain junction — ~6cm each
6. BT module R → 8.2kΩ → Ch2 Drain junction AND 8.2kΩ → Ch4 Drain junction — ~6cm each
7. Each Drain junction also has 8.2kΩ series resistor from jack/switch signal coming in
8. JFET outputs (Drain junctions) → amp inputs: Ch1/Ch2 → Amp1 L/R, Ch3/Ch4 → Amp2 L/R — ~6cm
9. Sub summing: each of 4 Drain junctions → 8.2kΩ → one node → sub amp L+R tied together. Sub amp GND → star ground.
10. All signal ground returns → star ground.
11. Jack sleeve (ground) pins → star ground directly, no resistors.
12. **Test:** power on, connect phone via BT, turn pot to max. You should hear audio from amp board headphone test point or see the board's level LED react. Power off.

**Stage 5 — Crossover assemblies (18 AWG)**
Build all 4 cap assemblies before running any speaker wire.
1. Build 4× back-to-back 220µF assemblies (see Crossover Values section).
2. Amp1 L output → cap assembly → 18 AWG → North coaxial + terminal — ~20cm
3. Amp1 R output → cap assembly → 18 AWG → South coaxial + terminal — ~20cm
4. Amp2 L output → cap assembly → 18 AWG → East coaxial + terminal — ~20cm
5. Amp2 R output → cap assembly → 18 AWG → West coaxial + terminal — ~20cm
6. Amp3 output → 18 AWG → sub driver + terminal — ~10cm
7. All driver – terminals → 18 AWG → star ground — run individual wires.
8. **Check polarity on every driver before final mount** — wrong polarity on one driver cancels bass and sounds thin.

**Stage 6 — Final test before sealing**
1. Power on. Connect BT. Play music at low volume.
2. Confirm all 4 coaxials produce sound. Confirm sub produces bass.
3. Toggle quad/stereo button — stereo mode should sound fuller/more blended.
4. Sweep master volume pot from min to max — all channels should rise and fall together smoothly.
5. Set sub amp LP crossover knob to ~400Hz.
6. Listen at moderate volume for 5 minutes. No buzz, hiss, or heat = good to seal.

---

## Assembly

### 1. Battery Pack

1. 3x 18650 in series (3S1P). Spot-weld nickel strips or solder carefully.
2. Attach 3S BMS: B+ to pack positive, B- to pack negative, B1/B2 to cell junctions for balance.
3. Wrap in kapton tape. Should read 9.0-12.6V across P+/P-.

### 2. Power & Charging Circuit

1. BMS P+ → rocker switch → 10Ω resistor → power bus. 1000µF cap from bus to ground.
2. BMS P- → ground bus.
3. USB-C PD trigger board (set to 15V) → XL4015 CC/CV module (set output to 12.6V, current limit to 2A) → BMS charge input pads.
4. AMS1117-5V module: input from power bus, output to BT module VCC.
5. Panel-mount the PD trigger board's USB-C connector on enclosure wall.

### 3. Amps + BT

1. Mount 3 amp boards + BT module + AMS1117 in base area on M3 standoffs (air gap underneath for cooling).
2. Each amp gets: ferrite bead inline on power + 100µF + 100nF caps at power terminals.
3. Star ground all back to battery ground point.
4. BT module audio L/R → 10k summing resistors → amp inputs (see signal flow diagram).

### 4. Input Wiring

1. Mount jacks + 4PDT latching button on base panel.
2. Wire per quad/stereo diagram. 10k summing resistors on stereo sum lines.
3. BT L/R mixed in at amp inputs via 10k resistors — bypasses the switch.
4. Sum all 4 post-switch signals through 10k resistors → Amp3 (sub) input.

### 5. Crossovers

1. Build 4× back-to-back cap assemblies first (see Crossover Values section for full wiring diagram).
2. Each assembly: 2× 220µF polarized caps, negative legs twisted and soldered together, heat-shrunk. Inline on wire, no perfboard needed.
3. Wire Amp1 L output → cap assembly → North coaxial +
4. Wire Amp1 R output → cap assembly → South coaxial +
5. Wire Amp2 L output → cap assembly → East coaxial +
6. Wire Amp2 R output → cap assembly → West coaxial +
7. Sub: no external crossover — set sub amp board's built-in LP crossover knob to ~400Hz.
8. Wire Amp3 output → sub driver directly (crossover is inside the amp board).

### 6. Mount Drivers

1. **Sub (4")**: Center of flat base, firing down. Gasket tape seal. Rubber feet (~15-20mm) lift base for airflow — floor acts as boundary reinforcement for bass.
2. **Coaxials (3.5" x4)**: N/E/S/W at 68.7mm above base, 97.1mm from center axis. Printed flat mounting pads at each position. Gasket tape seal.
3. **Internal baffle**: 212mm diameter disc at 65mm height. Seals sub chamber (~1.29L) below. Coaxials share ~2.24L open air space above. Friction fit or screw mount.

### 7. Print Enclosure

1. Print as two halves for assembly access — lower section (base to ~65mm) and upper dome (65mm to apex). Join with friction fit, screws, or adhesive.
2. Print dome-up (flat base on print bed). No supports needed for either half.
3. 8mm solid walls, 100% infill perimeters for acoustic damping.
4. 4x coaxial cutouts at CCRMA positions in upper dome (see geometry section for coordinates).
5. 1x sub cutout centered in base of lower section.
6. Holes in lower section sidewall for: 2x jacks, 1x 4PDT button, 1x rocker switch, 1x USB-C.
7. Internal baffle printed as separate piece (212mm disc) — mounts at the join between the two halves.
8. Estimated print time: 30-50 hours total depending on speed settings.

### 8. Final

1. Connect all amp outputs → crossover components → drivers. Check all polarities (+/+).
2. Tuck wires, zip tie. Keep power wires physically separated from signal wires.
3. Battery in base area with foam padding or printed cradle.
4. Seal hemisphere halves. Power on. BT should appear as "MH-M18" immediately.
5. Test: BT connect (should hear chime through speakers), play music, test quad/stereo switch, test both jacks, verify sub bass knob and coaxial bass/treble knobs work.

---

## Heat

TPA3116 is class D — switches instead of dissipating power as heat.

```
At 12W output per channel: ~1-1.5W lost as heat per channel
3 amp boards at loud volume: ~8-12W total heat dissipation
At moderate volume (~5W/ch): ~3-5W total heat (less than a phone charger)
```

PETG softens at ~80°C — amps stay well below that in normal use (~40-50°C board temp). PETG is the right choice over PLA here for the heat margin alone. Mount boards on standoffs with air gap. Driver cutouts provide passive airflow. Sub firing downward creates slight convection draft through base gap.

No overheating risk in normal use.

---

## Battery Life (3S1P — 33.3Wh)

| Usage | System Draw | Runtime |
|-------|-------------|---------|
| Moderate (background/conversation) | ~5-7W | 4.5-6 hrs |
| Normal (music at room level) | ~10-12W | 2.5-3.5 hrs |
| Loud (pushing it) | ~18-22W | 1.5-2 hrs |

**Road trip use:** Charge while driving with a USB-C PD car charger (~$15, Anker or similar — plugs into cigarette lighter, outputs USB-C PD). Same protocol as MacBook charger, charges at full speed.

**Extended play:** Upgrade to 3S2P (6 cells, 66.6Wh) doubles all runtimes at +138g weight. Same BMS works, just wire two parallel pairs then series.

---

## Weight Budget

| Component | Weight |
|-----------|--------|
| 3x Samsung 30Q (3S1P) | 138g |
| 1x RS100-4 sub | 390g |
| 4x 3.5" coaxials (~130g ea) | ~520g |
| 2x TPA3116 stereo boards | ~80g |
| 1x TPA3116 mono board | ~40g |
| BT module + AMS1117 | ~20g |
| BMS + PD trigger + XL4015 | ~40g |
| Crossover components (4 caps) | ~30g |
| Wiring + hardware + standoffs | ~60g |
| Enclosure (PLA, 8mm solid walls) | ~1300g |
| **Total** | **~2.62 kg (5.8 lbs)** |

Comparable to JBL Xtreme 3 (1.97kg) — heavier due to 8mm solid walls and 5 drivers, but dramatically more sound output.

---

## Performance vs. JBL

### SPL (Loudness)

```
Each 3.5" coaxial: ~87 dB/W/m sensitivity (typical)
At 12W drive:      87 + 10 × log10(12) = 87 + 10.8 = 97.8 dB per driver (on-axis at 1m)
4 drivers (different directions, power addition): +6 dB room energy
Sub contribution in bass: +3-5 dB below 400Hz
Effective room SPL at loud volume: 92-98 dB
```

### Comparison

| Spec | Quadro Punch Packer | JBL Charge 5 (~$180) | JBL Xtreme 3 (~$350) |
|------|--------------------|-----------------------|----------------------|
| Drivers | 4x 3.5" coax + 4" sub | 1x racetrack + 2 passive radiators | 2x 70mm + 2x tweeters + 2 passive radiators |
| SPL (room level) | **92-98 dB** | 80-86 dB | 85-92 dB |
| Coverage | **360° horizontal** | ~120° forward | ~160° forward |
| Bass | ~80Hz (dedicated sub, boundary loaded) | ~65Hz (passive radiator) | ~55Hz (passive radiator) |
| Inputs | **2x 3.5mm + BT (simultaneous)** | BT only | 3.5mm + BT |
| Quad mode | **Yes (4 independent channels)** | No | No |
| EQ control | **Physical bass/treble knobs** | App EQ | App EQ |
| Waterproof | No | IP67 | IP67 |
| Battery | 33.3Wh (2.5-6 hrs) | 40Wh (~20 hrs) | 50Wh (~15 hrs) |
| Weight | 2.65 kg | 0.96 kg | 1.97 kg |

**The QPP is ~7-10 dB louder than a JBL Charge 5** — perceived as roughly twice as loud. 360° coverage fills a room more evenly than any forward-firing speaker. Dedicated sub provides real bass impact that passive-radiator designs can't match at this size. Competes with the $350 JBL Xtreme 3's output while costing ~$150 less.

**Trade-offs vs. JBL:** Heavier, shorter battery life, not waterproof, no app/smart features. But it's louder, wider coverage, has a real sub, quad input, physical EQ, and costs less.

---

## Cost

### Amazon Order Total

Most parts come in multipacks — you'll have spares.

| Category | Parts | Est. Cost |
|----------|-------|-----------|
| Coaxials (2 pairs BLACK DIAMOND DIA-35.2) | 4 drivers | ~$40-50 |
| Sub (Dayton RS100-4) | 1 driver | ~$18 |
| Acxico TPA3116D2 stereo w/ bass+treble | 2 boards | ~$20 |
| TPA3116 mono sub amp w/ built-in LP crossover | 1 board | ~$12 |
| MH-M18 BT receiver (3-pack) | 1 used + 2 spare | ~$9 |
| AMS1117-5V regulator (10-pack) | 1 used + 9 spare | ~$6 |
| Samsung 30Q 18650 (4-pack) | 3 used + 1 spare | ~$15 |
| 3S BMS w/ balance (6-pack) | 1 used + 5 spare | ~$7 |
| USB-C PD trigger 15V (2-pack) | 1 used + 1 spare | ~$8 |
| XL4015 CC/CV buck module (3-pack) | 1 used + 2 spare | ~$8 |
| 3.5mm jacks + 4PDT button + rocker switch | Input/controls | ~$17 |
| Non-polarized cap kit (crossover) | 4x 100µF used | ~$10 |
| Resistor kit + ceramic caps + electrolytic assortment + ferrite beads | Summing + decoupling + noise | ~$24 |
| IEMAI Smoke Gray PETG (~1.4 spools) | Enclosure | ~$35 |
| 18 AWG speaker/power wire (50ft) + shielded signal cable (6m) | Wire | ~$15 |
| Rubber feet + grille mesh + gasket tape | Hardware | ~$12 |
| **Amazon Total** | | **~$256-266** |

**Cost of parts actually used: ~$195-210.** The rest is spares from multipacks and extra wire.

---

## Build Session Log — 2026-04-29

### What Was Built

Full Blender geometry rebuild from scratch, replacing all prior v6/v7 attempts. Final result is a clean two-piece print-ready architecture under collection `QPP/v7_1/v7_1_print_ready/`:

- **QPP_v7_1_floor** — bottom piece, prints standing upright
- **QPP_v7_1_shell** — dome shell, prints upside-down

Both objects are watertight (0 non-manifold edges, 0 boundary edges) and ready for Bambu X1C with PETG 100% infill.

---

### QPP_v7_1_floor

**Dimensions:** z = −5 to +60 mm · **Mass:** ~876 g PETG · **Volume:** ~690 cm³

**Construction (bottom to top):**
- 4× rubber foot pads: Ø20mm cylinders, z = −5 to 0, PCD 95mm at 0°/90°/180°/270°
- Solid disc base: R = 124mm, z = 0–12mm (12mm thick, no gap)
- Sub tube wall: OD = 130mm, ID = 122mm, z = 12–60mm
- 3× internal bracing ribs at 120° spacing (30°/150°/270°), 15×4×35mm, spanning z = 15–50, PCD 53.5mm
- Sub tube cap: seals at z = 55–60mm → airtight sealed sub chamber z = 12–60
- Sub through-hole: Ø77mm centered, passes full 12mm of base disc for driver mounting
- 6× M3 sub mounting holes: Ø3.5mm, PCD 90mm at 60° intervals, z = 0–12
- Centering alignment ring: annulus r = 112–119mm, z = 12–16mm (4mm tall, 7mm wide) — dome slip-fits over this ring; min clearance to dome inner wall = 0.43mm

**Driver angle math preserved:** Sub fires straight up through Ø77 hole. Sub mounting PCD = 90mm accommodates Dayton RS100-4 bolt pattern. All coaxial math unaffected by floor changes.

---

### QPP_v7_1_shell

**Dimensions:** z = 12 to +124 mm · **Mass:** ~690 g PETG · **Volume:** ~543 cm³

**Construction:**

*Hemisphere shell:* Outer R = 124mm, inner R = 120.5mm (3.5mm wall), trimmed flat at z = 12 (no overhang into floor territory).

*Coaxial driver pads (×4):* Each coax sits at cube-vertex position d = (±1, ±1, +1)/√3, giving θ = arccos(1/√3) = 54.74° from vertical. Firing directions preserved exactly — all four drivers equidistant from listening position on the diagonal axes.

Pad profile is exponential, not flat: r(t) = 45 + 27·(e^(2t/T) − 1)/(e^2 − 1) where T = 26.01mm and t runs from pad base (axial distance 100.99mm from center) to pad top (127mm). This gives a smooth organic flair from Ø90mm base to Ø144mm top, 30 stacked frustum segments. Base starts at sphere surface; top ring stops flush with outer hull.

Driver opening is an elliptical cut: Ø75mm along the screw axis (u = Z × d), Ø79mm perpendicular — the larger dimension clears screw head material while the cone OD fits the tighter axis. Two M3 countersunk screw holes per driver at PCD 80mm along the screw axis direction.

*Panel cutouts:*
- Back panel (−Y face): USB-C 9.8×4.3mm rect, 2× Ø6.2mm jack holes, Ø8mm pushbutton
- Front panel (+Y face): Ø12mm rocker switch, Ø7mm pot hole

*Panel thinning:* Recessed zones at each panel face, sphere R = 122mm intersected with 60×30×45mm box — leaves enough wall for panel hardware to seat flush.

---

### Key Design Decisions This Session

| Decision | Reasoning |
|----------|-----------|
| Two-piece split at z = 12 | Keeps both pieces within Bambu 256mm build volume; floor prints upright, dome inverts |
| Centering ring r = 112–119, h = 4mm | Dome slip-fits without wobble; 0.43mm radial clearance ≥ PETG tolerance |
| Sub tube stops at z = 60 (cap), not z = 12 | Shorter tube frees ~30mm vertical interior space for electronics |
| Tube OD 130 / ID 122 (4mm wall) | Sub driver (Dayton RS100-4) OD = 122mm fits exactly; 4mm wall is structural at 100% infill |
| Exponential pad curve (30 segments) | Smooth flair vs. visible step rings; no additional non-manifold edges vs. flat frustums |
| No baffle piece | Sub cap z = 55–60 seals sub chamber alone; fewer parts, simpler assembly |
| Solid disc base z = 0–12 | Eliminates shelf/gap artifact visible in previous two-layer floor iterations |

---

### Errors Hit and Fixed

- **Boolean direction collapse:** Unioning all 4 coax pads simultaneously before shell union caused geometry to collapse to one octant. Fix: one coax at a time, `transform_apply` before each op.
- **Non-manifold on thin shell approach:** Building hollow shell first and carving coax pads into it produced 762 nm / 477 boundary edges. Fix: solid-first pipeline (build solid hemisphere → carve cavity → add features).
- **Coax pads hanging below z = 12:** Pad base axial = 100.99mm; at d direction that projected to z ≈ 2mm world space. Fix: final trim cube clips all material below z = 12.
- **Centering ring interference:** Ring outer r = 119.5 but dome inner at z = 16 = 119.43mm → 0.07mm negative clearance. Fix: trimmed ring to r = 119.0 giving 0.43mm clearance.
- **Floor gap (two-shelf artifact):** Old lower + connecting floor left visible 7mm empty band. Fix: replaced with single solid disc z = 0–12.
- **Dome disappearing:** Shell existed but had `hide_render = True` set from archiving. Fix: unset both hide flags.

---

### Outstanding / Next Steps

1. **Stepped driver cut** — current elliptical cut is Ø75×79 at full depth. Proper fit should be Ø79 for top ~8mm (clears speaker surround), narrowing to Ø75 below where cone tapers. Zero clearance in screw axis currently.
2. **Floor↔shell mechanical attachment** — centering ring is slip-fit only. Heat-set M3 inserts exist in floor at r = 98, z = 6–12 but dome has no corresponding clearance holes for through-bolts or captured hardware.
3. **Wire routing** — no channel or relief cut for signal/power cables between floor and dome yet.
4. **Print test** — first physical prototype not yet printed; all geometry is computational only.

---

## Build Session Log — 2026-05-01

### Progress
- Charging circuit fully working — new XL4015, 1N5822 diode in series, current pot dialed to ~1.9A, batteries charging confirmed
- Short found and fixed — + and - were touching in power bus screw terminal
- 1000µF cap added to power bus
- 4PDT switch wired and installed
- All signal chain wiring complete — jacks, BT (broken, needs replacement), switch, JFET stage, amp inputs, sub summing
- Audio routing confirmed partially working — panning works on 2 speakers proving jack ground correct

### Outstanding Issues
- **AMS1117 damaged** — outputting 8V instead of 5V from overheating earlier. Needs replacement from spare 10-pack. Causes pot to sweep wrong voltage range.
- **BT module dead** — overheated, needs replacement from spare pack
- **Volume pot not working** — left leg not grounded properly OR AMS1117 damage causing wrong voltage. Fix AMS1117 first then retest.
- **Two speakers always on regardless of input** — switch wiring suspect. When toggled, one speaker stays on and other turns off. Needs diagnosis — unplug jack completely and check if problem speakers go silent.
- **J201s possibly fried** — saw 8V on gates from bad AMS1117. Test after AMS1117 replaced.
- **Two J201s removed** — need replacement from 20-pack

### Next Session Starts Here
1. Replace AMS1117 — confirm 5V output before connecting anything
2. Replace BT module
3. Fix pot ground connection — confirm wiper sweeps 0-5V
4. Diagnose always-on speakers — unplug jack, check if silent, trace from there
5. Replace missing J201s
6. Test full audio with everything working

---

## Build Session Log — 2026-04-30

### Charging Circuit Issue Diagnosed

XL4015 current limit pot is defective — stuck, cannot be adjusted down from ~3A. At 3A into dead batteries, XL4015 hits overcurrent protection and blinks/restarts in a loop. Workaround during session: loose wire connection on XL4015 input terminal accidentally acted as current limiter, allowing slow charging. Batteries recovered from 9.9V to approximately 12V during session.

**Permanent fix ordered:**
- 5W 2Ω wirewound resistors — in series on XL4015 output, limits inrush current
- 1N5822 Schottky diodes — blocks back-current from battery into XL4015 when unplugged
- New XL4015 3-pack — replace defective board, set current to ~1A before use

**New charge circuit:**
```
XL4015 out → [1N5822 diode] → [2Ω 5W resistor] → BMS charge input
```

### JFET Master Volume Stage Built

Built dead-bug style (no perfboard). J201 pin order with flat side down, legs down, looking at rounded back: Drain — Source — Gate (left to right). 8.2kΩ substituted for 10kΩ throughout signal chain — works fine.

### Signal Chain Designed

Full signal chain confirmed:
- Jacks → 4PDT switch → Drain junction (with BT summing in at same node) → JFET attenuation → amp inputs
- Sub summing taps off post-JFET Drain junctions via 4× 8.2kΩ → sub amp L+R

### 4PDT Switch Ordered

Original latching pushbutton (2-pin) cannot do 4-pole switching. Replaced with mxuteuk MTS-402 mini toggle switch (ON/ON, 12 terminal, 4PDT).
