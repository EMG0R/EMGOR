# PMOR v2 — Housing Design Brief

**For:** a housing-design agent. This is the complete input spec.
**Date:** 2026-09-13
**Status of the electronics:** bring-up verified on hardware (see §1). Housing is the open work.

---

## 0. The one thing that changed most

**v1 was "housing dropped on top of a PCB." v2 is panel-mount: every user-facing
component fastens to the housing itself.**

In v1 the PCB was the structural spine — parts were soldered to it, and the shell was a
lid with holes that had to line up with wherever the PCB put things. That coupled the
industrial design to the PCB layout and made both harder.

**v2 inverts it.** The housing is the structure. Controls, jacks, screen and speakers
mount to the shell, and the electronics hang off them by wire. Consequences the design
must honour:

- **Component placement is a design decision, not a PCB consequence.** Put controls
  where they feel right.
- **Every panel-mount part needs a real mounting feature** — threaded boss, captive nut,
  screw flange, bezel lip, or a press-fit pocket with a retention detail. Not just a hole.
- **Wire runs need routing volume and strain relief.** Assume flying leads everywhere.
- **The board(s) need their own mounts** — standoffs or a sled — independent of the panel
  parts.
- **It must come apart for service.** This is a rebuild of a lost unit; the user will be
  inside it repeatedly.

**Also: taller than v1.** v1 was 46mm deep. v2 should be taller — exact figure is a design
decision, driven by the sealed speaker chambers (§4), the joystick nub travel, and the
panel-mount standoff depth. Don't treat 46mm as a ceiling.

---

## 1. What is verified working (do not redesign these)

Bring-up on real hardware, 2026-09-12/13, on a **Daisy Seed 3**:

| Subsystem | Status |
|---|---|
| NeoTrellis 4x4 + bit-banged I2C | ✅ verified |
| Rotary encoder (KY-040 breakout) | ✅ verified |
| Analog joystick (2 axis + push) | ✅ verified |
| MPU6050 6-axis IMU | ✅ verified |
| VL53L1X lidar | ✅ working |
| OLED 2.42" SSD1309, bit-banged 4-wire SPI | ✅ flashed, panel driving |
| micro-SD, bit-banged SPI + FatFS | ✅ driver proven in PMOR |
| FSR velocity | ✅ tuned (330R, cubed curve, 13/16 ceiling) |
| Buttonless DFU over USB CDC | ✅ verified |

Not yet built: audio I/O jacks, amp + speakers, mic, power system, housing.

---

## 2. Parts manifest

Dimensions marked **[MEASURE]** are not confirmed — the designer must caliper the actual
part before committing geometry. Do not trust a remembered figure for a mounting feature.

### Compute
| Part | Qty | Dimensions | Mounting notes |
|---|---|---|---|
| **Daisy Seed 3** (STM32H750, TAC5242 codec, **onboard USB-C**) | 1 | 60.96 x 25.4mm PCB, ~12mm tall on machine-pin headers (Seed 3 is lower-profile than earlier revs) | Socketed on machine-pin headers, **must stay removable**. Its USB-C is the external port (§6). |
| Machine pin female headers 1x40P 2.54mm | 1pk | — | Snap to 2x 20-pin rows |

### User interface (all PANEL MOUNT)
| Part | Qty | Dimensions | Mounting notes |
|---|---|---|---|
| **NeoTrellis PCB** (Adafruit 3954) | 1 | **60 x 60mm** | 4x corner holes. Silicone pad sits proud of the PCB — bezel must clear the button tops at rest and allow full travel. |
| **NeoTrellis silicone** (Adafruit 1611) | 1 | 60 x 60mm | Needs a compression lip around the perimeter or the pads lift. |
| **OLED 2.42" 128x64 SSD1309**, 7-pin SPI | 1 | **[MEASURE]** module PCB and active-area offset | Active area is NOT centred on the module PCB — measure the offset and align the window to the *glass*, not the board. |
| **Rotary encoder — KY-040 breakout** | 1 | **[MEASURE]** breakout PCB | Note: this is a *breakout board*, not a bare PCB-mount encoder like v1's CYT1100. It has its own PCB with onboard pull-ups. Shaft is ~20mm, threaded bushing may or may not be present — **[MEASURE]**. Needs a knob. |
| **Joystick — user-supplied NUB** | 1 | **[MEASURE — user has a specific nub in hand]** | **User intends to remove the joystick from its breakout/module and mount the nub directly to the housing.** Design needs: a pocket/bracket for the stick body, clearance cone for full travel, and a route for its 5 wires (X, Y, SW, V+, GND). Confirm travel angle and cap diameter with the user. |
| **FSR — SF45-65** | 1 | **45 x 45mm active, 50 x 65mm body**, film-thin | Sits UNDER the NeoTrellis, between trellis PCB bottom and the structure below. Needs a flat, rigid backing plate and even pressure distribution, or velocity response goes uneven across the grid. Tail exits and needs a routing slot. |

### Sensors
| Part | Qty | Dimensions | Mounting notes |
|---|---|---|---|
| **MPU6050** 6-axis IMU (GY-521 class) | 1 | **[MEASURE]** ~21 x 16mm typical | **Must be RIGIDLY mounted** — an accelerometer measures whatever it is bolted to. Soft/tape mounting adds noise and drift. Keep away from speaker magnets and boost-converter inductors. |
| **VL53L1X ToF lidar** (TOF400C class) | 1 | **[MEASURE]** | Needs an unobstructed optical window. No cover glass unless it is IR-transmissive and non-scattering. Sensor face must sit flush/proud, not recessed behind a tunnel. |
| **MAX4466 mic preamp + electret** | 1 | **[MEASURE]** | Mic port needs an acoustic path to outside. **Isolate mechanically from the speakers** or it will pick up structure-borne vibration. In v1 the mic hole was jammed ~3mm from the PCB top edge — v2's panel-mount freedom removes that constraint, use it. |

### Audio
| Part | Qty | Dimensions | Mounting notes |
|---|---|---|---|
| **PJ-307 3.5mm switched jack** | 2 | 5-pin | **Panel mount now.** One IN, one OUT. The switch contact is TIP-referenced (used for headphone detect). |
| **Speakers — 20 x 40 x 5.5mm, 8 ohm 2W** | 2 | 20 x 40 x 5.5mm | **SEE §4 — SEALED CHAMBERS. This is the single biggest acoustic requirement.** |
| **PAM8403 amp** (VKLSVAN Super Mini, pin-pad) | 1 | **[MEASURE]** | Internal. No USB connector on the board despite the listing. |

### Power
| Part | Qty | Dimensions | Mounting notes |
|---|---|---|---|
| **LiPo 10000mAh** (LP1260110) | 1 | **60 x 112 x 12mm** | **The largest single object in the device.** See §7 — worth asking the user whether to downsize. Needs foam padding, no compression, no puncture risk. |
| **TP4056 + DW01A charger** | 1 | **[MEASURE]** ~26 x 17mm | Internal. Gets warm while charging — do not bury in foam. |
| **XL3608-5V boost x2** | **2** | **[MEASURE]** ~25 x 15mm ea | **Two now, not one** — one digital, one audio-only. Separate them physically; keep their inductors away from the IMU and the speakers. |
| AO3401 P-MOSFET + EG1218 slide switch | 1 ea | EG1218 **11.6 x 4 x 7.4mm**, 1.5mm pin pitch | Main power. Slider must protrude through the shell. |
| **CHARGE switch (NEW)** — SPST, **>=2A rated** | 1 | **[MEASURE]** | Panel mount near the USB-C. Breaks Seed VBUS -> TP4056. **Required**, not optional — the USB isolator supplies only 400mA and the charger wants 1A. |
| **LIFT switch (NEW)** — SPST, signal level | 1 | **[MEASURE]** | Panel mount near the audio OUT jack. Lifts the output sleeve ground (10R \|\| 100nF network). |

### Storage
| Part | Qty | Dimensions | Mounting notes |
|---|---|---|---|
| **WWZMDiB SPI micro-SD module** | 1 | **[MEASURE]** ~25 x 15mm | **Card slot should be user-accessible from outside** if possible — v1 buried it. Worth a design pass. |
| SanDisk 16GB microSDHC, FAT32 | 1 | — | |

### Fasteners / misc
| Part | Qty | Notes |
|---|---|---|
| M2 x 30mm socket head cap screws, 304 SS | pk | Front-to-back shell fastening |
| M2 brass heat-set inserts | 4+ | Melted into solid shell bosses |
| Polyester batting / 2mm foam | 2 sheets | Battery padding, rattle damping |
| Passives | — | 330R (FSR), 10R (ground lift), 10k x4, 100k x2, 100nF x12+, 10uF x7+, 100uF, **470uF (amp bulk)**, 1uF |

---

## 3. Cancelled from v1 — do not design these in

- **USB-C breakout + micro-USB male flying-lead hop.** Seed 3 has an onboard USB-C.
  The external port is now just a hole aligned to the Seed's own connector.
- **Isolated clock island** (B0505S DC-DC, PC817/6N137 optocouplers, extra TRS jacks).
  Clock goes over USB MIDI; ground isolation is handled by an external USB isolator dongle.
- **Second rotary encoder.** Replaced by the joystick nub.

---

## 4. ACOUSTIC REQUIREMENT — sealed speaker chambers

**This is the highest-value requirement in the document and it costs nothing but geometry.**

v1 mounted the two drivers open-backed, firing sideways into external "ear" scoops, with
their rear radiation dumping into the main body cavity. That is a **dipole**: front and
back waves cancel through the midrange, which is exactly where a 20x40mm driver makes its
perceived loudness. It is why v1 was quiet.

**Requirement: each driver gets its own SEALED rear chamber of roughly 5-10cc.**

- Sealed means sealed — gasket or a compliant bead at the driver flange, no wire pass-through
  without sealing around it, no shared volume with the main cavity.
- The two chambers must not connect to each other.
- Front radiation still exits through a grille to the outside.
- Chamber walls should be stiff. Thin flexing walls resonate and undo the benefit.

**Optional improvement worth considering:** front-firing rather than side-firing. v1's side-firing
"ears" were a passive redirect that gives only a modest forward bias. If the taller form factor
allows front-firing drivers, that is strictly better than any scoop.

---

## 5. Thermal and EMI placement rules

- **Two boost converters** — physically separate them. Their inductors radiate.
- **Keep boost inductors and speaker magnets AWAY from the MPU6050.** Magnetic fields
  corrupt the IMU.
- **TP4056 gets warm charging** at 1A — needs some airflow, not buried in foam.
- **The OLED module has its own boost converter** for the panel. Known issue: under
  lit-pixel load it sags the rail enough to kill the Daisy's USB. Firmware mitigates
  (contrast pinned at 0x01) but give the module some breathing room.
- Do not fully encase switching converters in tape or foam.

---

## 6. Openings required

| Opening | For | Notes |
|---|---|---|
| NeoTrellis window | 60x60 silicone pad | Bezel must allow full button travel |
| OLED window | active glass area | Align to glass, not module PCB |
| Encoder shaft | KY-040 shaft + knob | |
| **Joystick nub** | full travel cone | **[MEASURE travel angle]** |
| Lidar window | optical, unobstructed | No scattering cover |
| Mic port | acoustic path | Mechanically isolated from speakers |
| Speaker grilles x2 | front radiation | |
| USB-C | Seed 3's onboard connector | Aligned to the Seed's position |
| 3.5mm jack x2 | IN and OUT | |
| Power slider | EG1218 | |
| **CHARGE switch** | SPST | Near USB-C |
| **LIFT switch** | SPST | Near audio OUT |
| micro-SD slot | card access | **Should be externally accessible** |

---

## 7. Open questions for the user

1. **Battery size.** The 10000mAh cell is 60 x 112 x 12mm and dominates the volume. A
   ~5000mAh cell would roughly halve that and cut charge time from ~10h to ~5h. **Ask
   before designing around the big one.**
2. **Joystick nub** — exact part, travel angle, cap diameter, and how it detaches from
   its module. Everything about that pocket depends on it.
3. **Speaker orientation** — front-firing (better) vs side-firing ears (v1's look).
4. **Overall height target** — "taller" is agreed, but how tall?
5. **Is the SD card meant to be user-swappable** from outside?

---

## 8. Reference material

- `WIRING-v2.md` — the locked electrical pin map, power topology, ground rules
- `pocket-OpGorator.md` — v1 housing spec: enclosure cutouts, layer stack, the "pebble"
  form language, Blender build methodology. **Its pin-assignment section was deleted for
  having drifted from the code — trust `WIRING-v2.md` for anything electrical.**
- `_fsr_scale.h` — FSR conditioning, locked
- `DaisyExamples/seed/INPUT_TEST/` — the bring-up firmware proving the parts above
