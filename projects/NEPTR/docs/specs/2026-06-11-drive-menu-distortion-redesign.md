# DRIVE Menu Redesign — 5 Distortion Voices

**Date:** 2026-06-11
**Status:** IMPLEMENTED & DEPLOYED to Pi 2026-06-11 (v1). Deltas from this spec:
- Toggle IDs are **31-35** (not 30-34 — 30 was taken by master volume).
- Final controls: CC29 expression = drive, slot encoder = tone, CC20/CC23 =
  per-pedal wildcards (see midi_handler.orc header).
- Menu-0 presets rewired: preset 1 = OVERDRIVE channel (replaces TS preset),
  preset 2 (formerly blank slot) = SOLID DIST channel (Dual High-Gain).
- Old tubescreamer.orc kept on disk as unwired legacy; distortion.orc deleted.
- Oversampling shipped in v1 for ALL pedals via shared OS4Tanh/OS4Diode/OS4Hard
  UDOs in globals.orc (4x Hermite + 4th-order Butterworth decimation).
  Measured: aliases -35..-55 dB vs naive -22..-28 dB. Pi CPU: ~50% of one core
  with three pedals + ambient stack running; idle cost zero (FX_BYPASS).
- All 5 pedals bench-calibrated to ±2 dB level match at 0.1-RMS input.
**Audience:** the engineer/model implementing the Csound effects, slot by slot
**Scope:** Replace the DRIVE menu (Menu 5) contents with 5 distinct, faithfully-modeled
dirt voices. Retire the old `powershape` distortion (instr 60) and the standalone Tube
Screamer (instr 100); both are superseded.

---

## 0. Design intent (read first)

- **Five distinct voices across the whole dirt spectrum**, minimal overlap:
  light overdrive → British crunch → dual high-gain → aggressive hard-clip → fuzz.
- **Faithful circuit emulation.** Each pedal is modeled from its real circuit
  (topology, clipping type, diode thresholds, EQ corners) — not a generic `tanh`.
  References are linked per slot in §5.
- **Everything is PEDAL/PREAMP voiced to STACK INTO A REAL AMP.**
  A separate amp/cab module is planned later. Therefore:
  - **NO cabinet/speaker simulation** in any DRIVE pedal (a cab into a real amp+cab
    = double-cab mud).
  - **NO power-amp sag / power-tube compression / presence-resonance power voicing.**
  - The "amp-style" slots (1 and 2) are **preamp-only**: cascaded gain stages + a
    tone stack. This is exactly how amp-in-a-box / preamp pedals (BE-OD, SansAmp,
    Victory pedals) stack into a clean amp.
- **Uniform control scheme** (see §3) so the whole menu feels consistent.

---

## 1. Menu layout & MIDI map

Menu 5 (DRIVE). Physical encoder order RIGHT→LEFT = slot0..slot4
(`E1, E4, E3, E2, E6`). Slot → toggle CC / own-rotation CC are fixed by the existing
hardware remap in `instr 99`:

| slot | Voice | Toggle CC | Own-rotation CC | New instr |
|------|-------|-----------|-----------------|-----------|
| 0 | **Overdrive** (Klon ⊕ Tube Screamer) | 74 | 24 | 130 |
| 1 | **Marshall Crunch** (preamp, soft clip) | 75 | 25 | 131 |
| 2 | **Dual High-Gain Preamp** (SLO ⊕ 5150) | 76 | 26 | 132 |
| 3 | **RAT × Metal Zone** (hard clip + mid sweep) | 71 | 21 | 133 |
| 4 | **Big Muff** (Triangle/Fallout fuzz) | 72 | 22 | 134 |

Instr numbers 130–134 are confirmed free (existing: 1,40,48–52,55,60,62,65,70,80,
90–93,99,100,101,119,120,150,200,211,222,233,300,401–405,500).

---

## 2. Spectrum rationale (why these 5, no overlap)

| Voice | Gain | Clipping topology | Distinct because |
|-------|------|-------------------|------------------|
| Overdrive | low–med | soft, diodes in op-amp feedback | transparent/clean-blend (Klon) ↔ mid-hump (TS) |
| Marshall Crunch | med | cascaded tube-style soft clip | British amp voicing, the only "crunch" |
| Dual High-Gain | med–high | cascaded preamp gain stages | amp lead/metal voices, blend/stack |
| RAT × Metal Zone | high | **hard** clip (diodes to ground) | only hard-clip voice; sweepable mid |
| Big Muff | high (fuzz) | 4 cascaded stages → square | scooped wall-of-fuzz, unique sustain |

Earlier drafts double-covered low gain (Klon **and** TS as separate slots) and lacked
amp character. Merging Klon+TS into one slot freed room for the Marshall crunch; the
dual high-gain preamp adds the second amp voice.

---

## 3. Control scheme (identical for all 5 slots)

The menu hardware gives each active effect four continuous controls. Dispatch uses the
existing `gkLastToggle` (CC20/CC23) and `gkLastToggle29` (CC29) trackers — each drive
effect must set BOTH on toggle-on so the shared knobs and the expression pedal route to it.

| Control | Hardware | Routed via | Meaning |
|---------|----------|-----------|---------|
| **Drive / amount** | Expression pedal, **CC29** (0–127 absolute) | `gkLastToggle29` | main gain into the clipper. Foot-sweepable dirt. |
| **Tone** | Slot's own encoder rotation (CC24/25/26/21/22) | per-slot `ENC_ROT` | tone/EQ, voiced per archetype |
| **Wildcard 1** | Prev-menu encoder rotation, **CC20** | `gkLastToggle` | effect-dependent (see §5) |
| **Wildcard 2** | Next-menu encoder rotation, **CC23** | `gkLastToggle` | effect-dependent (see §5) |

Per-slot control assignments:

| slot | Drive (CC29) | Tone (own rot) | Wildcard 1 (CC20) | Wildcard 2 (CC23) |
|------|--------------|----------------|-------------------|-------------------|
| 0 Overdrive | gain into clip | treble taper (Klon presence ↔ TS tone) | **Voice morph** Klon↔TS | **Clean blend** (Klon clean-mix) |
| 1 Marshall | preamp gain | tone-stack tilt | **Presence** (top bite) | **Tightness** (pre-clip low-cut) |
| 2 Dual Hi-Gain | master gain | tone | **A/B blend** (SLO↔5150, → series stack at extreme) | **Tightness** (low-cut) |
| 3 RAT×MZ | distortion | RAT "Filter" (LP brightness) | **Mid-sweep freq** (200 Hz–5 kHz) | **Mid level** (±boost) |
| 4 Big Muff | sustain | passive scoop balance | **Mids** (scoop ↔ mid-forward) | **Voicing** (Triangle↔Ram's Head↔Fallout) |

CC29 is absolute 0–127; map to drive with a per-pedal curve (often exponential — small
foot motion near the top = big gain change). `gkLastToggle29`-based dispatch already
exists for the GLITCH group; add the 5 drive toggle IDs to that dispatcher.

---

## 4. Architecture & integration (the 5-file pattern)

Follow the existing modular pattern exactly (see `PHASE4IMPLEMENTATION.md` §10 and any
current effect, e.g. `effects/delay.orc`). Each effect touches:

1. **`csound/effects/<name>.orc`** — one `instr N` that reads `ga<FX>_sendL/R` and
   writes `ga<FX>_outL/R`. Use `FX_BYPASS` launch/kill (effect runs only when on).
2. **`csound/globals.orc`** — add per-effect globals:
   - bypass `gk<FX>_bypass init 1`
   - params `gk<FX>_drive`, `gk<FX>_tone`, `gk<FX>_wc1`, `gk<FX>_wc2`, `gk<FX>_mix`
   - audio buses `ga<FX>_sendL/R`, `ga<FX>_outL/R` (`init 0`)
   - encoder step sizes `gkStep<FX>_*`
3. **`csound/neptrPhase4.csd`** — add `#include "effects/<name>.orc"` for the 5 new
   files; **remove** the `effects/distortion.orc` and `effects/tubescreamer.orc`
   includes (retire instr 60 and 100).
4. **`csound/signal_chain.orc`** — in `instr 1`, replace the old `DISTORTION (60)` and
   `TUBE SCREAMER (100)` blocks with the 5 new drive blocks, in this serial order
   (lighter dirt first so stacking multiple is musical):
   `Overdrive(130) → Marshall(131) → DualHi(132) → RAT×MZ(133) → BigMuff(134)`.
   Each block: set send buses from `aPrevL/R`, call `FX_BYPASS`, then mix back.
   Mix convention: because Drive is the "amount" and lives on CC29, the slot can run
   100% wet when active (`aPrevL = ga<FX>_outL`), OR honor `gk<FX>_mix` via `ntrpol`
   if a parallel blend is desired (required for Overdrive's clean blend — slot 0).
5. **`csound/midi_handler.orc`** — Menu 5 wiring in `instr 99`:
   - Toggle branches under CC74/75/76/71/72 for `gkCurrentMenu == 5`:
     `$FX_TOGGLE(gk<FX>_bypass'<toggleID>)` and on toggle-on set
     `gkLastToggle = <toggleID>` and `gkLastToggle29 = <toggleID>`.
   - Own-rotation branches under CC24/25/26/21/22 for menu 5 → `ENC_ROT(gk<FX>_tone...)`.
   - Add `<toggleID>` cases to the CC20 dispatcher → `gk<FX>_wc1`, CC23 dispatcher →
     `gk<FX>_wc2`, and CC29 dispatcher → `gk<FX>_drive`.
   - Assign 5 new toggle IDs that don't collide with existing ones (existing run low,
     ~0–25; use **30–34** for slots 0–4). Verify against `globals.orc`/`midi_handler.orc`.
   - Update the menu header comment block (lines ~30–37) to the new layout.
   - OSC feedback: mirror each param with `OSC_F`/`OSC_I` so the Kivy UI tracks state
     (follow how existing effects report; check `docs/ui.md` for addresses).

**Level-matching (mandatory).** Each pedal must be calibrated so toggling on does not
jump output level. Use open-loop makeup gain as a function of Drive (see the worked
example and rationale in `effects/tubescreamer.orc` instr 100 — keep that comment's
approach: no RMS/envelope makeup, it pumps). Document each pedal's makeup curve inline.

**Retirement.** Delete or unwire `effects/distortion.orc` (instr 60) and
`effects/tubescreamer.orc` (instr 100). Grep presets and `midi_handler.orc` for CC/instr
references to the old distortion before removing. TS character is preserved inside slot 0.

---

## 5. Per-slot circuit + DSP specifications

> Csound building blocks available: `tanh`, `powershape`, `clip`, `tablei`
> (waveshaper tables), `pareq` (shel/peak EQ), `butterhp/butterlp`, `tone/atone`,
> `dcblock2`, `lpf18`/`moogladder`. Asymmetric clipping: add a DC bias before the
> waveshaper, clip, then `dcblock2`. See §6 for the **oversampling requirement** —
> it is the single biggest factor in sounding analog vs. harsh-digital.

### Slot 0 — Overdrive (Klon ⊕ Tube Screamer), instr 130
**Refs:** ElectroSmash Klon analysis; Coda Effects Klon analysis; ElectroSmash Tube
Screamer analysis.
- **Klon path:** op-amp gain → **asymmetric** germanium soft clip (1N34A, Vf ≈ 0.3 V)
  in the feedback loop → blended against a **parallel clean** signal (the dual-gang
  gain/clean-mix is the Klon's defining feature). Voicing is treble-forward, ~hi-fi
  (real pedal runs an 18 V charge pump for headroom → model as gentle, late soft knee).
- **TS path:** non-inverting gain with **symmetric** Si diodes (1N4148) in feedback;
  the 220 nF feedback cap creates the **mid hump** (boost mids *before* clipping);
  RC tone (LP/HP blend) afterward; output level.
- **Unified model:** one waveshaper with morphable pre-emphasis and symmetry.
  - `wc1` (Voice morph) crossfades: morph=0 → Klon (flat/full-range pre-emphasis,
    asymmetric clip, treble-boost out) ; morph=1 → TS (mid-band pre-emphasis,
    symmetric clip, darker out).
  - `wc2` (Clean blend) = dry mix amount (parallel). At morph=Klon this is the
    signature clean blend; keep it useful at TS end too.
  - Drive(CC29) = gain into clipper. Tone(own rot) = output treble taper.
- DC-block after the asymmetric (Klon) stage.

### Slot 1 — Marshall Crunch (preamp), instr 131
**Refs:** Marshall JCM800 / 1959 Plexi preamp schematics; FMV (Fender/Marshall/Vox)
tone-stack analysis (e.g. ElectroSmash tone-stack, DIY tone-stack calculators).
- **Topology:** 2–3 cascaded gain stages (12AX7 common-cathode), each soft + slightly
  **asymmetric** clipping, with **interstage high-pass coupling caps** (prevents low-end
  blooming as gain rises). "Bright cap" across the first stage volume → high-freq lift
  at low gain. Marshall **tone stack** (bass/mid/treble passive network) after the
  stages. **No power amp, no cab.**
- **DSP:** chain of 2–3 `tanh`/asymmetric soft-clip stages; between stages a
  `butterhp` (~30–80 Hz, tightening) and a mild presence emphasis; then a tone-stack
  approximation (low shelf + mid dip + high shelf via `pareq`).
  - Drive(CC29) = input gain (sweeps from clean-ish to full crunch).
  - Tone(own rot) = tone-stack tilt (single-knob morph of the bass↔treble balance).
  - `wc1` Presence = post high-shelf gain.
  - `wc2` Tightness = interstage HP cutoff (raise to tighten lows for stacking).
- Stack-safe: preamp only. DC-block at the end.

### Slot 2 — Dual High-Gain Preamp (SLO ⊕ 5150), instr 132
**Refs:** Soldano SLO-100 preamp schematic; Peavey 5150/EVH preamp schematic;
"amp-in-a-box" preamp pedals (Friedman BE-OD) for the stack-into-amp voicing.
- **Topology:** TWO parallel high-gain **preamp** voices, each = 3–4 cascaded gain
  stages with aggressive interstage HP (tight low end) and a voicing EQ:
  - **Engine A "SLO":** smoother, mid-forward lead saturation.
  - **Engine B "5150":** tighter, more aggressive, slight mid-scoop, modern.
  - Blend in parallel; at the extreme of `wc1` transition to a **series stack**
    (A feeding B) for maximum gain.
- **DSP:** two chains of cascaded soft/medium waveshapers with `butterhp` between
  stages; Engine B adds a mid-scoop (`pareq` cut ~600 Hz), Engine A a mid push.
  - Drive(CC29) = master gain feeding both engines.
  - Tone(own rot) = shared output tone.
  - `wc1` = A/B blend (0=SLO, 1=5150; top of range = series stack).
  - `wc2` = Tightness (global pre-/interstage low-cut).
- Stack-safe: preamp only — **no cab, no power-amp sag.** DC-block at the end.

### Slot 3 — RAT × Metal Zone, instr 133
**Refs:** ElectroSmash ProCo Rat analysis; Electric Druid Boss MT-2 Metal Zone analysis.
- **RAT core:** very high op-amp gain whose **LM308 slew rate (~0.3 V/µs, ~40× slower
  than a TL07x)** rounds/compresses the signal — model as a **one-pole LP inside the
  feedback / before the clipper** (this slew limiting IS the RAT's feel). Then
  **symmetric hard clip**, diodes to ground (1N914/1N4148). Passive **"Filter"** LP =
  the Tone knob (turning down = darker).
- **Metal Zone layer:** add the MT-2's signature **sweepable mid** — a high-Q peaking
  filter, frequency 200 Hz–5 kHz, with adjustable gain — placed post-clip in the tone
  section.
- **DSP:** gain → slew-limit LP → `clip`/diode-waveshaper (hard, symmetric) →
  RAT filter (`tone`/`butterlp`, = Tone knob) → MZ peaking EQ (`pareq` mode 0,
  fc=`wc1`, gain=`wc2`, high Q).
  - Drive(CC29) = pre-clip gain (distortion amount).
  - Tone(own rot) = RAT filter cutoff.
  - `wc1` = MZ mid frequency. `wc2` = MZ mid gain (cut↔boost).
- **Hard clip → strong aliasing. Oversample this stage (see §6).** DC-block after.

### Slot 4 — Big Muff (Triangle/Fallout), instr 134
**Refs:** ElectroSmash Big Muff Pi analysis; Coda Effects Big Muff analysis;
bigmuffpage.com version schematics; ThorpyFX Fallout Cloud = Triangle-era 4-transistor
silicon Muff (voicing target for the "Fallout" position of `wc2`).
- **Topology (4 cascaded common-emitter stages):**
  1. Input booster (gain + HP, sets sustain).
  2. Clipping stage 1: ~25 dB gain, **soft-clip diode pair in feedback**, clamp ≈
     ±0.6 V; series caps (~1 µF) set the clipped frequency band.
  3. Clipping stage 2: identical → two consecutive clips push toward **square/hard**.
  4. Passive **tone control** = scoop network (blend of a LP and HP path, mid dip
     ~1 kHz) → output booster (recovers volume lost to clipping).
- **DSP:** input HP + gain → waveshaper stage 1 (with band-limiting HP before clip) →
  waveshaper stage 2 → Muff tone (crossfade LP-path ↔ HP-path = scoop) → output makeup.
  - Drive(CC29) = Sustain (input gain).
  - Tone(own rot) = scoop balance (dark ↔ bright through the mid dip).
  - `wc1` = Mids — add a mid-recovery peak (`pareq` boost ~700 Hz–1 kHz) to un-scoop
    (popular Muff mod); 0 = stock scoop, 1 = mid-forward.
  - `wc2` = Voicing — morph component values/clip symmetry between **Triangle**,
    **Ram's Head**, and **Fallout** (Triangle = open/scooped, Ram's Head = smoother
    mids, Fallout = tighter, slightly hotter silicon).
- Cascaded clipping → heavy harmonics. **Oversample (see §6).** DC-block after.

---

## 6. Shared DSP concerns

### 6.1 Oversampling / anti-aliasing (highest-impact item)
Naively clipping sample-by-sample at `sr = 48 kHz` generates harmonics above Nyquist
that **fold back as inharmonic aliasing** — this is what makes digital distortion sound
harsh/fizzy instead of analog. For faithful "analog" character:

- **Worst offenders (must oversample 4–8×):** RAT×MZ hard clip (slot 3), Big Muff
  cascaded clip (slot 4). Hard/square clipping has the richest high harmonics.
- **Milder (soft clip, can defer / 2×):** Overdrive, Marshall, Dual Hi-Gain — lower-order
  harmonics alias less. Still oversample if CPU allows.
- **Implementation options in Csound (pick one, document choice):**
  1. **Global higher sr** (e.g. run the orchestra at 96 kHz; `ksmps` scaled to keep
     the 128-sample quantum/block timing). Simplest correctness; costs CPU across the
     whole graph. NOTE the rate-lockstep constraint in `PHASE4IMPLEMENTATION.md` §7 —
     changing sr touches the PipeWire/quantum contract; verify before committing.
  2. **Per-stage oversampling UDO:** upsample (zero-stuff + steep `butterlp` or a
     polyphase interpolation) → apply the nonlinearity → steep `butterlp` →
     decimate. Localizes the CPU cost to the clip stages only. Preferred for a
     stack of pedals where only some need it.
  - Csound has no one-call audio-rate oversampling opcode; `setksmps` only *lowers*
    the local control rate, it does not oversample audio. Build the interpolation/
    decimation explicitly.
- **Build order:** get each pedal's *topology and controls* right at 48 kHz first
  (fast iteration), then wrap the nonlinear stages of slots 3 and 4 in the
  oversampling UDO and A/B for aliasing.

### 6.2 DC blocking
Asymmetric clipping (Klon, Marshall, Fuzz/Muff) introduces a DC offset → always
`dcblock2` each effect's output before it returns to the chain.

### 6.3 Level matching
Open-loop makeup gain per pedal, as a function of Drive, calibrated so toggle-on sits
near dry level (no jump). Do **not** use RMS/envelope-follower makeup — it pumps
sustain (see the detailed `tubescreamer.orc` instr 100 comment). Document each curve.

### 6.4 Stereo
The chain is stereo (`aPrevL/aPrevR`). Process L and R through identical, independent
instances (guitar is mono upstream, but the chain may already carry stereo content
from earlier effects — match existing per-effect convention).

### 6.5 Stack-safe guarantee
No DRIVE pedal contains a cabinet/speaker sim or power-amp model. If a future amp
module adds cab IR, these pedals feed it cleanly; into a real amp today they behave
like real stomp/preamp pedals.

---

## 7. Build, test, deploy

1. Implement **slot 0 (Overdrive)** first — simplest, validates the whole control/
   wiring path end-to-end (toggle, tone rot, CC20/CC23 wildcards, CC29 drive, OSC).
2. Then slots 1 → 2 → 3 → 4. Add oversampling to 3 and 4 after their topology is right.
3. **Local test (Mac):** `neptrPhase4.csd` `<CsOptions>` already has a laptop block
   (`-odac -iadc -+rtmidi=coremidi`). Run csound locally, drive via a MIDI controller
   or virtual port.
4. **On-Pi test:** `./scripts/sync_to_pi.sh` → `ssh gorpi7@demiurge.local
   'sudo systemctl restart demiurge'` → inject CCs with
   `ssh gorpi7@demiurge.local 'bash ~/NEPTR_phase4/scripts/claude_midi.sh cc <num> <val>'`.
   Select Menu 5 first (CC73/CC70 to navigate), then toggle (74/75/76/71/72), sweep
   tone (24/25/26/21/22), wildcards (20/23), drive (29).
5. **Verify** each pedal: no volume jump on toggle, no runaway feedback, no obvious
   aliasing on the hard-clip/fuzz voices, controls move the right parameter, UI mirrors.

---

## 8. Sources

- Klon Centaur — ElectroSmash: https://www.electrosmash.com/klon-centaur-analysis ·
  Coda Effects: https://www.coda-effects.com/p/klon-centaur-circuit-analysis.html
- Tube Screamer — ElectroSmash TS analysis (tube-screamer circuit pages).
- Marshall preamp / tone stack — JCM800 & 1959 Plexi schematics; FMV tone-stack analyses.
- Soldano SLO-100 / Peavey 5150 preamp schematics; Friedman BE-OD (stack-into-amp voicing).
- ProCo RAT — ElectroSmash: https://www.electrosmash.com/proco-rat
- Boss MT-2 Metal Zone — Electric Druid: https://electricdruid.net/boss-mt-2-metal-zone-pedal-analysis/
- Big Muff Pi — ElectroSmash: https://www.electrosmash.com/big-muff-pi-analysis ·
  Coda Effects: https://www.coda-effects.com/p/big-muff-circuit-analysis.html ·
  bigmuffpage.com version schematics
- ThorpyFX Fallout Cloud (Triangle Muff voicing) — https://thorpyfx.com/en-us/blogs/effects-pedal-guidance/why-fallout-cloud
- Fuzz Face (reference for asymmetric transistor-feedback fuzz, not used as a slot) —
  https://www.electrosmash.com/fuzz-face
- Boss DS-1 (reference for op-amp hard-clip) — https://www.electrosmash.com/boss-ds1-analysis
- Fender Pugilist (dual-engine concept) — https://www.fender.com/articles/parts-and-accessories/fiery-and-flexible-whats-behind-fenders-pugilist-distortion-pedal
- Landlord FX Whisky Chaser (3-mode high-gain reference) — Andertons product page.
