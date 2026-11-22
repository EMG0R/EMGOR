# neuralGrid Housing — Handoff for the Blender Agent (2026-09-03)

You are modeling the physical enclosure for **neuralGrid**, an 8×8 pad controller
with four ribbon sensors and four lidar height sensors. It is one *controller*
implementation of the Septagraph protocol (see `SEPTAGRAPH_PLAN.txt` §8 for the
interaction design the housing must serve). Author of record: emgor.

Rules: **additive-only** in this archive — never move/rename/delete anything.
Put all Blender work in `neuralGrid/housing/` (new dir): `.blend` files, exports
(STL/3MF for print, DXF/SVG for laser), and a `housing/README.md` with a BOM and
print/cut settings. Model in **millimetres** (Scene Units: Metric, Unit Scale
0.001, Length: Millimeters). Commit only your own files (`git add <paths>`,
never `git add -A`).

---

## 1. What it is, in one paragraph

A tabletop instrument. The player faces it, hits pads, slides fingers along four
ribbons on the four edges, and raises/lowers a hand above it (lidars read hand
height). LEDs under every pad speak a colour language (warm white = a robot's
real note; blue-green = synth continuum; violet pulse = predicted next; amber =
loop playhead). It connects by one USB cable to a Raspberry Pi 5 that runs the
brain. **Read the pads and LEDs as the star; everything else is frame.**

## 2. Components to house (as built and verified)

Dimensions marked **(verify)** are from datasheets/memory — **caliper them
before modelling**; the user has the parts. Ask for photos/measurements rather
than guessing.

| Part | Qty | Footprint / notes |
|---|---|---|
| Adafruit **NeoTrellis 4×4** PCB + silicone elastomer keypad | 4 (tiled 2×2 → one 8×8) | Each PCB 60 × 60 mm **(verify)**; tiled block **120 × 120 mm** with ~0 mm gap (boards butt together and are soldered edge-to-edge). Elastomer: 4×4 translucent silicone buttons, 15 mm pitch, ~6–8 mm proud of the PCB **(verify)**. Total stack (PCB + elastomer) ≈ 10–12 mm **(verify)**. Mounting holes at the PCB corners **(verify diameter/inset)**. I²C addresses 0x2E 0x2F / 0x30 0x31 (top row / bottom row) — orientation matters for the LED index map: index `i → x = i % 8, y = i // 8`, x left→right, y top→bottom from the player's view. |
| **SoftPot** linear membrane ribbon (SpectraSymbol-style) | 4 — labelled N, S, E, W | One per edge, parallel to the edge, just outside the pad block. Likely the **100 mm active-length** model (~124 × 20 mm overall, ~0.5 mm thick, adhesive back) **(verify length — could be 200 mm)**. Needs a flat, rigid, smooth surface under it and a gentle finger-accessible surround; tail connector (3-pin, 0.1") exits one end and must route under the top plate. Each has an ~82–100 kΩ pulldown resistor on the wiper — small, lives on the wiring, no housing impact. |
| **VL53L1X** time-of-flight lidar breakout (Pololu) | 4 — N, S, E, W | Pololu breakout ≈ 13 × 18 mm, two mounting holes **(verify)**. Sensor must point **straight up**, with an **unobstructed opening** — no cover glass/acrylic over it unless you accept range error (the sensor's FoV is ~27°; give a clear cone). Ranges hand height 100–800 mm. Place one at the centre of each edge, outboard of the ribbon or between ribbon and pads (user's call — ask). XSHUT/INT pins wired; all four share the I²C bus. |
| **ESP32 WROOM devkit** (30/38-pin) | 1 | ≈ 55 × 28 mm, ~13 mm tall with headers **(verify)**. **Micro-USB** on a short edge — this is the ONLY external connection (to the Pi). Needs a USB port cut-out and strain relief. Runs I²C bus (GPIO21/22) to Trellis + lidars; ADC to ribbons. Keep it mounted so the USB port sits flush with a wall. |
| Raspberry Pi 5 (16 GB) | 0 or 1 | **Currently external** (`neuralgrid.local`, on the desk, USB to the ESP32). Optional internal bay only if the user asks — Pi 5 is 85 × 56 mm, needs airflow + access to its USB-C power and ethernet. Default: **do not house the Pi**; leave a single USB exit. |
| 5 V supply for the Trellis LED rail | 1 | Trellis LEDs run on a **separate 5 V rail**, common ground with the ESP32. Currently fed from the ESP32 board's VIN/5V pin off USB **(verify with user — if a separate 5 V barrel/USB-C input exists, it needs a second port cut-out)**. |
| Wiring | — | Dupont/silicone wire, ~1 cm bundle. Reserve a **≥ 12 mm** cavity under the Trellis for wiring and the Trellis's bottom-side components. |

## 3. Layout the interaction design demands

From the player's seat:

```
            [ N ribbon ]        <- rate modulator
            [ N lidar  ]
 [W]  ┌──────────────────┐  [E]
 [W]  │   8 × 8 pads     │  [E]      W ribbon = harmonicity
 [W]  │  120 × 120 mm    │  [E]      E ribbon = dynamics/loudness
 [W]  └──────────────────┘  [E]
            [ S lidar  ]
            [ S ribbon ]        <- space/distance modulator
```

- Pads: **x → frequency (low left → high right), y → scale/texture (clean front
  → dense back)**. So the *front* row (nearest the player) is y = 7 or y = 0
  depending on board orientation — **confirm with the user which physical edge
  is "front" before cutting anything**, then the Trellis board orientation is
  fixed by it.
- Ribbons are performance modulators, played while holding pads — they must be
  reachable by the same hand without lifting far. Keep them **within ~15 mm of
  the pad block edge**, flush or ≤ 1 mm recessed.
- Lidars read the hovering hand over the pads; they should sit at the pad
  block's edge midpoints so a hand over the centre is in all four cones.
- Top plate: pads protrude through a **120 × 120 mm (+ clearance) aperture** —
  or 64 individual 12–13 mm square holes if you want a bezel between buttons
  (nicer, but the elastomer's exact button size must be measured first).
- The elastomer is translucent: **light bleed between pads is bad** for the LED
  language. A bezel grid with ~1.5 mm walls between buttons (touching the PCB)
  kills bleed. Matte black or dark grey interior.

## 4. Fabrication constraints

- Target process: **FDM 3D print** (Bambu/Prusa class; 256 × 256 mm bed max — the
  body is ~180–200 mm square with ribbons, fits) **or** **laser-cut layered
  panels** (3 mm acrylic/plywood, tab-and-slot). Ask the user which; if no
  answer, model for **3D print** with the top plate as a separate part.
- Tolerances: 0.2–0.3 mm clearance on press-fits; M2.5 heat-set inserts for the
  Trellis and ESP32 standoffs; M3 for lid screws.
- Wall thickness ≥ 2.5 mm; top plate ≥ 3 mm (people will lean on it).
- No overhangs > 45° without a reason; split the body if height > ~50 mm.
- Total height budget: Trellis stack ~12 mm + wiring cavity ~12 mm + floor 3 mm
  ≈ **30 mm minimum**; make it 35–40 mm for hands and cable routing.
- Rubber feet recesses on the underside (4× 10 mm dia).
- **The ESP32 must be serviceable without desoldering**: lid off → board
  visible → USB reachable. Firmware is reflashed over that USB.

## 5. Deliverables

1. `housing/neuralgrid_housing.blend` — parametric where sensible (drivers or
   geometry nodes for pad pitch, ribbon length, wall thickness) so a caliper
   correction doesn't mean remodelling.
2. `housing/exports/` — STL/3MF per part (body, top plate, bezel, ESP32 tray,
   lidar mounts) and/or DXF per laser layer.
3. `housing/README.md` — BOM (with the verified dimensions filled in), print or
   cut settings, assembly order, which physical edge is "front", and every
   assumption you made.
4. Renders: one exploded view, one player's-eye view with pads lit in the LED
   language colours (warm white / blue-green / violet / amber) — useful for the
   NIME paper figure.

## 6. Questions to ask the user before modelling (don't guess these)

1. Exact measurements: NeoTrellis PCB, elastomer button size/height, SoftPot
   overall length, VL53L1X board vendor + hole spacing, ESP32 board variant.
2. Which physical edge is "front" (defines the y-axis orientation)?
3. 3D print or laser cut? Material/colour?
4. Lidars inboard (between ribbon and pads) or outboard of the ribbons?
5. Pi inside or outside (default: outside)?
6. Separate 5 V input port for the LEDs, or USB-only?
7. Any aesthetic references? (The current LED idle state is a slow purple
   "lavalamp" drift — the housing colour should not fight violet/blue-green.)

## 7. Context files (read-only)

- `neuralGrid/HANDOFF.md` §3 — as-built wiring, pins, addresses.
- `neuralGrid/docs/SEPTAGRAPH_PLAN.txt` §8 — artistic defaults (axis
  assignments, ribbon roles, LED language).
- `neuralGrid/firmware/esp32/grid_link_lidar/README.md` — pin table.
- Monitor UI (if the Pi is on): http://neuralgrid.local:8765 shows the live
  grid orientation and sensor lanes — useful for confirming which edge is which.
