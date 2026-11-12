# hyperGuitar — Design & Build Document

**Date:** 2026-05-22
**Status:** Pre-build. Design locked. Luthier brief ready.

---

## 1. Concept

hyperGuitar is a custom **semi-hollow headless 6-string** electric guitar with a fully embedded **Bela Gem Multi** DSP core. Ten passive audio sources — a neck humbucker, a bridge humbucker, a Submarine Pickups **SubSix** hexaphonic pickup in the middle position, and two body-mounted piezo contact mics — are digitised at 24-bit / 96 kHz and processed entirely in software. The musical core is **digital pickup blending**: every channel has a software-controlled gain modulated by physical controls and gestures, enabling continuous phasor / comb-filter sweeps between pickup positions, per-string spectral processing on the hex pickup, and a full pedalboard-grade effects suite headlined by a **flagship, H90-class real-time pitch shifter**. An internal **hexaphonic pitch-to-MIDI synth engine** turns each string's tracked pitch into a polyphonic synth voice on Bela, scaling toward the CPU headroom ceiling. Sensors: a NeoTrellis 4×4 grid, two pots, two encoders, a 5-way blade switch and a Jazzmaster slide switch, a VL53L1X time-of-flight LiDAR, a BNO055 IMU, and a perimeter NeoPixel ring. The instrument runs untethered on an internal LiPo, charges and programs through a single USB-C, and outputs stereo TRS plus optional USB-C audio and casual wireless.

---

## 2. Body spec — for online custom order

This section is the spec attached to an online custom-build order (no local luthier interaction). The doc is sent as supplementary documentation alongside the order form.

**Order via [Halo Custom Guitars](https://www.haloguitars.com/store/custom-guitars.html)** — their headless 25.5" single-scale custom build, with "Submit custom design" (+$150) for shapes beyond their presets. Attach Strandberg-inspired reference imagery + this spec sheet. Lead time 3–4+ months. 1:1 Strandberg cloning will be refused on IP grounds; "Strandberg-inspired but distinct" is the path. Preset shapes **Merus** and **Octavia** are worth comparing first — one of them may already be close enough to skip the custom-design fee.

### 2.1 Instrument type
- **Semi-hollow headless 6-string electric.**
- **Tuning:** **B standard** (B E A D F♯ B, low to high). Nut clamp slots, fret slot positions, and string-gauge recommendation sized for this. Suggested gauges: **.011–.056** (looser feel) or **.012–.060** (tighter, more defined).
- **Scale length:** **25.5" single-scale, parallel frets** (no fan). Player preference is loose feel; 25.5" delivers that at B standard without sacrificing low-end definition when paired with a .060 low B string. Default for almost every production headless 6-string — keeps body-shape options maximum.
- **Wood:** luthier's choice — light/medium tonewood (alder, swamp ash, mahogany, korina, basswood are all acceptable). The instrument's voice is sculpted in DSP; body wood is for feel, weight, and aesthetic.
- **Frets:** stainless or nickel, 22 or 24 — luthier choice.
- **Finish:** luthier's choice. Satin oil + wax is fine for v1. Back panel and LED diffuser stay unfinished — I add those.

### 2.2 Critical architectural rule — TWO SEPARATE CHAMBERS

The body has two distinct internal chambers, sealed off from each other:

- **Acoustic chamber** — the semi-hollow void, under the top, in (e.g.) the upper bout. Hosts the contact mics gluing surfaces and any f-hole / sound-port the luthier wants. Carries body resonance for the piezos to pick up.
- **Electronics chamber** — sealed cavity in the back / lower bout, accessed via a removable back panel. Hosts all the embedded electronics. Solid wood wall between this and the acoustic chamber so the electronics don't rattle and don't load the acoustic body.

This separation matters — combined chambers would have electronics rattling against the top, picking up noise, and changing the acoustic chamber's voice unpredictably.

### 2.3 Electronics chamber spec

- **Footprint:** ≥ **150 × 90 mm** interior. (Fits the Bela Gem Multi + battery + buffer board + power modules laid out side-by-side, not stacked.)
- **Depth:** ≥ **25 mm** interior. Total body thickness at the cavity ≈ 5 mm front face + 25 mm chamber + 5 mm back panel = **35–45 mm body thickness**.
- **Access:** the entire **back face** over the chamber is a **removable panel**. Brass threaded inserts pressed into the body around the panel perimeter; M3 machine screws hold the panel. Flush-recessed rabbet preferred.
- **Back panel material:** I supply — 5 mm tinted acrylic (lets the perimeter LED ring spill colour through). Luthier provides the rabbet and insert positions.
- **Cable pass-throughs into the electronics chamber:**
  - 2× ~5 mm holes near the SubSix's cable exit on the surface, with brass ferrules.
  - 1× hole near each humbucker route.
  - 2× small holes from the acoustic chamber for the contact-mic leads.
  - 1× hole from the LED channel.

### 2.4 Pickup routing

- **Neck humbucker route** — standard humbucker dimensions (~70 × 38 mm), standard orientation (parallel to nut / perpendicular to strings). Mounting either through-body screws into pickup rings, or direct-mount via brass inserts — luthier's call.
- **Bridge humbucker route** — standard humbucker dimensions, standard orientation. **If possible, position the bridge humbucker over a solid (un-chambered) section of wood** — magnetic pickups + semi-hollow + heavy DSP gain can feedback; a solid wood block under the bridge pickup tames it.
- **Middle position — SubSix:** **no route required.** The Submarine Pickups SubSix slides under the strings with its own height-adjuster. What's needed instead:
  - At least **50 mm of clear string length** between the back of the neck pickup ring and the front of the bridge pickup ring, top surface flat in this region.
  - Two ~5 mm cable pass-throughs adjacent to where the SubSix's two 3.5 mm TRRS leads naturally exit.

### 2.5 Contact mic mounting

- Two 27 mm piezo discs (or AKG C411-style contact mics) glue inside the body top:
  - **Mic A** — inside top, upper bout area.
  - **Mic B** — inside top, behind the bridge / lower bout.
- Luthier just leaves the inside top surface accessible from inside the acoustic chamber. I install the mics during assembly with cyanoacrylate or silicone RTV.
- Run 2 thin cable pass-throughs from the acoustic chamber into the electronics chamber for the mic leads.

### 2.6 Perimeter LED channel

- **Milled groove** around the body's side rim: ~**5 mm wide × 5 mm deep**, full perimeter.
- **Skip** the neck heel area (~30 mm gap leading into the neck pocket) and a ~20 mm window at each strap button.
- Designed for a 5 mm-wide WS2812B LED strip + a 5 mm silicone or acrylic diffuser cap. I supply strip and diffuser; luthier just cuts the channel.
- 1× pass-through hole from the channel into the electronics chamber for the strip's 3-wire lead.

### 2.7 Front-face cutouts (luthier-cut, layout at luthier's ergonomic discretion)

| Element | Cutout |
|---|---|
| NeoTrellis 4×4 (Adafruit 3954) | ~44 × 44 mm square, **flush-recessed pocket** ~5 mm deep into the front face |
| Pot × 2 | ~10 mm dia round holes, plus small flat for anti-rotation tab |
| Encoder × 2 | ~7 mm dia round holes |
| 5-way blade switch | ~30 × 6 mm through-slot |
| Jazzmaster slide switch | ~10 × 4 mm through-slot |
| VL53L1X LiDAR lens window | ~6 mm dia round, on the upper bout, **oriented to point outward** (toward the player's strumming hand or the audience — luthier's call which) |
| Status LED pinhole (optional) | 3 mm dia |

The NeoTrellis is the largest control element and should be placed reachable while playing — luthier ergonomic call.

### 2.8 Edge jack mounts

- **1/4" TRS stereo output jack** — panel-mount, on the **lower edge** (standard strap-button-adjacent location). Through-bore + nut + washer.
- **USB-C jack** — panel-mount right-angle, on the **side edge** near the lower strap button. Rectangular cutout ~10 × 5 mm for the connector.
- **Optional footswitch jack** — 1/4" TRS panel-mount, lower edge.

### 2.9 Headless hardware

- **Headless single-scale bridge** — standard 6-saddle headless bridge, all saddles in line. Quality recommendations: ABM 3700-series, Hipshot headless, Schaller headless. Double-ball-end string compatible.
- **Nut clamp** at the headstock end of the neck.
- **Tuners at the bridge** (standard headless geometry).
- **Truss rod** accessible at the bridge end.

### 2.10 What I supply in the order notes / attachments

- Exact Bela Gem Multi mechanical dimensions (70 × 58 × 22 mm, mounting-hole pattern).
- Exact LiPo pouch dimensions (~90 × 60 × 10 mm for 5000 mAh; confirm specific cell pre-build).
- Component datasheets for NeoTrellis, VL53L1X, BNO055, NeoDriver, USB-C panel jack, TRS panel jack, pots, encoders, switches.
- Note in order: I'll install the SubSix and contact mics myself after delivery; main humbuckers can be installed by Halo or by me at final assembly.

### 2.11 What ships from Halo

A complete playable headless 6-string with: neck installed, frets levelled, headless bridge and nut clamp mounted, strap buttons placed, electronics chamber + acoustic chamber routed and sealed from each other, all front-face cutouts cut, edge jack pass-throughs drilled, perimeter LED channel milled, pickup routes complete, cable pass-throughs drilled with ferrules, finish applied. **Without:** electronics, back panel, LED strip, contact mics. Main humbuckers installed at Halo's option or by me at final assembly; SubSix I install.

---

## 3. System architecture

```
[neck humbucker]──┐
[bridge humbucker]┤
[SubSix hex (6 ch)]┼──▶ Buffer board ──▶ Bela Gem Multi (10 audio IN @ 24-bit/96 kHz)
[contact mic A]   ┤                                  │
[contact mic B]───┘                                  │
                                                     ▼
                              PocketBeagle 2 (quad-core ARM Cortex-A53 @ 1.4 GHz, M4F, PRUs)
                              ├─ Core 0: I/O + per-channel pre-process + pickup mixer + phasor engine
                              ├─ Core 1: Flagship pitch shifter (stereo sum bus)
                              ├─ Core 2: Time FX (chorus, vibrato, flanger, phaser, delay, reverb)
                              └─ Core 3: Hex pitch-to-MIDI synth + modulation/saturation/filter + sensor mapping + OSC

[NeoTrellis]──────┐
[VL53L1X LiDAR]───┤
[BNO055 IMU]──────┼─ I²C / Qwiic chain ─▶ Bela Gem Multi I²C bus
[NeoDriver → WS2812 strip]─┘
                                  │
[2 pots]         ────┐
[5-way blade sw]─────┼─▶ Gem Multi analog inputs (8 ch dedicated, 16-bit / 24 kHz)
[Jazzmaster sw(s)]───┘
                                  │
[2 encoders + buttons]───▶ Gem Multi GPIO (16 digital I/O, 3.3 V)

                              Gem Multi audio OUT (10 ch)
                              ├─ Ch 1-2  ─▶ Stereo TRS 1/4" output jack
                              └─ Ch 3-10 ─▶ Spare (future CV / aux; 8 are DC-couple-able)

                              PocketBeagle 2 USB-C ─▶ USB-C panel jack (data + power + charge)
                              PocketBeagle 2 USB-A ─▶ WiFi/BT dongle (OSC + optional UDP audio stream)

[LiPo pouch] ─▶ USB-C LiPo charger w/ load-sharing power-path ─▶ 5 V boost ─▶ system 5 V rail
                            ▲
                            └── VBUS from USB-C panel jack
```

Bela's per-core `render()` API runs the four cores in parallel; chains of all four still stay under 5 ms end-to-end.

---

## 4. Compute & audio platform

- **Bela Gem Multi** (https://shop.bela.io/collections/multichannel/products/bela-gem-multi). 10 audio in / 10 audio out @ 24-bit / 96 kHz (8 outputs DC-couple-able); separate 8 analog inputs (16-bit / 24 kHz) for controls; 16 GPIO (3.3 V); I²C on dedicated pins + Qwiic connector; USB-C device + USB-A host; ~1 ms round-trip latency typical; 70 × 58 × 22 mm; 5 V powered, battery supported.
- **PocketBeagle 2** (rev A1, TI AM6254) — quad-core ARM Cortex-A53 @ 1.4 GHz, Cortex-M4F MCU, PRU subsystems handling sub-ms I/O.
- **OS / runtime.** Linux + Xenomai (Bela stack). Per-core render API across all 4 A53 cores in parallel; the chain stays under 5 ms even when four cores are serialised.
- **DSP languages:** **C++** for orchestration, the pickup mixer / phasor engine, sensor I/O, multi-core dispatch, and the flagship pitch shifter wrapper. **Faust** for the modulation / time / filter / saturation effects and the synth voices (compiles to tight optimised C++ inside Bela). Csound and Pure Data are *not* used — their interpreter overhead is the documented cause of headroom struggles on classic Bela.

---

## 5. Audio input path

### 5.1 Channel allocation

| Ch | Source | Notes |
|:--:|---|---|
| 1 | Neck humbucker | Passive |
| 2 | Bridge humbucker | Passive |
| 3 | SubSix — string 1 (low E) | Passive magnetic |
| 4 | SubSix — string 2 (A) | Passive magnetic |
| 5 | SubSix — string 3 (D) | Passive magnetic |
| 6 | SubSix — string 4 (G) | Passive magnetic |
| 7 | SubSix — string 5 (B) | Passive magnetic |
| 8 | SubSix — string 6 (high E) | Passive magnetic |
| 9 | Contact mic A — upper bout | Passive piezo |
| 10 | Contact mic B — behind bridge / lower bout | Passive piezo |

**Submarine Pickups SubSix** is fully passive magnetic: 12 N42 neodymium magnets + 6 micro-coils, one per string. Outputs 6 channels via **two 3.5 mm 4-conductor (TRRS) jacks** (3 channels + ground per jack). Slides under the strings — no body modification.

### 5.2 Buffer board (the only custom analog)

A single small protoboard with 10 channels of unity-gain FET-input op-amp follower. Purpose: **impedance matching, not gain.** Bela handles level in software (~59 dB of input gain available); the buffer exists to give every passive pickup the high-Z load it wants (≥ 1 MΩ) instead of being loaded down by the Gem Multi's input impedance.

- **Contact mic channels (9, 10) — mandatory.** Piezos otherwise sound thin / quacky into low-Z inputs (15 nF source cap × 50 kΩ input = ~200 Hz high-pass that eats the bass).
- **Neck / bridge magnetic channels (1, 2) — recommended.** Preserves resonant-peak character.
- **SubSix hex channels (3–8) — recommended.** Same magnetic-pickup reasoning. SubSix is passive magnetic, treated identically to neck/bridge.

Verdict: **all 10 channels get the buffer**, for sound quality.

**Circuit per channel** (single-supply 5 V, mid-rail biased, AC-coupled in and out):

```
       +5V ────┬──── V+ (op-amp pin 4)
               │
               R1 (4.7 MΩ)
               │
               ├──→ Vbias (~2.5 V) ── op-amp non-inv input ◀── C_in (1 µF film) ── PICKUP IN
               │
               R2 (4.7 MΩ)
               │
       GND ────┴─── C_bias (10 µF, to GND)
                                          op-amp output ─── C_out (10 µF) ── TO BELA INPUT
                                              │
                                              └── op-amp inv input (unity follower)
Per-IC: 100 nF ceramic V+ → GND. Bulk: 10 µF V+ → GND.
```

- R1 ∥ R2 = ~2.35 MΩ load → ~4.5 Hz HPF corner with a 15 nF piezo → full bass.
- Op-amp: **TL074** (DIP, cheap, FET-input) or **OPA1655** (low-noise upgrade in SOIC + adapter). 3 quad ICs cover all 10 channels.
- All passive RC; no gain stage; flat across the audio band.

### 5.3 Wiring

- Neck, bridge, and contact-mic leads solder directly to the buffer board (no internal jacks).
- SubSix wires in via **two short 3.5 mm TRRS leads** from its native outputs into the buffer board, where the 6 channels are broken out to discrete buffer inputs (3 channels + shared ground per TRRS).
- Buffer outputs to the Gem Multi via short shielded leads or a small flat ribbon.

---

## 6. DSP & instrument software

### 6.1 Signal graph

```
[10 input ch]
   │
   ▼
[per-channel pre-process: HPF, gain trim, per-string pitch + amplitude tracking on the 6 hex, light dynamics]
   │                                              │
   │                                              ▼
   │                            [hexaphonic pitch-to-MIDI synth: 6-voice polyphonic engine on Bela]
   │                                              │
   ▼                                              │
[digital pickup mixer / phasor engine:            │
 software gain + sub-ms delay per channel;        │
 cross-pickup comb / phase morphs]                │
   │                                              │
   ▼                                              │
[stereo sum bus] ◀────────────────────────────────┘   (synth mixed pre-FX or post-FX, per patch)
   │
   ▼
[flagship pitch shifter] ─▶ [chorus / vibrato / flanger / phaser] ─▶ [delay] ─▶ [reverb] ─▶ [saturation / filter / utility]
   │
   ▼
[stereo OUT bus] ─▶ TRS jack (Gem Multi out 1-2) + USB-C audio gadget + optional wireless stream
```

Cheap operations live on the 10-channel pre-mixer path; expensive operations live on the stereo sum.

### 6.2 Digital pickup mixer / phasor engine (signature)

Every one of the 10 channels has a software-controlled gain plus a sub-ms variable delay line. Modulating the gains crossfades between pickup positions, per-string voices, pickups vs piezos. Modulating the delays + summing generates **continuous phasor / comb-filter sweeps between pickup positions** that no physical pickup selector can produce. The mixer *is* the instrument's voice; effects are downstream.

Interface sketch:

```cpp
class PickupMixer {
public:
    static constexpr int kInputs = 10;
    struct ChannelParams { float gain; float delaySamples; float pan; };
    void setSampleRate(float sr);
    void setChannelParams(int ch, const ChannelParams& p);
    void process(const float* const* in, float* outL, float* outR, int frames);
};
```

Implementation: per-channel delay buffer (256 samples ≈ 2.7 ms at 96 kHz) with linear-interp read for fractional delay, equal-power pan on output, summed to L/R.

### 6.3 Flagship pitch shifter

The headline effect. Targets, in order:

1. **Quality** — H90-class clean transposition. Low artefact, transient-preserving, polyphonic on chords / mixed-string signal. Tonally clean across the guitar's range.
2. **Latency** — the lowest the algorithm window + Bela I/O permits. Bela's I/O is ~1 ms; the pitch algorithm's window dominates total latency. Aim for the cleanest result at the shortest viable window.
3. **Range** — wide interval range with no quality cliff.

Approach: integrate **Signalsmith Stretch** (https://github.com/Signalsmith-Audio/signalsmith-stretch) into the Bela C++ layer. Modern phase-vocoder lineage, peer-acknowledged top-shelf real-time quality, dedicated low-latency operating modes, clean C++, permissive license. Backup if needed: Rubber Band's real-time mode. Lives on Core 1, its own dedicated A53 core — generous headroom.

### 6.4 Effects suite

Hot-swappable Faust modules, sequenced by the patch loader:

- **Vibrato** — delay-line modulation; effectively zero added latency.
- **Chorus** — multi-tap delay modulation, stereo.
- **Flanger** — short delay with feedback.
- **Phaser** — allpass cascade; no added latency.
- **Delay** — variable, tape-style, with wow / filtering / feedback shaping.
- **Reverb** — Feedback Delay Network. Convolution avoided to preserve headroom.
- **Saturation / overdrive / distortion** — analog-modelled waveshaping per stage.
- **Filter** — state-variable multimode (LP / BP / HP / notch), modulatable.
- **Modulation utilities** — tremolo, ring mod, frequency shifter, bit-crush / sample-rate reducer.
- **Per-string utilities** (upstream of the sum) — HPF, gain, pan, optional per-string pitch-track for downstream control.

### 6.5 Hexaphonic pitch-to-MIDI synth

A per-string YIN-class pitch tracker runs on each of the 6 SubSix channels at the pre-process stage. It emits internal MIDI-style note events (note on/off, pitch, velocity from amplitude envelope) that drive a **6-voice polyphonic synth engine** on Bela. Synth output mixes into the stereo bus — patch-configurable as pre-FX (shares the guitar's FX chain) or post-FX (synth dry / processed independently).

The synth is the project's **scalable DSP load**. v1 voices are simple subtractive (osc + filter + ADSR); complexity grows from there — FM, wavetable, additive, physical modelling — taking whatever headroom remains after the pickup mixer, pitch shifter, and effect suite. Faust voice modules wrapped in a C++ voice allocator (one voice per string, no allocation conflict). MIDI events stay internal — the synth is part of the instrument's voice, not a controller for external gear.

Per-string detection latency is bounded by string period. With B standard tuning: low B (~62 Hz) needs ~16 ms to lock; high B (~247 Hz) under 5 ms. Per-string isolation makes this far easier than polyphonic detection on a summed signal.

### 6.6 Patches & mapping

A **patch** is: a routing graph + a control-mapping table. The mapping assigns each physical control (NeoTrellis pad, pot, encoder, switch position, LiDAR distance, IMU axis) to a software parameter (mixer gain, effect parameter, modulation source, pitch interval). Patches are JSON on the Bela's filesystem, hot-reloadable, selectable from the NeoTrellis or the 5-way switch.

Example patch:

```json
{
  "name": "phasor-sweep",
  "routing": { "synth_inject": "pre-fx", "chain": ["pitch","chorus","delay","reverb"] },
  "bindings": {
    "pot.1":     "mixer.neck.gain",
    "pot.2":     "mixer.bridge.gain",
    "lidar":     "mixer.bridge.delaySamples",
    "imu.roll":  "fx.reverb.mix",
    "enc.1":     "fx.pitch.interval",
    "sw.5way":   "patch.select"
  }
}
```

### 6.7 Multi-core layout

| Core | Role |
|---|---|
| 0 | Audio I/O, per-channel pre-process, digital pickup mixer / phasor engine |
| 1 | Flagship pitch shifter |
| 2 | Time-based FX (chorus, vibrato, flanger, phaser, delay, reverb) |
| 3 | Hexaphonic pitch-to-MIDI synth + modulation / saturation / filter + sensor mapping + OSC |

Cores are dedicated, not load-balanced — keeps timing deterministic.

---

## 7. Controls & sensors

### 7.1 I²C / Qwiic chain

Daisy-chained via STEMMA QT cables, no soldering between devices:

- **Adafruit NeoTrellis 4×4** (3954, addr 0x2E) — 16 RGB pads. Patch select, scene morph, effect on/off.
- **STMicro VL53L1X** (Adafruit 3967, addr 0x29) — time-of-flight, ≤ 4 m. Forward-facing on the upper bout. Distance → continuous parameter.
- **Bosch BNO055** (Adafruit 4646, jumpered to addr 0x28 to avoid VL53L1X conflict) — 9-axis with onboard sensor fusion. Tilt / roll / yaw → modulation, spatialisation, expression.
- **Adafruit NeoDriver Seesaw** (5766, addr 0x60) — drives the WS2812 perimeter strip over I²C so the audio thread is never tied up with WS2812 timing.

### 7.2 Analog inputs (Gem Multi's 8 dedicated, 16-bit / 24 kHz)

- 2 pots → 2 channels.
- 5-way blade switch → 1 channel via resistor-ladder voltage divider.
- Jazzmaster slide switch → 1 channel via resistor ladder (or to GPIO if discrete states preferred).
- 3–4 spare channels — expansion (expression pedal jack, additional pots, etc.).

### 7.3 GPIO

- Encoder 1 — A + B + button = 3 pins.
- Encoder 2 — 3 pins.
- Footswitch jack (optional) — 1–2 pins.
- Many lines spare.

### 7.4 Perimeter NeoPixel ring

WS2812B side-emit strip in the routed channel around the body's side rim, under a silicone or frosted-acrylic diffuser. Driven via the I²C NeoDriver. Visual mapping per patch: per-effect colour scheme, per-string activity, LiDAR proximity, IMU tilt, NeoTrellis state echo, pitch-shift interval visualisation.

### 7.5 Control bus & mapping philosophy

A C++ `ControlBus` (running on a Bela aux task at ~1 kHz) holds every source's normalised value addressable by string ID:

`pot.1`, `pot.2`, `enc.1`, `enc.1.btn`, `enc.2`, `sw.5way`, `sw.jazz`, `neotrellis.r0c0`…`r3c3`, `lidar`, `imu.roll`, `imu.pitch`, `imu.yaw`, `imu.acc.{x,y,z}`.

Every control is a **soft control**. The patch defines what each does. No hardwired control-to-effect binding. That's what makes the instrument "hyper" — its behaviour is patch-defined, not panel-fixed.

---

## 8. Outputs & wireless

- **Primary — Stereo TRS 1/4" jack** on the lower edge. Channels 1–2 of the Gem Multi's outputs. Zero-latency wired path.
- **Secondary — USB-C audio.** Bela Gem's USB-C device port configured as a USB Audio Class gadget on the PocketBeagle 2 (Linux USB gadget framework). Enumerates as a USB audio interface to a host computer — stereo (and optionally multichannel) directly into a DAW. Doubles as the development / programming connection. Sub-10 ms latency typical.
- **Optional — Wireless** via a Linux-compatible USB WiFi / BT dongle (Edimax EW-7811Un or equivalent) plugged into the Gem Multi's USB-A host:
  - **WiFi OSC** — phone or laptop edits parameters live. `/control/<bus-id>`, `/patch/load`, etc. ~10 ms LAN-RTT.
  - **WiFi UDP audio stream** — stereo PCM packets out, receiver on the same LAN. ~10–30 ms typical. Casual, not for tight monitoring.
  - **Bluetooth A2DP** — out to headphones, ~150 ms, casual listening only.
  - Wireless is convenience-grade; wired TRS + USB-C are the latency-critical paths.

Remaining 8 Gem Multi outputs are kept spare (8 DC-couple-able for potential future CV / aux routing).

---

## 9. Power system

- **Battery.** Single-cell LiPo pouch, flat form factor, **5 000 mAh** (~3.7 V × 5 Ah ≈ 18.5 Wh). System draw ~3.5 W → ~5 hours continuous. Sized to fit chamber depth.
- **Power module set** (all off-the-shelf, no PCB design):
  - **Adafruit USB-C LiPoly Charger** with load-sharing power-path (BQ24074-class).
  - **Adafruit PowerBoost 1000** 5 V boost converter.
  - **Adafruit USB-C breakout** — splits VBUS + GND from D+ / D−.
  - Inline 2 A polyfuse on VBUS.
  - Sparkfun Buck-Boost Battery Charger is an acceptable single-module substitute.
- **USB-C combined power + data.** One panel jack carries 5 V VBUS (to charger) and USB 2.0 data (to PocketBeagle 2 USB-C device port). USB-C breakout splits the lines. Power-path / load-sharing: USB plugged → charger feeds system and tops up battery; USB unplugged → battery + boost runs the system seamlessly. No USB-PD negotiation needed at ~5 W.
- **Power budget:** PB2 + Gem Multi ~2–3 W; sensors ~0.3 W; NeoPixels ~0.5–1 W average (cap brightness in software); buffer board negligible. Typical **~3–4 W**.
- **Safety.** Protected LiPo cell (internal PCM). Polyfuse on VBUS. LiPo not sandwiched under any module — must be able to vent.

---

## 10. Build sequence

Not phases — just the sensible order to do things once the luthier delivers the body.

**Parallel-trackable while the luthier is building:**

1. **Order all electronics** — Bela Gem Multi, PocketBeagle 2, microSD, USB-C cables/PSU, buffer-board BOM (TL074 ×3, sockets, resistors, caps, perfboard), pickups (SubSix + neck/bridge humbuckers + contact mic piezos), control hardware (NeoTrellis, VL53L1X, BNO055, NeoDriver, WS2812 strip + diffuser, pots, encoders, 5-way switch, Jazzmaster switch), power modules (Adafruit USB-C LiPoly Charger, PowerBoost 1000, USB-C breakout, 5000 mAh LiPo, polyfuse), USB-C panel jack, TRS panel jack, USB WiFi dongle.
2. **Bench bring-up of the Bela** — flash Gem image to SD, boot, verify IDE access, validate all 10 audio inputs + 10 outputs via a loopback project, validate the multi-core render path, validate the Faust compile path, validate the USB audio gadget enumeration.
3. **Build the buffer board on protoboard** — schematic from §5.2; build channel 9 (contact mic A) first, test end-to-end through Bela with a real piezo (silent noise floor, tap test, bass-response comparison against unbuffered), then replicate for the remaining 9 channels with per-channel verification.
4. **Bring up controls and sensors on the bench** — i²C chain (NeoTrellis, LiDAR, IMU, NeoDriver), pots and switches on analog inputs, encoders on GPIO, control-bus framework in C++. Integration smoke test where every source updates its bus slot.
5. **DSP suite** — pickup mixer in C++, Signalsmith Stretch integrated, Faust effect modules authored, per-string YIN trackers + hexaphonic synth (start with simple subtractive voices), patch loader, multi-core dispatch. Scale synth voice complexity until headroom ceiling. Author 3 starter patches.
6. **Power on the bench** — wire the USB-C breakout → charger → boost → 5 V system rail → battery. USB-only test, battery-only test, hot-handover test, thermal check.

**Sequential once luthier delivers:**

7. **Receive the guitar from the luthier.** Verify all routes and cutouts to spec.
8. **Fabricate the back panel** — 5 mm tinted acrylic, traced to body, drilled for the threaded inserts the luthier placed.
9. **Install electronics permanently:** mount Bela Gem Multi + PB2 on M3 standoffs into the chamber, install LiPo on VHB tape beside the stack, mount the buffer board and the power-module set, install the WS2812 strip in the perimeter channel + diffuser, install the contact mics inside the acoustic chamber, install the SubSix in the middle slide-under position, install neck and bridge humbuckers in their routes, install the front-face controls (NeoTrellis, pots, encoders, switches) and the LiDAR through its lens window, install TRS and USB-C panel jacks.
10. **Wire the audio harness** — pickup leads through pass-throughs to buffer board inputs (per §5.1 channel map), buffer outputs to Gem Multi inputs, contact mic leads through acoustic-chamber pass-throughs to buffer.
11. **Wire the control harness** — STEMMA QT chain (NeoTrellis → VL53L1X → BNO055 → NeoDriver), pots/switches to analog inputs, encoders to GPIO.
12. **Wire the power harness** — USB-C panel jack → breakout → charger → boost → 5 V rail; LiPo to charger BATT.
13. **Close the back panel.**
14. **Full system integration test** — cold-boot on battery, run the DSP project, play through every pickup, exercise every control, verify USB hot-handover, 30-minute sustained-play test. Tune patches.
15. **Optional wireless** — plug in the USB WiFi dongle, configure WiFi, implement OSC server in C++, optional UDP audio stream, optional web UI.

---

## 11. Consolidated bill of materials

### 11.1 Compute & audio platform
- 1× **Bela Gem Multi**
- 1× **PocketBeagle 2** (rev A1, AM6254)
- 1× microSD card, ≥ 16 GB, A1/A2 class
- 1× USB-C ↔ USB-C cable (data + power), 1 m

### 11.2 Buffer board
- 3× **TL074CN** (or OPA1655 + SOIC adapter)
- 3× DIP-14 IC sockets (machined pin)
- 20× 4.7 MΩ 1/4 W 1% metal-film resistors
- 10× 1 µF film caps (WIMA MKS2 or similar)
- 10× 10 µF electrolytic or non-polar (bias bypass)
- 10× 10 µF electrolytic (output AC-couple)
- 3× 100 nF ceramic (per-IC bypass)
- 1× 10 µF electrolytic (bulk bypass)
- 1× perfboard ≥ 7 × 9 cm
- 1× 30-pin 0.1" header strip
- Hookup wire 22 AWG, assorted colours

### 11.3 Pickups & mics
- 1× **Submarine Pickups SubSix** (passive magnetic hexaphonic)
- 1× neck humbucker (player's choice)
- 1× bridge humbucker (player's choice)
- 2× contact mic piezos — **AKG C411** recommended, or DIY from 27 mm piezo discs + thin shielded micro-coax

### 11.4 Controls & sensors
- 1× Adafruit NeoTrellis 4×4 (3954)
- 1× Adafruit VL53L1X breakout (3967)
- 1× Adafruit BNO055 breakout (4646)
- 1× Adafruit NeoDriver Seesaw (5766)
- 1 m of WS2812B side-emit LED strip, 60 LED/m
- 1 m of silicone diffuser channel for WS2812
- 6× STEMMA QT / Qwiic 4-pin cables, ~50–100 mm
- 2× linear pots, 10 kΩ, panel-mount, ~12 mm shaft
- 2× rotary encoders with push-button, 24 PPR (Bourns PEC11R)
- 1× 5-way blade switch (CRL or Oak Grigsby)
- 1× 2P3T or 3P3T mini slide switch (Jazzmaster-style)
- Assorted 1k–22k resistors for switch ladders

### 11.5 Power
- 1× Adafruit USB-C LiPoly Charger (4410 or current rev)
- 1× Adafruit PowerBoost 1000 Charger (2465)
- 1× Adafruit USB-C breakout (4090)
- 1× flat LiPo pouch, 5000 mAh / 3.7 V, with protection PCM
- 1× inline polyfuse, 2 A
- 1× USB-C right-angle panel-mount jack
- Silicone-jacketed 20 AWG red/black wire
- JST-PH and Faston connectors

### 11.6 Output & connectors
- 1× 1/4" TRS panel-mount stereo output jack
- 1× optional 1/4" TRS footswitch jack

### 11.7 Wireless
- 1× **Edimax EW-7811Un** USB WiFi dongle (or equivalent Linux-compatible)

### 11.8 Mounting hardware
- 12–16× M3 brass threaded inserts (heat-set)
- 4× M3 brass standoffs, 10 mm
- M3 machine screws (assorted lengths)
- M2 screws + inserts for NeoTrellis
- 3M VHB foam tape
- Clear silicone RTV (cable strain relief + contact-mic gluing)
- Hook-and-loop straps / small zip-ties for harness dressing
- 5 mm tinted acrylic sheet for back panel

---

## 12. Open items / decisions deferred

- **Main neck & bridge humbucker models** — auditioning. Confirms whether any input-level trim is needed beyond Bela's digital gain.
- **Specific donor / luthier choice** — luthier engaged; instrument spec from §2 is the brief.
- **Wood and finish choice** — luthier's discretion within the §2 envelope.
- **USB audio gadget on current Bela Gem image** — confirm enabled by default in current Bela distribution, or document enable steps from the Bela team.
- **Bela Gem Multi full input electrical spec** — datasheet pending publication. Design is robust to it via the buffer board.
- **Bela Gem per-core render API symbol names** — confirm from current Bela Gem example projects at bench-bring-up time (the API was rolled out with the Gem and has been iterating; the bench bring-up's first job is verifying current symbols).
- **Specific WS2812 strip length** — depends on the as-built body perimeter; measure when luthier delivers, order strip to fit.

---

*End of design & build document.*
