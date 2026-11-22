# HYPERTRUMPET V2 — plan

Daisy Seed 3 on the horn, bell mic, 2-cell (1S2P) battery, wireless stereo (TX powered from the instrument battery). Detail in `docs/`. Bela variant: `_attic/BELA_VARIANT/`.

## As-built update (2026-11-12)
Parts actually in use are in `BOM.md` "As-built parts (2026-11-12)"; they win over the lines below. Changes: power chain is 2 cells (1S2P, genuine 21700 5000 mAh from IMR Batteries) -> eMagTech BMS -> Twidec toggle -> TPS61088 (5.0-5.1 V) -> Seed VIN + FCOB strip (via level shifter); Xvive TX battery pads off the BMS output. The trumpet now HAS an FCOB strip (~157 px, cap ~20/255), superseding "NeoPixel strips dropped". Halls (49E) go straight to the Seed ADC: ADS1115 not used. Bell mic is a dynamic clip-on direct into Seed line in (no preamp). No MAX17048, no LDOs. Runtime at about 4.0 W: 28.5 Wh new / 22.8 Wh aged = 7.1 h / 5.7 h on 2 cells (SHARED_POWER_KIT.md). Grounds: power/LED/TX to the battery-negative star, sensors to DGND, audio to AGND.

Runtime on the 21700 5000 mAh flat-top cells (unprotected, IMR Batteries, 2 per instrument plus spares; estimates, `../SHARED_POWER_KIT.md`; table in `BOM.md`):

| Trumpet, about 4.0 W | 2 cells new (28.5 Wh) | 2 cells aged 80 % (22.8 Wh) | 1 cell new (reference) | 1 cell aged (reference) |
|---|---|---|---|---|
| Runtime | 7.1 h | 5.7 h | 3.6 h | 2.9 h |

## Locked (2026-11-12): Daisy Seed 3
- **Board: Daisy Seed 3** (same platform as VOCAL_HAT), replacing the Bela Gem Stereo. Why: only 3-4 effects run at once, all v2 DSP is C++, USB audio not needed. Bela can return if a heavy chain or USB audio is needed (`_attic/BELA_VARIANT/`).
- **Slot engine:** 4 FX slots, any library effect per slot; CPU bounded by the worst 4.
- **Shared library:** one portable C++ DSP library + one param-schema editor for trumpet and hat, PMOR V3 pattern: USB CDC serial, param schema, web editor over WebSerial.
- Power (updated 2026-11-12): **2x Molicel P45B in parallel (1S2P)**, 2 single-cell holders, like the hat. Wireless TX (stereo pair of 2, ~0.75 W each) run from the instrument battery. Trumpet load 2.7 W: 9.5 h new / 7.6 h aged on 2 cells, 4.7 / 3.8 h on 1 cell (estimates, `../SHARED_POWER_KIT.md`) [SUPERSEDED by as-built: about 4.0 W, 7.1 / 5.7 h on 5000 mAh cells]. Supersedes the earlier 1-cell ~11.7 h wired-only figure. Housing exposes the Seed's USB-C with a strain-relief cradle.
- Bell mic -> preamp -> Seed line in L (R free). Halls on ADS1115 [SUPERSEDED: Seed ADC directly]. NeoPixel strips dropped (Trellis lights) [SUPERSEDED: FCOB strip added, see As-built]. Pins: `docs/PINMAP_SEED3.md`.

## Locked (2026-11-11, still valid)
- Rig audio: hat (vocals), trumpet, guitar each send **stereo wireless** (6 links, one brand family).
  Every instrument also has a **stereo wired out** (3.5 mm); guitar adds a **6-ch wired** option.
- Sensors: v1 set, valves upgraded to analog Hall (DRV5055A2 + 6x3 mm N52 caps).
- ~~Boot + USB audio: hyperGuitar stack ported (`bela/`)~~ superseded 2026-11-12 (attic).

- Power/USB (2026-11-12): no panel USB-C breakout. The Seed 3's own USB-C is exposed through the housing (printed strain-relief cradle + right-angle/slim plug opening). Chain: cell, PCM, switch, U3V40F5 boost, Seed VIN (verify Seed 3 range) + NeoTrellis/TX/preamp rails. No ferrite/inductor in any ground or return path; supply-rail ferrites allowed.

## Docs
| File | What |
|---|---|
| docs/V1_SCRAPE.md | v1 sensors, 13-FX chain, signal-flow audit, non-portable list |
| docs/PINMAP_SEED3.md | Seed 3 pins (verified vs unverified), ADS1115, I2C map, LED recommendation |
| docs/GROUNDING_PLAN.md | GND vs GNDA, star ground, bench checklist |
| docs/PARTS_POWER_SENSORS.md | Hall, magnets, cells, holder, charger, runtime |
| docs/WIRELESS_AUDIO.md | stereo wireless options, integration, noise |
| docs/V1_PRINT_AUDIT.md | why v1 mount broke, 16 horn measurements, v2 reqs |
| _attic/BELA_VARIANT/ | Bela boot/USB stack + PINMAP_V2 (parked) |

## Open
1. Read the real Seed 3 datasheet (VIN range, 3V3 current, audio levels); I2C scan on the bench.
2. Pick wireless unit (Emory is researching; must run continuously from external 5 V/USB with its internal cell removed or bypassed; Boss WL-20 has a non-removable battery and cannot run while charging, so it is a poor fit unless bench-proven). Charger decided: one 8-bay XTAR VC8-class external charger.
3. Take the 16 horn measurements -> CAD redesign.
4. Build the bell-mic preamp (Seed has no mic bias).
5. DSP: shared C++ library with 4-slot engine; port the 11 non-portable FX; McCurdy flanger + grain delay need a C++ port (Csound not on the Seed; CC BY-NC-SA, credit).
6. New interaction model: playing-driven (valves, breath, tilt) instead of knob-driven.
7. Param schema + WebSerial editor shared with VOCAL_HAT.
