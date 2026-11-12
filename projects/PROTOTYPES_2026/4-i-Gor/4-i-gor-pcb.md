# 4 i Got — PCB Design Guide

**Project: EMGOR SYNTH | Date: 4/16/26**

---

## Design Overview

Hybrid modular motherboard PCB. Breakout boards plug into female headers. Passives, connectors, and jacks soldered directly to the PCB. 2-layer board, smallest footprint within reason.

**USB:** Teensy's built-in micro-USB handles power + audio + MIDI. No separate USB connector on the motherboard.

---

## Board Architecture

### PCB Type
- **2-layer** (top copper + bottom copper)
- Top layer: component placement, ground planes, some signal routing
- Bottom layer: power traces, remaining signal routing, decoupling cap placement

### Estimated Board Size
~120mm × 80mm (finalize after placing all headers in KiCad)

### Component Zones (top view)

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│   [ADC 1 header]        [TEENSY 4.0]       [ADC 2 header]│
│   (1×6 + 1×6 + 1×2)    (2×14 headers)     (1×6+1×6+1×2)│
│                                                          │
│   [DAC 1 header]     passives zone        [DAC 2 header] │
│   (1×6)              (caps, ferrites,      (1×6)         │
│                       resistors)                         │
│                                                          │
│   [6N138 socket]                                         │
│   (DIP-8)            MIDI resistors                      │
│                      + diode                             │
│                                                          │
├──────────────────────────────────────────────────────────┤
│ JACK EDGE (front panel)                                  │
│ [IN1] [IN2] [MIDI IN] [SUST/EXPR] [MIDI OUT] [OUT1] [OUT2]│
└──────────────────────────────────────────────────────────┘
       ← ANALOG GROUND ZONE →    ← DIGITAL GROUND ZONE →
```

### Jack Layout (left to right, front edge)
1. Audio IN 1 (stereo L/R)
2. Audio IN 2 (stereo L/R)
3. MIDI IN (TRS Type A, through 6N138)
4. Sustain / Expression (software-configurable)
5. MIDI OUT (TRS Type A)
6. Audio OUT 1 (stereo L/R, from DAC 1)
7. Audio OUT 2 (stereo L/R, from DAC 2)

All 3.5mm TRS, through-hole PCB mount.

---

## Female Header Sizes per Breakout

| Breakout | Header Config | Pin Spacing | Notes |
|----------|--------------|-------------|-------|
| **Teensy 4.0** | 2× 1×14 female | 2.54mm | Rows on each long edge, 36mm × 18mm board |
| **PCM5102 DAC** (×2) | 1× 1×6 female | 2.54mm | SCK, BCK, DIN, LCK, GND, VIN |
| **PCM1808 ADC** (×2) | ~1×6 top + 1×6 bottom + 1×2 side | 2.54mm | Verify exact pin count with calipers |
| **6N138** | 1× DIP-8 socket | 2.54mm | Standard 8-pin IC socket, 7.62mm row spacing |

### DAC Header Pinout (confirmed from board photos)
```
Pin 1: SCK  — NOT connected on motherboard (breakout ties to GND internally)
Pin 2: BCK  — Teensy BCLK (pin 21)
Pin 3: DIN  — Teensy DOUT (pin 7 for DAC1, pin 2 for DAC2)
Pin 4: LCK  — Teensy LRCLK (pin 20)
Pin 5: GND  — connect to DGND plane
Pin 6: VIN  — connect to 3.3V rail
```

Additional motherboard connections per DAC:
- XSMT pad/pin → 10kΩ pull-up → 3.3V (guarantees unmuted)
- FMT pad/pin → 10kΩ pull-down → GND (guarantees I2S format)
- L output pad → 10µF AC coupling cap → Audio OUT jack TIP
- R output pad → 10µF AC coupling cap → Audio OUT jack RING

### ADC Header Pinout (from board photos — verify exact order)
```
Top row (6 pins):    BCK, DOUT, LRC, SCK, AGND, VCC
Bottom row (6 pins): FMT, MD0, MD1, DGND, VDD, [verify if 6th pin]
Left side (2 pins):  LINR, RINR
```

Motherboard connections per ADC:
- SCK → Teensy pin 23 (MCLK) — only ADCs get MCLK, not DACs
- DOUT → Teensy pin 8 (ADC1) or pin 3 (ADC2)
- LRC → Teensy pin 20 (shared LRCLK net)
- BCK → Teensy pin 21 (shared BCLK net)
- VCC → 5V rail through ferrite bead (analog power)
- VDD → 3.3V rail (digital power)
- AGND → analog ground plane
- DGND → digital ground plane
- MD0, MD1, FMT → all tied to GND (slave mode, I2S format)
- LINR ← 1kΩ ← Audio IN jack TIP
- RINR ← 1kΩ ← Audio IN jack RING

---

## Physical Component Dimensions

| Component | Dimensions | Package | Footprint Notes |
|-----------|-----------|---------|-----------------|
| Teensy 4.0 | 36mm × 18mm | Through-hole headers | 2×14 pin, 2.54mm spacing |
| PCM5102 DAC breakout | ~29mm × 16mm | Through-hole 1×6 header | Has built-in 3.5mm jack (ignored) |
| PCM1808 ADC breakout | ~30mm × 30mm | Through-hole multi-row | Has 4 onboard electrolytic caps, mounting holes |
| 6N138 optocoupler | 9.91mm × 6.86mm × 3.04mm | DIP-8 through-hole | 2.54mm pin spacing, 7.62mm row spacing |
| 3.5mm TRS jack (CESS) | **Measure with calipers** | Through-hole PCB mount | 3 pins — measure spacing when arrived |
| Ferrite bead 600Ω | **Measure with calipers** | Through-hole (verify) | Measure lead spacing when arrived |
| 100nF ceramic cap | ~5mm × 2.5mm typical | Through-hole radial | 2.54mm or 5.08mm lead spacing |
| 10µF electrolytic | ~5mm dia × 7mm tall typical | Through-hole radial | 2mm or 2.54mm lead spacing |
| 1N4148 diode | ~4mm × 2mm | Through-hole axial | ~7.62mm lead spacing |
| Resistors (1/4W) | ~6mm × 2mm | Through-hole axial | ~10mm lead spacing |

---

## Grounding Strategy (Audio Quality Critical)

### Split Ground Plane
```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│     ANALOG GROUND (AGND)    │    DIGITAL GROUND (DGND)   │
│                             │                            │
│  - ADC analog sections      │  - Teensy 4.0              │
│  - Audio jack sleeve pins   │  - MIDI circuit (6N138)    │
│  - DAC analog outputs       │  - DAC digital pins        │
│  - Ferrite bead ground side │  - ADC digital pins        │
│                             │  - USB signals             │
│                             │                            │
│                        ★ STAR GROUND ★                   │
│                     (single connection point              │
│                      at Teensy GND pin)                   │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Ground Rules
- AGND and DGND connect at ONE point only (star ground near Teensy GND)
- Use via fence (row of ground vias) to physically separate the two zones
- Never route digital return current across the analog ground plane
- Audio jack sleeve pins go to AGND only — never DGND
- 6N138 MIDI circuit on DGND side, physically separated from audio section

---

## Power Distribution

```
USB 5V (from Teensy VUSB)
  │
  ├── 500mA polyfuse ── 5V RAIL
  │                       │
  │                       ├── Ferrite bead ── ADC 1 VCC (analog 5V)
  │                       ├── Ferrite bead ── ADC 2 VCC (analog 5V)
  │                       └── 6N138 pin 8 (Vcc)
  │
  └── 10µF bulk cap (5V entry)

Teensy 3.3V (from onboard regulator, 250mA max)
  │
  ├── 3.3V RAIL
  │     │
  │     ├── DAC 1 VIN
  │     ├── DAC 2 VIN
  │     ├── ADC 1 VDD (digital)
  │     ├── ADC 2 VDD (digital)
  │     ├── DAC XSMT pull-ups (×2, through 10kΩ)
  │     └── MIDI IN pull-up (4.7kΩ to 6N138 output)
  │
  └── 10µF bulk cap (3.3V entry)
```

**Power budget:** ~190mA on 3.3V rail (under 250mA limit, but no headroom for extras)

### Decoupling Capacitors
- 100nF ceramic within **1.5mm** of every VCC/VDD/VIN pin
- Use fat vias (0.3mm+) directly to ground plane
- No long traces between cap and pin — shorter = better
- ADC breakouts already have onboard electrolytics, but still add 100nF on motherboard near headers

---

## Signal Routing Rules (Optimized for Audio Quality)

### Clock Lines (MCLK, BCLK, LRCLK)
- Route as short as possible, bundled together
- Keep on DGND side of the board
- **Match trace lengths** within ~10-15mm of each other (prevents clock skew / jitter)
- Route in parallel, same approximate path from Teensy to each IC
- Keep at least 2-3mm from audio input traces and ADC input pins
- MCLK (pin 23) only goes to ADC 1 SCK and ADC 2 SCK — NOT to DACs

### Audio Signal Traces (I2S Data + Analog)
- I2S data lines (DIN/DOUT) on bottom layer, AGND side
- Keep 2-3mm clearance from clock traces and USB traces
- Never run audio traces parallel to clock or USB lines
- Audio jack grounds → short, direct vias to AGND (no long traces)

### MIDI Circuit
- 6N138 provides galvanic isolation — MIDI side is floating
- Keep MIDI circuit physically separated from audio section
- 6N138 output side connects to DGND
- Short trace from 6N138 pin 5 to Teensy pin 0 (RX1)

### Trace Widths
- **Power traces (3.3V, 5V):** ≥1mm wide (wider = less noise)
- **Signal traces:** 0.25mm-0.3mm standard
- **Audio analog traces:** 0.3mm minimum, keep short

---

## KiCad Workflow (Step by Step for Beginners)

### What is KiCad?
KiCad is free, open-source PCB design software. Two main editors:
1. **Schematic Editor** — draw the circuit (what connects to what)
2. **PCB Editor** — place components and route traces on the physical board

You work in the schematic first, then transfer to the PCB layout.

### Step 0: Install + Setup
1. Download KiCad 8 from [kicad.org](https://www.kicad.org/download/)
2. Install — includes all standard component libraries
3. Create new project: File → New Project → name it `4-i-gor`
4. This creates `.kicad_pro`, `.kicad_sch`, and `.kicad_pcb` files

### Step 1: Schematic (Estimated: 3-5 hours with AI help)

**What you're doing:** Drawing every component and connection from the spec.

1. Open schematic editor (the `.kicad_sch` file)
2. Add symbols for each component:
   - Place → Add Symbol → search library
   - For breakout boards: use **generic connector symbols** (Conn_01x06, Conn_01x14, etc.)
   - For 6N138: search "6N138" in library (should exist)
   - For passives: search "R" (resistor), "C" (capacitor), "D" (diode), "Ferrite_Bead"
   - For TRS jacks: use "AudioJack3" or generic 3-pin connector
3. Wire everything according to the pin connection tables in 4-i-gor.md
4. Add power symbols: +3.3V, +5V, GND, AGND
5. Label nets: MCLK, BCLK, LRCLK, DIN1, DIN2, DOUT1, DOUT2, MIDI_RX, MIDI_TX
6. Run ERC (Electrical Rules Check): Inspect → Electrical Rules Checker
7. Fix any errors

**Tips:**
- Use labels instead of wires for long connections (cleaner schematic)
- Group related components together (DAC section, ADC section, MIDI section)
- Each breakout is just a connector symbol — the actual IC is on the breakout board

### Step 2: Assign Footprints (Estimated: 1-2 hours)

**What you're doing:** Telling KiCad the physical shape of each component.

1. Tools → Assign Footprints
2. For each symbol, pick a footprint from the library:
   - Breakout headers: `Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical` (use female socket variant)
   - Teensy: `Connector_PinHeader_2.54mm:PinHeader_1x14_P2.54mm_Vertical` (×2)
   - 6N138: `Package_DIP:DIP-8_W7.62mm`
   - Resistors: `Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal`
   - Caps 100nF: `Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P2.50mm`
   - Caps 10µF: `Capacitor_THT:CP_Radial_D5.0mm_P2.00mm`
   - Diode: `Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal`
   - **TRS jacks: May need custom footprint — measure with calipers, create if not in library**
   - Ferrite bead: use axial resistor footprint if through-hole, or measure and pick
3. Run DRC after assigning all footprints

**Custom footprints:** If the CESS TRS jacks aren't in the KiCad library (likely), you'll need to:
1. Measure pin spacing with calipers
2. Open Footprint Editor → create new footprint
3. Place through-hole pads at the measured spacing
4. Draw the component outline on the silkscreen layer
5. Save to your project library

### Step 3: PCB Layout (Estimated: 4-8 hours with AI help)

**What you're doing:** Placing components on the board and routing copper traces.

1. Open PCB editor → Tools → Update PCB from Schematic
2. All footprints appear in a pile — drag them to approximate positions
3. **Place in this order:**
   a. Board outline first (Edge.Cuts layer) — draw rectangle ~120mm × 80mm
   b. TRS jacks along front edge (they define the panel)
   c. Teensy in center
   d. DAC breakouts near their output jacks
   e. ADC breakouts near their input jacks
   f. 6N138 near MIDI jacks
   g. Passives close to the ICs they serve (caps within 1.5mm of power pins)
4. **Set up ground planes:**
   - Add filled zone on front copper → select GND net → draw zone over digital area
   - Add second filled zone on front copper → select AGND net → draw zone over analog area
   - Ensure they connect at exactly one point (star ground)
5. **Route traces:**
   - Route power first (3.3V, 5V) — make these wide (≥1mm)
   - Route clock signals (MCLK, BCLK, LRCLK) — keep short, matched length, on DGND side
   - Route I2S data lines
   - Route MIDI circuit
   - Route audio analog traces last (AGND side)
6. Run DRC (Design Rules Check): Inspect → Design Rules Checker
7. Fix any clearance violations, unconnected nets

**Routing tips:**
- Press 'X' to start routing a trace
- Press 'V' to drop a via (switch layers)
- Use 'B' to refill ground planes after routing changes
- 45-degree angles only (no 90-degree bends on signal traces)

### Step 4: Review + Generate Files (Estimated: 1-2 hours)

1. **3D viewer:** View → 3D Viewer — check that everything looks right physically
2. **Silkscreen:** Add text labels for each connector (DAC1, DAC2, ADC1, ADC2, MIDI IN, etc.)
3. **Generate Gerber files:** File → Fabrication Outputs → Gerbers
   - Select all copper layers, silkscreen, solder mask, edge cuts
   - Generate drill file too
4. **Upload to manufacturer:** JLCPCB, PCBWay, or OSH Park
   - Upload the .zip of Gerber files
   - Select: 2-layer, standard thickness (1.6mm), any color
   - Typical cost: $5-15 for 5 boards + shipping

---

## Time Estimates (With AI Assistance)

| Phase | Time | Notes |
|-------|------|-------|
| Install KiCad + learn basics | 1-2 hours | Watch a 30-min YouTube tutorial first |
| Draw schematic | 3-5 hours | AI can help verify connections |
| Assign footprints | 1-2 hours | May need custom footprint for TRS jacks |
| PCB layout + routing | 4-8 hours | Most time-consuming part |
| Review + generate Gerbers | 1-2 hours | 3D viewer helps catch physical mistakes |
| **Total** | **10-19 hours** | Spread over a few days is fine |
| Order + shipping | 1-3 weeks | JLCPCB is cheapest/fastest |
| Solder + assemble | 2-3 hours | Headers first, then passives, then jacks |

---

## Recommended Measuring Tool

**Digital caliper** — the single most important tool for this project.

Use it to measure:
- TRS jack pin spacing (for custom KiCad footprint)
- Ferrite bead lead spacing
- Breakout board header pin spacing (verify 2.54mm)
- PCB-to-enclosure fit

Recommended: any $10-15 digital caliper on Amazon with mm/inch toggle.

---

## Assembly Order (After PCB Arrives)

1. Solder female headers for all breakout boards (lowest profile first)
2. Solder DIP-8 socket for 6N138
3. Solder all resistors
4. Solder all ceramic capacitors (100nF)
5. Solder electrolytic capacitors (10µF) — watch polarity
6. Solder ferrite beads
7. Solder diode (1N4148) — watch polarity (band = cathode)
8. Solder polyfuse
9. Solder AC coupling caps (if needed)
10. Solder TRS jacks last (tallest components)

### First Power-On Checklist
1. Before inserting any breakouts: check 3.3V and 5V rails with multimeter
2. Check no shorts between AGND and DGND (should be connected at one point only)
3. Insert Teensy only — verify USB enumeration on computer
4. Insert DAC breakouts — verify no smoke, check 3.3V at VIN pins
5. Insert ADC breakouts — verify 5V at VCC (through ferrite bead) and 3.3V at VDD
6. Insert 6N138 — verify 5V at pin 8
7. Load firmware, test audio output first, then input, then MIDI

---

## Verify Before Manufacturing

Before uploading Gerber files, check:

- [ ] All nets connected (no unconnected ratsnest lines)
- [ ] DRC passes with zero errors
- [ ] Ground planes fill correctly with AGND/DGND separation
- [ ] Star ground connection exists at one point
- [ ] Clock traces are short and matched length
- [ ] Audio traces on AGND side, away from clocks
- [ ] Decoupling caps within 1.5mm of every power pin
- [ ] Power traces ≥1mm wide
- [ ] Ferrite beads in-line on 5V to ADC VCC
- [ ] Polyfuse on 5V rail
- [ ] 3D view looks physically correct (no overlapping components)
- [ ] Board outline dimensions fit your enclosure
- [ ] All component values labeled on silkscreen
