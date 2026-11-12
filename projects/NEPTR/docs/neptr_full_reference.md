# NEPTR Phase 4 — Full Reference

Last updated: 2026-07-13

This is the authoritative reference for all NEPTR functionality, written for LLMs/engineers picking up the system cold. Keep it updated whenever the system changes so future sessions don't require re-reading all the source files. When this doc and the code disagree, the code wins — then fix this doc.

---

## 1. What NEPTR Is

NEPTR is emgor's live performance instrument running on DEMIURGE (Pi 5). It is a guitar effects processor for surf rock and experimental performance, feeding a **real amplifier** (so: pedal/preamp voicing everywhere, never cab/power-amp simulation). The Pi is the runtime; the Mac folder `NEPTR phase4/` is the source of truth (the old `DEMIURGE_OS/neptr/` copy was merged in and deleted 2026-06-10).

**Production mode: bypass.** csound runs directly on raw ALSA — no PipeWire, no JACK, no demiurge-sink, **no ChucK** (synthi.ck is commented out of `live.conf`'s chain). `bypass = on` in `live.conf` is the production default; do not flip it without explicit reason.

**Boot path:**
```
/boot/firmware/demiurge.conf → launch = /home/gorpi7/NEPTR_phase4/live.conf
demiurge.service ExecStart = /usr/local/bin/demiurge-mode-select.sh
  ├─ reads live.conf bypass=on
  ├─ stops pipewire + wireplumber
  └─ exec csound -+rtaudio=alsa -odac/-iadc plughw:CARD=USB,DEV=0
                 -r 48000 -B 2048 -b 256 --realtime  (RT prio 74)
```

**Audio graph:** Scarlett Mic 1 / Inst 2 → ALSA capture → csound instr 1 (mono-summed, stereo chain) → ALSA playback → Scarlett out → real amp.

Apply `.orc`/`.csd` changes: `./scripts/sync_to_pi.sh` then `ssh gorpi7@demiurge.local 'sudo systemctl restart demiurge'`. (Sudo needs a password/tty. Workaround without a terminal: `kill -9 $(systemctl show demiurge -p MainPID --value)` over ssh — `Restart=on-failure` brings it back with a fresh csound.)

**Engine header (globals.orc):** `sr = 48000`, `ksmps = 128`, `nchnls = 2`, `0dbfs = 1`. Latency ≈ 7 ms RTT at period 128 (deployed 2026-06-11; was 256 / ~12 ms). The period comes from `quantum =` in `~/demiurge/live.conf` — the launch target — NOT this repo's live.conf, and the `.csd`'s CsOptions `-b/-B` are ignored in bypass; ksmps must stay = quantum and ≤ every pvsanal hop (256). Full rules: DEMIURGE_OS gotchas Q7f. Next step down is 64/64 (~4 ms) after a worst-preset soak test.

---

## 2. Physical Hardware (Teensy)

### Encoders and the remap

7 encoders. The Teensy firmware sends **physical** CCs; `instr 99` in `midi_handler.orc` remaps them to **logical** CCs that all dispatch tables use. Everything below §2 in this doc speaks LOGICAL CCs.

| Physical (firmware) | btn/rot CC | → Logical role |
|---|---|---|
| E1 | 70 / 20 | slot0 toggle (→74) / slot0 rotation (→24) |
| E4 | 73 / 23 | slot1 toggle (→75) / slot1 rotation (→25) |
| E3 | 72 / 22 | slot2 toggle (→76) / slot2 rotation (→26) |
| E2 | 71 / 21 | slot3 toggle (71) / slot3 rotation (21) — unchanged |
| E6 | 75 / 25 | slot4 toggle (→72) / slot4 rotation (→22) |
| E5 | 74 / 24 | MENU NEXT button (→73) / tertiary param (→23) |
| E7 | 76 / 26 | MENU PREV button (→70) / secondary param (→20) |

Logical scheme: menu prev=70, next=73 (button presses, val 127). Slot toggles CC74/75/76/71/72 = slots 0–4 (physical right→left). Slot rotations CC24/25/26/21/22. Secondary param CC20, tertiary CC23, expression pedal CC29 (absolute 0–127). Rotations: val>64 = CW increment, val<64 = CCW decrement.

**Gotcha:** `scripts/claude_midi.sh` injects PHYSICAL CCs — e.g. menu-next is `cc 74 127`, slot0 toggle is `cc 70 127`.

### Expression pedal (CC29)

Absolute. Dispatched by `gkLastToggle29` — a sticky pointer updated only when toggling a CC29-using effect ON. Preset REVERB/AMBIENT routes it to volume swells (`gkLastToggle29 = 1` or master vol 30); the five DRIVE pedals route it to their drive amount.

---

## 3. Menu System — 17 menus

> Menu cycle ORDER (and which menus are hidden) is data: `neptr/menus.json`, edited at the web UI page `#/neptr/menus`, generated into `csound/handler/gen/menu_cycle.orc` and pushed live over OSC (`/mc`, `/mc_n`). Menu numbers are never renumbered. See `docs/menus.md`.

Navigate CC70 (prev) / CC73 (next), wrapping. OSC `/menu` (int 0–16).

> **Current numbering (2026-10-05):** the list below is the historical 17-menu table; see `docs/menus.md` and `MENUS` in `neptrPhase4_UI.py` for the live one: 0..18 effects, 19 UTILITY, **20 NORRIS** (SoundMagic rack, touch-only page), **21 HYPERGUITAR** (conditional, only while the guitar is present). New always-on menus go before the conditional block.

```
0 PRESETS   1 VOLUME   2 SPECTRAL   3 PITCH    4 FILTERING   5 MOD
6 DRIVE     7 PREAMP   8 TIME       9 GLITCH   10 LOOPER     11 CHASE BLISS
12 NAM      13 IR VERB 14 BASS      15 SYNTH   16 AIR
```

CHASE BLISS sits at **11, directly after the LOOPER (10)**, because BLOOPER is a
granular modifier *of* the looper's loops — the two menus are neighbours by
design (2026-07-13; NAM/IR/BASS/SYNTH/AIR each shifted up one).

(SPECTRAL, PREAMP, LOOPER and NAM were added since the 8-menu era; IR VERB and
BASS were added 2026-07-13 with the nine-effect batch; SYNTH and AIR were added
2026-07-13 with the ten-effect batch; CHASE BLISS was added 2026-07-13 and
gathers the three Chase Bliss ports — see §5.17. Because nav wraps, adding menus
at the end stays cheap — CHASE BLISS is one press *left* from PRESETS.
Authoritative table: midi_handler.orc header + `MENUS` in neptrPhase4_UI.py; the
wrap modulus and `MENU_COUNT` are cross-checked by `tests/test_menu_consistency.py`.)

**Menu-consistency guarantee.** `tests/test_menu_consistency.py` parses the UI's
`MENUS`/`EFFECT_PARAMS`/`STEP_LABELS` and the handler `.orc` files and fails the
build if they drift: every menu code must have a param row, every param row must
be reachable from a menu, toggle IDs must be a bijection with their bypass
globals, every `/<code>_state` OSC send must be routed by the UI, this doc's
toggle registry must list every toggle the handler fires, and the menu count must
match the handler's wrap modulus. Run it after ANY menu/effect/OSC edit.

---

## 4. Menu 0 — PRESETS

Menu 0 is now **mode-switched** (`gkPresetBank`, OSC `/preset_bank`): bank 0 = the surf gig bank below (untouched, gig-locked), bank 1 = the bass-rig bank (PRE LIGHT · PRE HEAVY · TOTAL GLITCH · BASS DIST · H90 WAMMY — see `docs/superpowers/specs/2026-06-22-bass-preset-menu-design.md`). The UI is mode-blind and draws whichever bank Csound reports. Surf-bank preset activation also force-enables the MAIN PREAMP (toggle 39) since the Broadcast preamp is no longer always-on.

Surf bank, physical layout left→right: **OVERDRIVE · SOLID DIST · PHASER · TREMOLO · REVERB**. OD, SOLID DIST and REVERB are mutually exclusive presets; PHASER and TREMOLO are stackable modifiers (cleared on any preset select). Pressing an active preset again → dry. `gkCurrentPreset`: −1 dry, 1 overdrive, 2 solid-dist, 3 ambient/reverb (plus bass-bank values 10–14). (Preset 0 "CLEAN" is retired; its dead +10% boost block was removed from signal_chain.orc 2026-07-13.)

| Slot | CC | Button | What it does |
|---|---|---|---|
| s4 (left) | 72 | **OVERDRIVE** preset | vib + spring delay base + OVERDRIVE (instr 130) at drive 0.45; slot rotation CC22 sweeps `gkOD_drive` 0–1 live |
| s3 | 71 | **SOLID DIST** preset | vib + spring delay base + DUAL HIGH-GAIN (instr 132) drive 0.6 blend 0.5; CC21 sweeps `gkDHG_drive` |
| s2 | 76 | **PHASER** toggle | 4-stage MXR-style, warm/analog voicing |
| s1 | 75 | **TREMOLO** toggle | amplitude trem at `gkTremRate` |
| s0 (right) | 74 | **REVERB / AMBIENT** preset | vib + spring delay base + spring reverb (FB 0.92, mix 0.70) + expression pedal = volume swell |

All presets share the base: vibrato from `giPresetVib*` (freq 0.5796 Hz, depth 0.001265 s) + spring delay from `giPresetDel*` (~60.6 ms, FB 0.62, wet 0.108). These `giPreset*` constants in globals.orc are DISTINCT from the manual-toggle defaults. Every preset/dry transition resets `gkMasterVol = 1`.

**Master volume (toggle ID 30, `gkMasterVol`):** an inline gain at the FRONT of the chain (stage 0.7) so expression-pedal swells feed the reverb/delay and the tails bloom (a post-reverb volume would chop tails). Unity when no preset routes it.

### Per-preset output trim (2026-07-13)

Presets are voiced at different gains (the ambient preset already carries a 2.6× `gkAmbBoost`); trimming them to match by ear used to mean editing the orchestra. Now each preset carries its own stored output gain.

- **Table:** `giPresetTrim` (globals.orc) — 16 linear multipliers, indexed by `gkCurrentPreset` (surf 1/2/3, bass 10–14). Default **1.0** (unity), so the trim is a no-op until you move it. Dry (`gkCurrentPreset = -1`) is always unity.
- **Applied:** signal_chain.orc stage 0.7b, multiplied into the master stage right after `gkMasterVol` × `gkAmbBoost`. It rides the swell rather than sitting after it, so the reverb still blooms.
- **Edited:** **menu 0 ONLY, CC20** (the secondary-param encoder). Each detent nudges the ACTIVE preset's entry by **0.5 dB**, clamped to **±6 dB** (×0.501 … ×1.995). Menu 0 has no `gkLastToggle`-driven CC20 target, so this steals nothing — leave menu 0 and CC20 goes back to being the last-toggled effect's P2.
- **OSC:** `/preset_trim` (float, the active preset's multiplier).
- **SESSION-ONLY.** The table lives in RAM: trims survive preset switches but **not a restart**. Cross-boot persistence (writing the table to `presets/` or `phase4.conf` at shutdown, reloading at boot) is a deliberate FUTURE item — not wired.

---

## 5. Menus 1–16 — effect slots and encoders

Empty cells = unassigned. **Encoder rotations follow the v2 takeover scheme**
(2026-06/07, `docs/encoder_mapping_v2.md`): ALL seven rotations drive the
last-toggled effect's params (P1 dry/wet, P2 rate, P3 feedback, P4+ the rest) —
the per-slot "rotation = its own dry/wet" column of the old scheme is gone.
The expression pedal (CC29) dispatch is unchanged (§7).

### Menu 1 — VOLUME
| Slot | CC | Effect | Toggle ID |
|---|---|---|---|
| s0 | 74 | Volume Pedal (inline) | 1 |
| s1 | 75 | Ring Modulator | 11 |
| s2 | 76 | Leslie | 10 |
| s3 | 71 | Attack Shaper (v2 POG swell, 2026-07-03) | 2 |
| s4 | 72 | TAPE machine (Gen Loss-style; replaced Dropout) | 21 |

### Menu 2 — SPECTRAL (added 2026-06; owns the pvs-based processors)
| Slot | CC | Effect | Toggle ID |
|---|---|---|---|
| s0 | 74 | FORMS (clean formant, pvswarp) | 28 |
| s1 | 75 | Formant (legacy double-pvscale) | 5 |
| s2 | 76 | Spectral Delay | 16 |
| s3 | 71 | BIN REDU — spectral contrast exponentiation (instr 121, bincrush.orc) | 38 |
| s4 | 72 | BCRUSHER — spectral bin coarsener (instr 122, bincrusher.orc) | 44 |

**SOUNDMAGIC rack (2026-10, Michael Norris's SoundMagic Spectral re-implemented; spec `docs/specs/2026-10-04-soundmagic-spectral-rack-design.md`, contract `csound/spectral/CONTRACT.md`).** Menu 2 is the plain five-slot menu above again; the rack lives on its own touch page, **menu 20 NORRIS** (no engine slot branches: its five slot buttons do nothing). The 19 rack effects (fx 0..18, toggle ids 75..98) are driven from the touch page over OSC 7770 (`/st fx on`, `/sp fx p val`), not slot buttons. Tapping an effect sets `gkLastToggle = 75+fx` (and `gkLastToggle29`), so the encoders step its hero params (CC24/25/26/21/22/20/23 = P1..P7, one generic branch in each `rot_*.orc`) and the expression pedal writes its pedal param (one range branch in `expr_cc29.orc`). All params live in `giSMP` (idx = fx*32+p), on flags in `giSMon`, persisted dual-file in `$STATE_DIR/sm_*.ftab`, snapshotted per preset in `giSMPBank`/`giSMonBank`. Engine: `spectral/sm_rack.orc` (instrs 241–248 = one pvsanal/pvsynth group per FFT size, ascending; 249 = control), signal_chain stage 11.8 hands the chain over via `gaSMChainL/R` (one ksmps of bus latency; bit-exact dry when no effect is on). Out to UI on 8000: `/sm_state fx on`, `/sm_p fx p val` (drained change-scan, two sockets total). Credit: algorithms and parameter sets are Michael Norris's (michaelnorris.info); the code is ours.

### Menu 3 — PITCH
| Slot | CC | Effect | Toggle ID |
|---|---|---|---|
| s0 | 74 | H90 (sinusoidal model — moved from s4 2026-07-13) | 26 |
| s1 | 75 | Old PS (legacy 4-grain) | 3 |
| s2 | 76 | GPS (granular) | 4 |
| s3 | 71 | Pitch Fork (mincer PV) | 27 |
| s4 | 72 | *(empty)* — ONWARD moved to CHASE BLISS s1 (2026-07-13) | — |

Semitone amounts for the four pitch shifters live on CC20 (and CC29) via lastToggle
dispatch — see §6/§7.

### Menu 4 — FILTERING
| Slot | CC | Effect | Toggle ID |
|---|---|---|---|
| s0 | 74 | LOSSY — codec degradation (instr 123) | 46 |
| s1 | 75 | RESONATOR — sympathetic pipe bank, custom trails bypass (instr 157) | 47 |
| s2 | 76 | WAH (moved from s0 2026-07-13) | 22 |
| s3 | 71 | AutoMoog (env-follow filter — moved from s1) | 14 |
| s4 | 72 | *(empty)* — BLOOPER moved to CHASE BLISS s0 (2026-07-13) | — |

LOSSY (s0) and RESONATOR (s1) **stay here** — they were considered for the CHASE
BLISS menu and deliberately kept in FILTERING.

### Menu 5 — MOD
| Slot | CC | Effect | Toggle ID |
|---|---|---|---|
| s0 | 74 | Vibrato | 6 |
| s1 | 75 | Chorus | 7 |
| s2 | 76 | Phaser | 8 |
| s3 | 71 | Tremolo v2 (4 modes SINE/OPTO/BIAS/HARM; still a surf-preset modifier) | 43 |
| s4 | 72 | Flanger | 9 |

### Menu 6 — DRIVE (2026-06-11 redesign — see §9)
| Slot | CC | Pedal | Toggle ID | Instr |
|---|---|---|---|---|
| s0 | 74 | OVERDRIVE / TUBESCREAM (Klon ⊕ TS) | 31 | 130 |
| s1 | 75 | MARSHALL crunch preamp | 32 | 131 |
| s2 | 76 | RAT × METAL ZONE (swapped with DS-1K 2026-06-11) | 34 | 133 |
| s3 | 71 | DS-1K / DUAL HIGH-GAIN (SLO ⊕ 5150) | 33 | 132 |
| s4 | 72 | BIG MUFF (Triangle/Fallout) | 35 | 134 |

### Menu 7 — PREAMP (added with the bass rig)
| Slot | CC | Effect | Toggle ID | Instr |
|---|---|---|---|---|
| s0 | 74 | MAIN PREAMP (BroadcastPre — no longer always-on, see §10) | 39 | inline |
| s1 | 75 | BASS PREAMP LIGHT (radio pair — mutually exclusive with HEAVY) | 40 | 160 |
| s2 | 76 | BASS PREAMP HEAVY | 41 | 161 |
| s3 | 71 | BASS DISTORTION | 42 | 162 |
| s4 | 72 | COMPRESSOR — optical/FET comp + downward-expander gate (2026-07-13) | 57 | 180 |

(NOTE: the PREAMP row in the UI's `MENUS` is listed **s0 → s4**, the reverse of
every other menu. Pre-existing quirk, left alone — COMPRESS is the last entry,
which is s4/CC72, matching the handler.)

### Menu 8 — TIME
| Slot | CC | Effect | Toggle ID |
|---|---|---|---|
| s0 | 74 | Delay | 15 |
| s1 | 75 | *(empty)* — HABIT moved to CHASE BLISS s2 (2026-07-13) | — |
| s2 | 76 | Reverb | 19 |
| s3 | 71 | Granular | 18 |
| s4 | 72 | MOOD | 17 |

HABIT now lives in **menu 16 CHASE BLISS s2** (§5.17) — its behaviour is unchanged;
only the menu home moved. Kept here for reference:
HABIT (instr 64, `effects/habit.orc`; spec `docs/specs/2026-06-11-habit-echo-collector-design.md`,
manual `docs/reference/Habit_Manual_Chase_Bliss.pdf`): ~175 s always-recording mono
memory ring + AUTO-scan playhead → ~87 s echo with dual-tap click-free SIZE
(50 ms–60 s, never pitch-warps) and tape-aged feedback (wow/flutter + HP80/LP4500 +
tanh IN the loop). CC29 expression = scan (how often + how deep the auto-scanner digs),
CC20 = size, CC23 = repeats (max = stable infinite loop). Memory clears on re-engage.

### Menu 9 — GLITCH (press/hold/release semantics)
| Slot | CC | Effect | Toggle ID |
|---|---|---|---|
| s0 | 74 | IMPULSE SYNTH — per-pick synth voice (instr 156). **PLAIN toggle**, NOT in the glitch chance/BPM group | 48 |
| s1 | 75 | BBcut | 20 |
| s2 | 76 | WGuide | 23 |
| s3 | 71 | SqMod | 24 |
| s4 | 72 | FreqShift | 25 |

(The glitch quartet shifted one slot left 2026-07-13 when IMPULSE took s0;
the release handlers moved to the new CCs with them.) Glitch buttons use `GLITCH_PRESS`/`GLITCH_RELEASE` macros: press (127) forces the effect ON at 100% chance while held; release (0) classifies click (<0.3 s, `giGLITCH_holdThresh`) = toggle vs hold = off. **Always send val 0 to release** or `_held` stays 1 and the effect fires at 100% forever. Shared: CC29 → `gkGLITCH_chance`, CC20 → `gkGLITCH_bpm` (also slaved to the DEMIURGE clock ch16 CC119). FreqShift effective probability = chance × 0.2.

### Menu 10 — LOOPER (**in progress**, branch `feature/looper-pedal`)
| Slot | CC | Channel | Instr |
|---|---|---|---|
| s0 | 74 | Loop CH1 | 210 |
| s1 | 75 | Loop CH2 | 211 |
| s2 | 76 | Loop CH3 | 212 |
| s3 | 71 | Loop CH4 | 213 |
| s4 | 72 | Loop CH5 | 214 |

Five independent end-of-chain loop channels (`effects/looper.orc`, instrs
210–214 + helper 209, buses `gaLpIn`/`gaLpOut`). NOT toggle-ID effects: each
slot button is a press-fired state machine (rec → close/play → overdub on
0.4 s hold, resume, double-press clear — see the "LOOPER HOLD WATCHER" in
instr 99). Loop length is power-of-two quantized against the first loop. State
per channel: `gkLpState<n>` (0 empty · 1 rec · 2 play · 3 overdub · 4 stopped),
speed index `gkLpSpIdx<n>`, position OSC `/lp<n>_pos`. Details are still
settling — read looper.orc + the looper blocks of midi_handler.orc before
touching anything.

### Menu 11 — NAM (Neural Amp Modeler — toggle ID 37, end of chain)
| Slot | CC | Amp row | Row idx |
|---|---|---|---|
| s0 | 74 | CRUNCH (JC-120 → Plexi) | 4 |
| s1 | 75 | TWEED (edge → breakup) | 3 |
| s2 | 76 | FENDER (Super Reverb → Princeton) | 2 |
| s3 | 71 | DELUXE (clean → light drive) | 1 |
| s4 | 72 | AC30 | 0 |

One NAM instance (`effects/nam.orc` + the `NAMProcess` plugin opcode from
`csound/nam/`, always running at end of chain; bypass = 100%-dry blend). Slots
select a ROW of amp profiles; the row's variant is encoder-selected
(`gkNAMRowSel[]`, OSC `/nam_sel<r>`), wet = `gkNAMWet`. Profile files live in
`~/demiurge/profiles/nam` (`<row+1><variant+1>_name.nam`); the UI reads amp
names from the filenames.

### Menu 12 — IR VERB (added 2026-07-13; convolution reverb, instr 240)
| Slot | CC | IR row | Toggle ID |
|---|---|---|---|
| s0 | 74 | SPRING (row 0) | 49 |
| s1 | 75 | PLATE (row 1) | 50 |
| s2 | 76 | HALL (row 2) | 51 |
| s3 | 71 | CHAMBER (row 3) | 52 |
| s4 | 72 | WOOD RM (row 4) | 53 |

NAM-style wiring: one convolution engine (`effects/ir_reverb.orc`, instr 240,
instrs 241–249 reserved); slot button = row on/off (`gkIRActiveRow`/`gkIRBypass`,
pressing the active row = off); slot ROTATION browses that row's 5 variants
(`gkIRSel[r]`, wraps, OSC `/ir_sel<r>` — the UI reads IR names from the
filenames like NAM). Per-row dry/wet `gkIRRowMix[r]` = P1 on the row's home
encoder; CC20 = `gkIRDamp` (wet tone), CC23 = `gkIRLevel` (wet trim). Wet-path
predelay ≈ 42.7 ms (ftconv partition 2048) + 2.7 ms bus. IR wavs:
`csound/ir/*.wav`, named `<row+1><variant+1>_label.wav`.
**Deploy:** `rsync csound/ir/*.wav gorpi7@demiurge.local:~/demiurge/ir/`
(engine default `/home/gorpi7/demiurge/ir`, override with the `IR_DIR`
orchestra macro; UI env `NEPTR_IR_DIR`, default `~/demiurge/ir`).

### Menu 13 — BASS (added 2026-07-13; bass rig in one place)
| Slot | CC | Effect | Toggle ID | Instr |
|---|---|---|---|---|
| s0 | 74 | BASS PREAMP LIGHT (same toggle as PREAMP menu s1) | 40 | 160 |
| s1 | 75 | BASS PREAMP HEAVY (mutually exclusive with LIGHT) | 41 | 161 |
| s2 | 76 | BASS DISTORTION (same toggle as PREAMP menu s3) | 42 | 162 |
| s3 | 71 | OCTAVER — OC-2-style analog-voiced sub (rotation = SUB level) | 54 | 170 |
| s4 | 72 | ENV FILTER — boutique bass envelope filter | 55 | 171 |

BPL/BPH/BDS are the SAME effects/toggles as in the PREAMP menu — menu 13 is a
second front door, not new instances.

### Menu 14 — SYNTH (added 2026-07-13; pitch-tracked voices + the tone stack that feeds them)
| Slot | CC | Effect | Toggle ID | Instr | Chain |
|---|---|---|---|---|---|
| s0 | 74 | TRACK SYNTH — ptrack-follower 3-osc synth voice (saw + detuned square + sub → moogladder w/ per-onset AD; amplitude slaved to the input envelope). Claims CC29 → cutoff | 61 | 184 | 19.3 |
| s1 | 75 | PLL FUZZ — comparator/divider/PLL fuzz (Data Corrupter-style: master square + ÷2 sub + a simulated PLL steered to ×interval; slow lock = glitchy re-locks) | 62 | 185 | 14.3 |
| s2 | 76 | TONE EQ — amp-style tone stack (mid peak w/ sweepable freq, tilt, low/high shelves; 0.5 = flat on every band) | 58 | 181 | 11.5 |
| s3 | 71 | — (free) | — | — | — |
| s4 | 72 | — (free) | — | — | — |

TONE EQ lives here rather than in DRIVE because DRIVE is full at 5 and PREAMP's
only free slot went to COMPRESSOR. It is a general-purpose EQ, not a synth — it
sits next to the two tracked voices because they are the things that most want
re-voicing, and it is the only stage in the chain with **no mix control on
purpose** (an EQ dry/wet blend is a comb-filter foot-gun): it is wired straight
through. TRACKSYNTH and PLLFUZZ are both prime bass-bank material (see the bass
preset spec) — the tracker clamp reaches 55 Hz and PLLFUZZ's divider goes an
octave under that.

### Menu 15 — AIR (added 2026-07-13; space, texture and feedback)
| Slot | CC | Effect | Toggle ID | Instr | Chain |
|---|---|---|---|---|---|
| s0 | 74 | FEEDBACKER — pitch-tracked sustained-feedback overtone voice (fundamental / +oct / +oct+5th / +2oct, bloom envelope, vibrato; swells UP as the string decays, FreqOut-style). Claims CC29 → intensity push | 63 | 186 | 2.5 |
| s1 | 75 | DIFFUSION — the front half of a reverb, amputated: 6 LFO-modulated Schroeder allpasses with NO recirculating tank. Texture, not space (dies in <400 ms). DEPTH scales times, gains AND live stage count together | 64 | 187 | 18.7 |
| s2 | 76 | *(empty)* — SHIMMER **REMOVED 2026-07-13** at the user's call: a shimmer is achievable by routing the existing pitch shifter into reverb. Toggle 65 / instr 188 are FREE; `effects/shimmer.orc` archived in `effects/_retired/` | — | — | — |
| s3 | 71 | — (free) | — | — | — |
| s4 | 72 | CHAIN FB — feeds the chain back into itself: captures pre-reverb, runs gain → tanh → tone tilt → dcblock → hard ceiling, re-injects post-drives. Every effect in the span becomes part of a playable feedback instrument | 60 | 183 | 11.4 + 22.9 |

**CHAIN FB is the odd one out**: it is not a link in the chain, it *wraps* a
span of it, so it has TWO tap points in signal_chain.orc (inject at 11.4,
capture at 22.9). The injection is gated by a 10 ms `portk` ramp — this is
REQUIRED, not defensive: `ga` buses hold their last block after `turnoff2`, so
without the gate the loop would keep ringing a stale block after bypass.
Boundedness is proven in `chainfb.orc`: the last op on the return bus is
`limit ±kCeil` with `kCeil ≤ 0.95`, scaled by `kRet ≤ 1`, so the injected signal
can never exceed 0.95 peak no matter what the loop grows into.

### Menu 11 — CHASE BLISS (added 2026-07-13; the three Chase Bliss ports, gathered)
| Slot | CC | Effect | Toggle ID | Instr | Moved from |
|---|---|---|---|---|---|
| s0 | 74 | BLOOPER — **granular modifier of the LOOPER's loops** (plain toggle) | 56 | 208 | FILTERING s4 |
| s1 | 75 | ONWARD — **time-domain dynamic sampler** (complete v2 rebuild; was a spectral freeze pad that latched near-silence). Claims CC29 = **ERROR**. `gkONW_gate` is GONE; new params blend/size/err/type | 45 | 155 | PITCH s4 |
| s2 | 76 | HABIT — echo collector (~175 s ring). **Button: click = on + recording · click again = freeze/unfreeze the buffer · double-click = off** | 36 | 64 | TIME s1 |
| s3 | 71 | **MOOD II** — REWORKED 2026-07-13: SLIP-only auto-sampler, plain $FX_BYPASS toggle (the old always-on micro-looper + RECORDING/PLAYING/OVERDUBBING gesture grammar is GONE — it was the root cause of three live bugs: toggling off didn't silence it, sweeping SIZE clicked, and stale audio kept replaying). Ring buffer for LIVE INPUT + read-only sum of the LOOPER's (menu 10) currently-playing loops, both run through one variable-rate CLOCK engine (write-side ZOH decimation down only, read-side varispeed both up and down). CLOCK is quantized to a 13-rung tonal ladder (root/5th/octave stacking, mirrored up+down — see mood2.orc header); a new GENERATIVE knob (P4) free-runs the clock to random non-home rungs at up to ~50% duty. Params: Mix (P1), Clock (P2), Length (P3, SLIP window), Generative (P4). Claims CC29 = **CLOCK scrub** (now bipolar, full ladder). Separate effect from the original MOOD (t17, instrs 401-405, TIME menu — untouched). The v2 REVERB mode the user liked was extracted verbatim into `effects/grainverb.orc` (proposed toggle 65 / instr 188 — see below); NOT wired into any menu yet | 66 | 189 | *(new)* |
| s4 | 72 | — (free) | — | — | — |

**GRAINVERB** (`csound/effects/grainverb.orc`, NOT wired into menus/chain — built, benched, ready "for later" per the user): the 6-tap-cluster + 4-stage-Schroeder-allpass reverb algorithm extracted unmodified from MOOD II v2's REVERB mode. Self-contained globals (`gkGRV_*`/`gaGRV_*`/`giGRV_*`), proposed toggle 65 / instr 188 (both confirmed free by grep, same SHIMMER-vacated slots MOOD II's own registry row references). Params: Mix (P1), Time (P2, size/decay), Smear (P3, MODIFY equivalent — discrete cluster to washed-out tail). See the file's own INTEGRATION NOTES header for the exact signal_chain.orc / handler_menus.orc / UI wiring spec.

**These effects MOVED — they were not copied.** Each one now has exactly ONE menu
home. The vacated slots (FILTERING s4, PITCH s4, TIME s1) are deliberately left
empty. **LOSSY and RESONATOR stay in FILTERING** — they were considered for this
menu and deliberately kept where they were.

**Behaviour, params, toggle IDs and instr numbers are COMPLETELY UNCHANGED** by
the move. The one thing a move *must* change is the **P1 "home rotation"** — P1
is by definition the rotation of the encoder you toggled, so it follows the slot
(§encoder_mapping_v2). P2 (CC20), P3 (CC23) and the CC29 expression mappings are
dispatched on `gkLastToggle`, **not** on the slot, so they are untouched:

| Effect | P1 rotation before → after | Other rotations |
|---|---|---|
| BLOOPER 56 | CC22 (s4) → **CC24** (s0) — granular level | P2 CC20 motion · P3 CC23 **SCATTER** · P4 CC25 grain size · P5 CC26 grain character. CC29 = manual chaos push. No raw press/release edge any more — it is a plain toggle |
| ONWARD 45 | CC22 (s4) → **CC25** (s1) — pad level | none |
| HABIT 36 | CC25 (s1) → **CC26** (s2) — mix | blend stays CC22 (never derived from its old home; no collision with the new one) |

**BLOOPER IS A GRANULAR MODIFIER OF THE LOOPER** (rearchitected 2026-07-13; it
used to be a standalone looper pedal of its own, with its own 246 MB buffer,
layers and undo — all of that is gone).

* **The LOOPER** (menu **10**) — the 5-channel `lp0..lp4` multitrack looper,
  `csound/effects/looper.orc` + `handler_looper.orc`, instrs 209–214. It records
  and plays your loops. **Its behaviour is completely unchanged** and it is the
  only thing that owns a loop buffer.
* **BLOOPER** (menu **11**, s0) — toggle **56**, instr **208**, `gkBLP_*`,
  `effects/blooper.orc`. It owns **no buffer, no recording, no transport**. When
  you switch it on it *re-plays the loops the LOOPER already recorded*, through
  one independent monophonic grain stream per looper channel.

**Routing (no doubling).** `signal_chain.orc` crossfades: BLOOPER off → you hear
`gaLpOut` (the looper's own playback). BLOOPER on → you hear `gaBLP_Loop`
(BLOOPER's granular render of the same buffers) *instead*. It is a **replace,
never a sum**, so the loops are never heard twice.

**SCATTER = 0 is an exact null.** The grain stream uses Hann windows at a 50%
overlap hop, which is a COLA window pair (consecutive windows sum to exactly
1.0). At scatter 0 the displacement is 0, the per-grain pitch offset is 1.0 and
the grain length is constant, so both live windows read the same sample and sum
to unity: the output *is* contiguous loop playback. Bench-measured correlation
against looper-only playback: **1.00000, residual 0.109 %**.

**Why instr 208.** Instruments run in ascending order, and 208 < 209–214, so
BLOOPER sees `gkLpRdPos{N}` exactly as the looper instruments are about to use it
this cycle. Its grains therefore start from the identical read-pointer base as
the looper's own playback — which is what makes the scatter-0 null sample-exact.

**looper.orc and handler_looper.orc are NOT modified.** BLOOPER only READS
`gkLpState/Len/RdPos/SpIdx{N}`, `giLpSpeeds` and the `giLpBuf{N}L/R` ftables. All
routing lives in `signal_chain.orc`.

**Modulation design — grain EVENTS, not a granular cloud.** The loop plays back
**normally**; that is the baseline and the dominant state. Every so often a
**grain event** is thrown: a chunk of the loop, grabbed from elsewhere in the
timeline and played at a different speed (sometimes backwards), momentarily
displaces normal playback, then drops right back into the loop. **Punctuation,
not a wash.** This is deliberately *not* classical granular synthesis: the loop
is never reconstructed out of a dense grain stream.

**There is NO continuous pitch modulation anywhere.** No pitch LFO, no vibrato,
no per-grain detune. The only thing that changes pitch is a grain event's own
**discrete** playback speed — a per-event choice, not a modulator riding on the
sound. Bench: during normal (non-event) playback the pitch matches the
un-modulated loop to **0.000 Hz**.

**The loop always wins (duty cap).** Event coverage is capped at `BLP_MAXDUTY`
= 0.45, so even at the knob's maximum, grain events occupy at most ~45 % of the
timeline and you are hearing normal loop playback more than half the time. At
MOD = 0, no events fire at all. After an event of length L the next gap is
`G = L × (1 − duty) / duty`, randomised ×0.7–1.6 so the punctuation never locks
into a rhythm. Bench at max: events cover **42.6 %**, normal playback 57.4 %.

**Events are generative.** Length is a log-uniform draw (×1/3…×3) around a
centre set by the SIZE knob — mostly moderate, with big ones turning up now and
then (bench: 85–720 ms, median 267, std 158). Speed is a discrete per-event draw
from **{1×, 0.5×, 2×, −1× reverse}** multiplied by the looper's own varispeed
(1× is the most common). Position is a continuous random displacement around the
playhead; CHARACTER sets how far it can throw (NEAR ±10 % / THROW ±35 % / FAR
±50 % of the loop). Nothing snaps to the loop length, a subdivision or the BPM
clock — it is free-running, never grid-quantised.

**Each loop modulates independently.** The per-channel engine is a UDO, so each
of the 5 call sites owns its own density-drift LFO and its own random draws — no
shared seed, no shared phase. Bench: pairwise |xcorr| of the 5 event streams,
mean **0.058**, worst 0.133.

**The expression pedal CUES events.** Pushing it raises the event density *and*
throws a grain immediately on the way up ("throw one now"). It stays a
*modulator*: the duty cap still applies at full pedal, so the loop is never taken
over (bench: knob at 0, pedal 0 → 1 lifts duty 0.083 → 0.406, leaving 59 % normal
playback).

---

## 6. CC20 / CC23 — secondary & tertiary params

Dispatched by `gkLastToggle` (last toggled-ON effect):

| ID (effect) | CC20 | CC23 |
|---|---|---|
| 2 Attack | hold ms | — |
| 3 Old PS | semitones | — |
| 4 GPS | semitones | feedback |
| 5 Formant | shift ±12 | — |
| 6 Vibrato | rate | feedback |
| 7 Chorus | rate | depth |
| 8 Phaser | rate | stages |
| 9 Flanger | rate | feedback |
| 11 RingMod | depth | — |
| 14 AutoMoog | resonance | — |
| 15 Delay | time | feedback |
| 16 SpecDly | maxdel | FB |
| 17 MOOD | clock | density |
| 19 Reverb | room/FB | — |
| 20/23/24/25 Glitch | shared BPM | — |
| 21 TAPE | fail (drop/crinkle density) | model morph (reel→cassette→broken) |
| 26 H90 | semitones | — |
| 27 PitchFork | semitones (quantized set ±{5,7,9,12}) | — |
| 28 FORMS | shift ±12 | — |
| 31 OD | wc1: Klon↔TS voice morph | wc2: clean blend |
| 32 MAR | wc1: presence | wc2: tightness (low-cut) |
| 33 DHG | wc1: SLO↔5150 blend (>0.95 = series stack) | wc2: tightness |
| 34 RMZ | wc1: mid-sweep freq (200 Hz–5 kHz log) | wc2: mid gain |
| 35 MUF | wc1: mid recovery (un-scoop) | wc2: voicing (Triangle/Ram's Head/Fallout) |
| 36 Habit | size (50 ms–60 s exp) | repeats (max = stable infinite) |
| 43 Tremolo v2 | **MODE stepper** (per detent, wraps: SINE→OPTO→BIAS→HARM) | rate (exp 0.5–20 Hz — moved here from P1) |
| 45 ONWARD | sustain | octave voice −1..+1 |
| 46 LOSSY | loss | bandwidth |
| 47 RESONATOR | onset | feedback |
| 48 IMPULSE | tone | decay |
| 49–53 IR VERB | damp (wet tone) | level (wet trim) |
| 54 OCTAVER | tone | character |
| 55 ENV FILTER | sensitivity | mode morph |
| 56 BLOOPER (CC29 = manual chaos push) | auto-motion depth (how much the scatter drifts on its own) | **SCATTER** (the main granular knob) |
| 57 COMPRESSOR | speed (0 = FET fast 5/50 ms … 1 = opto slow 40/400 ms) | makeup (0 … +12 dB) |
| 58 TONE EQ | mid frequency (200 Hz – 3 kHz exp) | tilt (0.5 = flat) |
| 60 CHAIN FB | loop gain (×0 – ×1.5; **>0.67 = loop gain >1 = self-oscillation**) | loop tone tilt (0 dark LP · 0.5 open · 1 thin HP) |
| 61 TRACK SYNTH | filter cutoff (exp in DSP) | resonance |
| 62 PLL FUZZ | **interval stepper** (÷4 ÷2 ×1 ×2 ×3) | lock speed (0 = chaos/hunting … 1 = tight) |
| 63 FEEDBACKER | **harmonic stepper** (fund / +oct / +oct+5th / +2oct) | bloom onset (0.2 – 4 s) |
| 64 DIFFUSION | depth (scales allpass times, gains AND live stage count 2→6) | mod rate (0.08 – 2.6 Hz exp) |

**Menu-0 exception (2026-07-13):** while menu 0 is on screen, **CC20 does not
dispatch on `gkLastToggle` at all** — it edits the active preset's output trim
(§4). Every other menu is unchanged.

**DELAY (15) gained a 5th param:** `gkDlyMode` — 0 NORMAL / 1 REVERSE (repeats
play backwards; the reversed signal then runs through the same pitch-shift +
spring-tone feedback path, so the 2nd repeat comes back re-reversed = forward,
the classic reverse-delay feedback). It sits on the **P5 fill (CC25)**, stepped.
Mode 0 is a verified bit-exact null against the pre-mode delay. OSC `/dly_mode`.

(Since encoder-mapping v2 the same lastToggle dispatch also backs the other
rotation CCs — full param-per-position map in `docs/encoder_mapping_v2.md`.)

**Full encoder map for the 2026-07-13 ten-effect batch** (P1 = the effect's home
slot rotation; P2 = CC20; P3 = CC23; P4+ = the remaining slot rotations):

| Effect | Home | P1 | P2 (CC20) | P3 (CC23) | P4 | P5 | P6 |
|---|---|---|---|---|---|---|---|
| COMPRESSOR 57 | PREAMP s4 / CC22 | amt | speed | makeup | blend (CC24) | gate (CC25) | — |
| TONE EQ 58 | SYNTH s2 / CC26 | mid | freq | tilt | low (CC24) | high (CC25) | — |
| CHAIN FB 60 | AIR s4 / CC22 | return | gain | tone | ceiling (CC24, 0.1–0.95) | — | — |
| TRACK SYNTH 61 | SYNTH s0 / CC24 | blend | cutoff | res | glide (CC25) | sub (CC26) | detune (CC21) |
| PLL FUZZ 62 | SYNTH s1 / CC25 | dry/wet | interval | lock | voice morph (CC24) | — | — |
| FEEDBACKER 63 | AIR s0 / CC24 | level | harmonic | bloom | vibrato (CC25) | — | — |
| DIFFUSION 64 | AIR s1 / CC25 | dry/wet | depth | rate | duck (CC24) | — | — |

## 7. CC29 — expression pedal dispatch (`gkLastToggle29`)

| ID | Target |
|---|---|
| 1 | volume pedal level (ambient swells) |
| 2 | attack sensitivity |
| 3/4/5/26/27/28 | pitch-family semitones/shift (±12) |
| 8 | phaser feedback |
| 10 | leslie speed |
| 11 | ring mod freq (exponential) |
| 15 | delay time |
| 17 | MOOD clock (inverted) |
| 20/23/24/25 | glitch chance (shared) |
| 21 | TAPE transport (heel = play, toe = stop, spin-up on release) |
| 22 | wah position |
| 30 | **master volume** (front-of-chain preset swell) |
| 31–35 | drive pedal DRIVE amount (OD/MAR/DHG/RMZ/MUF) |
| 36 | Habit SCAN (auto-scan amount: how often + how deep) |
| 38 | BIN REDU contrast (heel = extreme crush, toe = transparent — polarity flipped on purpose) |
| 44 | BCRUSHER coarsening (heel = transparent, toe = max) |
| 45 | ONWARD capture gate (toe = FOLLOW every pick, heel = HOLD the pad, between = probability) |
| 61 | TRACK SYNTH filter cutoff |
| 63 | FEEDBACKER intensity push (more level + faster bloom) |

### CC29 arbitration — last-toggled-wins (2026-07-13)

This batch brought CC29 contention to a head: **three** effects now claim the
pedal on toggle-ON — **45 ONWARD**, **61 TRACKSYNTH**, **63 FEEDBACKER** — on
top of the existing claimants (drives, TAPE, WAH, the pitch family, the glitch
group, presets…).

**There is no priority table, and none is needed.** The standing sticky rule
already resolves this completely:

> `gkLastToggle29` is a single integer. Every claim is a plain
> `gkLastToggle29 = <id>` in the effect's toggle-ON branch. **Whichever effect
> you switched on most recently owns the pedal.** Everything else keeps working
> from its encoders — an unclaimed effect's expr param simply stops moving.

Consequences worth knowing:

- **Turning an effect OFF does not release the claim.** The pedal keeps pointing
  at it until something else claims it. Writing to a bypassed effect's param is
  harmless, so this is safe — and it means re-engaging the same effect finds the
  pedal already pointed at it.
- **Toggle-ON resets the gesture, not just the target.** FEEDBACKER clears
  `gkFBK_expr` on engage, so a stale pedal position left by a *previous* owner
  can never slam it the instant it comes on. (Same reasoning as the drive
  pedals' `drive = 0.75` toggle-on reset.)
- **Six of the new effects claim nothing on purpose** — COMPRESSOR (57),
  TONE EQ (58), CHAIN FB (60), PLL FUZZ (62), DIFFUSION (64).
  They leave the pedal wherever it was. Compressor-threshold-on-the-pedal is a
  tempting future option; it is deliberately NOT wired, because with three
  claimants already the pedal is the scarcest control on the box.

---

## 8. Signal Chain (instr 1, signal_chain.orc)

**Input (2026-07-13): TRUE STEREO switch.** `gkStereoIn` (seeded from
`--omacro:STEREO_IN`, default 1) — 1 = in1→L, in2→R preserved through the
chain; 0 = the legacy mono sum ((in1+in2)×0.5 to both sides, byte-identical
to the old code). OSC `/stereo_in`. Note: in stereo mode a single instrument
in one input leaves the other channel silent — use STEREO_IN=0 for one-source
rigs. Per-effect pattern: copy `aPrevL/R` to `ga<FX>_send`, `$FX_BYPASS(...)` (event-starts instr N when bypass<0.5, `turnoff2` when ≥0.5 — bypassed effects cost zero CPU), then `ntrpol` mix or 100%-wet assignment.

```
0.   PREAMP "BroadcastPre"  (UDO, inline — toggle 39, no longer always-on; §10)
0a.  BASS PREAMPS  160/161  (radio pair, mutually exclusive)
0b.  BASS DIST     162
0.7  MASTER VOL (inline, gkMasterVol — FRONT so reverb tails bloom on swells)
1.   ATTACK        150        14.  AUTOMOOG      51
2.   H90           48         15.  WAH           52
3.   PITCH FORK    49         16.  VOLUME PEDAL  (inline)
4.   OLD PS        50         17.  SPECTRAL DLY  200
5.   GPS           55         18.  MOOD          401–405
6.   PHASER        233        19.  GRANULAR      80
7.   FLANGER       222        19.5 TREMOLO      101 (before the slap delay)
8.   CHORUS        40         20.  DELAY         62
9.   VIBRATO       (inline)   20.5 HABIT         64
10.  LESLIE        300        21.  REVERB        70
11.  DRIVE (serial, light→heavy, all inline):   22.  BBCUT      90
     OVERDRIVE → MARSHALL → DUAL HI-GAIN        23.  WGUIDE     91
     → RAT×MZ → BIG MUFF                        24.  SQMOD      92
10.9 FORMS         119        25.  FREQSHIFT    93
11.  FORMANT       120        26.  NAM AMP SIM  (NAMProcess, end of chain)
11.5 BIN REDU      121        27.  LOOPER       210–214 (taps end of chain,
11.6 BINCRUSHER    122             mix-back — in progress)
12.  TAPE          (inline; replaced DROPOUT 500)
13.  RING MOD      65
```

**2026-07-13 batch insertions** (stage labels = insertion points, see
signal_chain.orc):

```
0.9  OCTAVER       170   after BASS DIST, before the drive section
11.7 LOSSY         123   after BINCRUSHER, before TAPE
16.5 ENV FILTER    171   after WAH
19.2 IMPULSE SYNTH 156   after GRANULAR, before TREMOLO
22.7 RESONATOR     157   after HABIT — CUSTOM bypass (turnoff2 release) for 2 s trails
22.8 ONWARD        155   after RESONATOR, before REVERB
21c  IR REVERB     240   right after REVERB (writes the already-mixed blend)
27.5 BLOOPER       175   after the 5-ch LOOPER mix-back (always-on, self-scheduled)
```

**2026-07-13 ten-effect batch insertions** (toggles 57–65, instrs 180–188):

```
0.7b PRESET TRIM   (inline)  giPresetTrim[preset] × the master stage (§4)
0.8  COMPRESSOR    180   after BASS DIST, before OCTAVER — post-preamp / pre-dirt,
                         so it evens the signal FEEDING the drive section
2.5  FEEDBACKER    186   after H90, before PITCH FORK (dry unity inside)
11.4 CHAINFB inject 183  return bus re-injected here — 10 ms portk gate REQUIRED
11.5 TONE EQ       181   after the drive block — STRAIGHT THROUGH, no mix stage
14.3 PLL FUZZ      185   after AUTOMOOG, before WAH (dry unity inside)
18.7 DIFFUSION     187   after MOOD, before GRANULAR (ntrpol on gkDIF_wet)
19.3 TRACK SYNTH   184   after IMPULSE, before TREMOLO — needs onset_pick.orc
                         #included BEFORE it in the .csd (shared OnsetPick UDO)
22.9 CHAINFB capture 183 after ONWARD, before REVERB — closes the wrapped span
```

CHAINFB is the only effect with **two** chain tap points: it wraps the span from
11.4 to 22.9 rather than sitting in it. At the capture point `aPrev` is NOT
modified — instr 183 only computes the return bus, and its `$FX_BYPASS` dummy
outs (`gaCFB_dumL/R`) exist purely so the stock macro can be used verbatim.

TONE EQ is the one effect with no `ntrpol` mix stage: blending a filtered copy
against dry is a comb filter, not a tone control, so it is wired straight
through.

(Old DISTORTION instr 60 and TUBE SCREAMER instr 100 deleted — tubescreamer.orc
removed 2026-07-13, its TS character lives in OVERDRIVE wc1=1. DROPOUT replaced
by TAPE. Exact ordering/section numbers: read signal_chain.orc — its section
comments are the authority.)

---

## 9. DRIVE Menu 2026 — five circuit-modeled pedals

Spec: `docs/specs/2026-06-11-drive-menu-distortion-redesign.md`. All five are PEDAL/PREAMP voiced to stack into a real amp — **no cab sim, no power-amp sag, ever**. Uniform controls (per §6/§7): CC29 = drive, slot rotation = tone, CC20/CC23 = per-pedal wildcards. Globals pattern: `gk<FX>_bypass/_drive/_tone/_wc1/_wc2`, all normalized 0–1, mapped internally.

| Pedal | File | Circuit model |
|---|---|---|
| OVERDRIVE (130) | overdrive.orc | One morphable circuit: Klon end = op-amp gain → asymmetric germanium 1N34A soft clip + parallel CLEAN blend + treble shelf (18 V headroom = late knee); TS end = mid-hump pre-emphasis → symmetric Si clip → darker tone |
| MARSHALL (131) | marshall.orc | JCM800/Plexi preamp only: cascaded 12AX7-style asymmetric soft-clip stages, interstage HP coupling, bright cap, FMV tone stack |
| DUAL HIGH-GAIN (132) | dualhigain.orc | Two parallel preamp voices — SLO (smooth mid-forward) ⊕ 5150 (tight, scooped) — blendable; top of wc1 = series A→B stack |
| RAT × MZ (133) | ratmz.orc | RAT core: LM308 slew-limit LP → symmetric HARD clip (diodes to ground) → "Filter" LP tone, wearing MT-2's sweepable high-Q mid EQ |
| BIG MUFF (134) | bigmuff.orc | 4 cascaded stages: input booster → two diode-feedback clip stages → passive mid-scoop tone; wc2 morphs Triangle/Ram's Head/Fallout |

**Anti-aliasing:** all drive clipping runs through shared 4× oversampled UDOs at the end of globals.orc — `OS4Tanh` (symmetric soft), `OS4Diode` (asymmetric Vf-style soft), `OS4Hard` (true hard clip). Each: 4-point Hermite upsample → nonlinearity at 192 kHz → 4th-order Butterworth LP (20 kHz) → decimate. Aliases measured −35..−55 dB vs −22..−28 naive. ~7% of a Pi core per instance, zero when bypassed. Since 2026-07-13 the scaffold lives ONCE in `OS4Core` (i-time shape selector); the three public names are thin wrappers — callers unchanged.

**Level matching:** open-loop makeup gain as a function of drive per pedal (never RMS-follower makeup — it pumps). All five calibrated ±2 dB at 0.1-RMS input.

---

## 10. PREAMP — "BroadcastPre" (front end)

File: `effects/preamp.orc` (UDOs `BroadcastChan` + `Preamp`, called inline at chain position 0; fully separate from the DRIVE pedals). Spec: `docs/specs/2026-06-11-broadcast-preamp-design.md`. **No longer always-on**: since the bass-rig work it is toggleable (toggle 39, PREAMP menu s0, `gkPreamp_bypass`), and any surf-bank preset activation force-enables it so the gig presets keep their voiced sound.

Stage-by-stage model of the **Hudson Broadcast** (transformer-coupled discrete class-A germanium preamp from 1960s broadcast consoles): the classic "always-on pedal squish in front of a real amp."

```
in → HP 80 Hz → [Si stage] → [Ge stage] → [transformer] → LP 9 kHz + makeup → out
```

- **Si stage:** squared-term even-harmonic seasoning, DC-blocked, 25 Hz coupling HP.
- **Ge stage (the dynamic part):** shared envelope follower (`follow2` 5 ms/120 ms on the mono sum, portk-smoothed) moves the tanh bias point — dig in → more asymmetry → 2nd-harmonic bloom + compression give. Formula: `(tanh((x·G + b)·d) − tanh(b·d))/(G·d)`, bias `b = b0 + env·bdep`. **The bias is `interp`ed to a-rate — k-rate bias steps into a waveshaper CRACKLE. Never downgrade this.**
- **Transformer:** phase-coherent split at 250 Hz; the low band saturates harder (steel-core flux limits → low-end thickening), +1 dB bump at 80 Hz, 12 kHz rolloff, dcblock2.
- **Gain staging insight:** real guitar sits at ~10–15% of full scale, so each mode has a gain INTO the clipper (`iInGain`, like a pedal's gain knob) — without it the nonlinearity never engages and "drive" constants do nothing audible.

**Modes** (`giPreVoice` at top of file; all calibrated to unity loudness at guitar level, so switching never jumps volume):

| Mode | iInGain | Character | H2 @ guitar level |
|---|---|---|---|
| 0 SOFT | 1.0 | barely-there glue | ≈ −37 dB |
| 1 MEDIUM (default) | 3.2 | clear warm thickening + squish | ≈ −11 dB |
| 2 MAX | 7.0 | always saturated, heavy | ≈ −5 dB |

No oversampling by design (soft clip at these levels = low-order harmonics; cost without audible return). To wire a UI later: replace the `giPreVoice` constant with a global — the mode rows already exist.

---

## 11. Effect-by-effect summary (everything else)

| Effect (instr, file) | Algorithm / character | Key globals |
|---|---|---|
| Attack (150, attack.orc) | envelope-follower attack shaper (swell/reverse feel); sens, hold ms | gkATTK_* |
| H90 (48, h90.orc) | pvsifd→partials→trscale→tradsyn sinusoidal PS, cleanest, ~8 ms; small-interval dry blend | gkSemiH90, gkH90mix |
| Pitch Fork (49, pitchfork.orc) | circular buffer → mincer FFT 1024 klock=1; bright/digital at big shifts, ~19 ms; semis quantized to ±{5,7,9,12} via CC20 step 2.5 | gkSemiPF, gkPFmix |
| Old PS (50, pitchshifter.orc) | custom `pitchshifter4` UDO, 4-grain crossfaded delay lines; gritty legacy | gkSemi, gkDryPctPS |
| GPS (55, gps.orc) | sndwarp granular PS with freeze/feedback/LPF/clip | gkGPS* |
| Phaser (233, phaser.orc) | `PhaserSt` UDO, phaser1 stages 1–24, 9 LFO shapes | gkPHA_* |
| Flanger (222, flanger.orc) | `Flanger` UDO, flanger opcode + LFO shapes, feedback ±1 | gkFLA_* |
| Chorus (40, chorus.orc) | `StChorus`/`StChorusRspline` UDOs, vdelay-based stereo chorus | gkChorus* |
| Vibrato (211, vibrato.orc) | vdelayxw pitch vibrato; depth = mean added latency (capped 5 ms) | gkVIB_* |
| Leslie (300, leslie.orc) | **v2 rebuild 2026-07-13**: physical horn+drum model — 800 Hz crossover (48 dB/oct drum side), counter-rotating a-rate phasors, per-mic distance/doppler (`vdelayx`) + cardioid directivity; second param is now mic DISTANCE (UI label "Mic Dist"; close = deep). `DopplerSpin` UDO no longer used (retirement candidate) | gkLeslie* |
| Tremolo (101, tremolo.orc) | **v2 rewrite 2026-07-13**: 4 modes on one LFO — 0 SINE (v1 math verbatim), 1 OPTO (photocell duck, asym lamp/LDR), 2 BIAS (skewed wobble + trough-synced tube grit), 3 HARM (800 Hz low/high see-saw). CC20 = mode stepper w/ wrap; CC23 = rate (exp); P1/P4 = depth. Manual toggle-on defaults to HARM; surf-preset paths force `gkTremMode = 0` so the gig sound is untouched. OSC `/trm_mode` | gkTrem*, gkTremMode |
| FORMS (119, forms.orc) | pvswarp + pvsblur + LFO-wobbled vowel resonator bank; dramatic formant character | gkForms* |
| Formant (120, formant.orc) | legacy double-pvscale formant-preserve shifter | gkFormant* |
| TAPE (inline, tape.orc) | Gen Loss-style tape machine (replaced Dropout 500): wow/flutter, biased OS4Tanh saturation, fail (drop/crinkle), model morph reel→cassette→broken; CC29 = transport (heel play / toe stop, spin-up) | gkTape* |
| Ring Mod (65, ringmod.orc) | oscillator ring modulation, exp freq 0.05–250 Hz | gkARMod* |
| AutoMoog (51, automoog.orc) | `EnvelopeFollower` UDO → moogladder/lpf18 auto-filter | gkAUTOMOOG_* |
| WAH (52, wah.orc) | resonant filter, CC29 = pedal position | gkWAH_* |
| Spectral Delay (200, spectraldelay.orc) | pvsanal FFT 256 per-bin delay/feedback (SPECTRAL menu s2) | gkSpecDel* |
| BIN REDU (121, bincrush.orc) | spectral contrast exponentiation (bin magnitude reshaping), 100% wet; CC29 polarity: heel = extreme crush | gkBin* |
| BCRUSHER (122, bincrusher.orc) | spectral bin coarsener (BinSelect survivors), 100% wet; spec `docs/specs/2026-06-23-bincrush-spectral-bin-quantizer-design.md`; v1 averaging algorithm archived in `effects/_retired/bincrusher_v1_coarsen.orc` | gkBc* |
| HABIT (64, habit.orc) | Chase Bliss Habit port: ~175 s always-recording memory ring + stochastic AUTO-scan playhead (exp pedal = how often/deep, crossfaded jumps) → ~87 s echo, dual-tap click-free SIZE 50 ms–60 s (never warps), tape-aged feedback loop (wow/flutter + HP80/LP4500 + tanh; max repeats = stable infinite). Memory clears on re-engage via valid-sample counters. Manual: docs/reference/Habit_Manual_Chase_Bliss.pdf | gkHabit* |
| MOOD (401–405, mood.orc) | multi-instr granular/pitch/bitcrush playground (own buses, inline launch logic in chain); `MOOD_BITCRUSH` macro, giSemi table. **Instr 401 pitch shifter uses H90 algorithm** (pvscale sinusoidal model, mono-summed input, stereo restored by granular stage). UP branch cascades two pvscale stages at sqrt(kRatio) each — one octave max per stage — so +24 semi is two clean ×2 shifts instead of one harsh ×4 shift. DOWN branch: pvslock+pvscale as in H90. Both with per-stage anti-alias LP + transient sidechain. | gkMOOD_* |
| Granular (80, granular.orc) | sndwarp-based granular wash | gkGran* |
| Delay (62, delay.orc) | vdelay3 + pitch-shifted feedback; **gkDelayMS is NOT ms: actual ms = gkDelayMS/1000**, max 800 ms; spring-tone feedback band-limit HP180/LP4k | gkDelayMS, gkFeedback, gkDelMix |
| Reverb (70, reverb.orc) | reverbsc; FB=size (0.92 default), damping fixed 0.9 | gkReverbFB, gkverbMix |
| BBcut (90, stutter.orc) | bbcutm beat-slicer (`STUTiteration` UDO), glitch group | gkBBcut_* |
| WGuide (91, stutter.orc) | waveguide resonator glitch | gkWGuide_* |
| SqMod (92, stutter.orc) | square-wave chopper glitch | gkSqMod_* |
| FreqShift (93, stutter.orc) | hilbert SSB shifter (`STUTfreqshifter`), fires at chance×0.2 | gkFShift_* |
| Volume Pedal (inline) | portk-smoothed gain, menu 1 s0 | gkVOLpedal* |
| Master Vol (inline, front) | preset swell gain — see §4 | gkMasterVol |
| Bass Preamp L/H (160/161, bass_preamp.orc) | radio-voiced bass preamp pair, mutually exclusive (toggling one kills the other); front of chain | gkBPL_*, gkBPH_* |
| Bass Distortion (162, bass_dist.orc) | PREAMP-family bass distortion (drive/blend/tone), front of chain | gkBDS_* |
| NAM (inline UDO call, nam.orc + plugin) | Neural Amp Modeler at end of chain — see §5 menu 11 | gkNAM* |
| LOOPER (209–214, looper.orc) | **in progress** — 5 end-of-chain loop channels, power-of-two length quantization — see §5 menu 10 | gkLp* |
| ONWARD (155, onward.orc) | auto-recapturing spectral freeze: every pick onset (shared `OnsetPick` UDO, onset_pick.orc) refreezes into one of two ping-pong `pvsfreeze` slots and morphs the pad to it; SUSTAIN = pad decay (1 = infinite), OCTAVE = ±1 sndloop voice; CC29 = FOLLOW/HOLD | gkONW_* |
| LOSSY (123, lossy.orc) | codec degradation: per-bin `SpectralGate` w/ hysteresis + 30 ms release (bins truly vanish), bandwidth crush, packet-drop scheduler (silence/repeat) | gkLSY_*, gkLsyBypass |
| RESONATOR (157, resonator.orc) | sympathetic pipe bank (`streson` voices pinned to semitones, onset gates, regen loop, ±octave companions, freeze); custom chain bypass w/ 2 s trail release | gkRES_* |
| IMPULSE SYNTH (156, impulse_synth.orc) | per-pick synth hit (OnsetPick + one-shot `ptrack` pitch sample): saw+square+sub → moogladder w/ per-hit AD; velocity from pick envelope; dry untouched | gkIMP_* |
| IR VERB (240, ir_reverb.orc) | ftconv convolution reverb, 5 rows × 5 IRs preloaded (spring/plate/hall/chamber/wood), NAM-style row/variant browsing — see §5 menu 12 | gkIR*, gaIR* (buses in globals.orc) |
| OCTAVER (170, octaver.orc) | Boss OC-2-style analog-voiced sub-octave (SUB/TONE/CHAR); before the drive section | gkOCT_* |
| ENV FILTER (171, envfilter.orc) | boutique bass envelope filter (wet/sens/mode/att/dec); AutoMoog stays the surf voicing | gkENV_* |
| COMPRESSOR (180, compressor.orc) | optical/FET compressor + downward-expander gate. AMT is a macro (threshold 0…−40 dB + ratio 1:1…10:1), SPEED morphs FET-fast (5/50 ms) → opto-slow (40/400 ms), MAKEUP 0…+12 dB, GATE is an expander threshold (−75…−25 dB, 0 = off). Parallel BLEND is the chain `ntrpol` | gkCMP_* |
| TONE EQ (181, toneeq.orc) | amp-style tone stack: sweepable mid peak (±12 dB, 200 Hz–3 kHz) + tilt + low/high shelves (100 Hz / 3 kHz). 0.5 = flat on every band; at exactly flat the instr hard-bypasses (bit-transparent null). **No mix stage** — wired straight through | gkTEQ_* |
| CHAIN FB (183, chainfb.orc) | feeds the chain back into itself — captures pre-reverb (22.9), runs gain (×0–×1.5, past unity = self-oscillation) → tanh soft limiter → LP/HP tone tilt → dcblock → hard ceiling → return level, re-injects post-drives (11.4) one k-block later. **Bounded by construction:** last op is `limit ±kCeil`, `kCeil ≤ 0.95`, scaled by `kRet ≤ 1` — the injected signal can never exceed 0.95 peak. Per-sample NaN/Inf guard; the 10 ms inject gate is required (ga buses hold their last block after turnoff2) | gkCFB_*, gaCFB_* |
| TRACK SYNTH (184, tracksynth.orc) | `ptrack` (hop 1024, ~21 ms — hop 512 octave-errors on E2/A2 saws) follows the input, clamped 55–1200 Hz with hold-previous on loss → glide → 3-osc voice (saw + detuned square + sub square) → moogladder whose cutoff rides a per-onset AD (shared `OnsetPick`) on top of the base cutoff. **Amplitude is slaved to the input envelope** — the synth dies with the string, no gate hangover. Prime bass-bank material | gkTSY_* |
| PLL FUZZ (185, pllfuzz.orc) | Data-Corrupter-style: a comparator hard-squares the input (master voice, near-zero latency), a hysteresis comparator measures the period sample-accurately and flip-flops a ÷2 square (sub voice), and a simulated PLL (phase-comparator error → 2nd-order loop filter) steers a `vco2` square toward f × interval (÷4 ÷2 ×1 ×2 ×3). LOCK sets loop gain AND damping: slow = underdamped hunting + injected loop noise = the glitchy re-locks that ARE the sound. Three voices morph master→sub→PLL | gkPLL_* |
| FEEDBACKER (186, feedbacker.orc) | pitch-tracks the input; while a note SUSTAINS, a chosen overtone (fund / +oct / +oct+5th / +2oct) swells in as a 2-partial oscillator with a bloom envelope (0.2–4 s, kBl² curve — slow start then accelerating, like real feedback taking hold). ±5-cent vibrato + slow random detune walk. Voice amplitude rides a latched note-level tracker, so it swells UP while the string decays (FreqOut behaviour) yet sits UNDER the dry at default level. CC29 = intensity push | gkFBK_* |
| DIFFUSION (187, diffusion.orc) | the FRONT HALF of a reverb, amputated on purpose: 6 LFO-modulated Schroeder allpasses (custom `delayr`/`deltap3` — stock `alpass` can't modulate its loop time), golden-ratio-spaced 5–40 ms, **no recirculating tank**. Transients smear into a cloud that dies in <400 ms — texture, not space. DEPTH scales times, gains and live stage count (2→6) together; DUCK pushes the wet down while you play so it only blooms in the gaps. L/R decorrelated (R = L × 1.07) | gkDIF_* |
| BLOOPER (175, blooper.orc) | Chase Bliss Blooper-style bottomless looper (menu 4 s4, always-on, self-scheduled): one stereo loop ≤40 s, 8 additive layers w/ O(1) undo/redo, live loop-only modifiers (STAB/DROP/SCRAM), stepped varispeed −1x/0.5x/1x/2x; taps post-LOOPER, loop bus mixed on top of the inline dry path; ~246 MB ftables at the 40 s default (`BLP_MAXSEC` overrides) | gkBLP_*, gaBLP_*, giBLP_* (all self-declared in blooper.orc) |

**Toggle ID registry:** 1 VolPedal · 2 Attack · 3 OldPS · 4 GPS · 5 Formant · 6 Vibrato · 7 Chorus · 8 Phaser · 9 Flanger · 10 Leslie · 11 RingMod · 12/13 retired (old dist/TS) · 14 AutoMoog · 15 Delay · 16 SpecDly · 17 MOOD · 18 Granular · 19 Reverb · 20 BBcut · 21 TAPE (was Dropout — ID reused) · 22 WAH · 23 WGuide · 24 SqMod · 25 FreqShift · 26 H90 · 27 PitchFork · 28 FORMS · 29 unused (avoid — CC29 confusion) · 30 MasterVol · 31 OD · 32 MAR · 33 DHG · 34 RMZ · 35 MUF · 36 HABIT (instr 64, CHASE BLISS s2 — moved from TIME s1 2026-07-13) · 37 NAM · 38 BIN REDU · 39 MAIN PREAMP · 40 BASS PRE L · 41 BASS PRE H · 42 BASS DIST · 43 TREMOLO · 44 BINCRUSHER · 45 ONWARD (instr 155, CHASE BLISS s1 — moved from PITCH s4 2026-07-13) · 46 LOSSY · 47 RESONATOR · 48 IMPULSE · 49 IR SPRING · 50 IR PLATE · 51 IR HALL · 52 IR CHAMBER · 53 IR WOOD RM · 54 OCTAVER · 55 ENV FILTER · 56 BLOOPER (instr **208**, CHASE BLISS s0 — granular MODIFIER of the LOOPER's loops, rearchitected 2026-07-13; owns no buffer) · 57 COMPRESSOR (instr 180, PREAMP s4) · 58 TONE EQ (instr 181, SYNTH s2) · 60 CHAIN FB (instr 183, AIR s4) · 61 TRACKSYNTH (instr 184, SYNTH s0) · 62 PLLFUZZ (instr 185, SYNTH s1) · 63 FEEDBACKER (instr 186, AIR s0) · 64 DIFFUSION (instr 187, AIR s1) · 66 MOOD II (instr 189, CHASE BLISS s3 — NOT the original MOOD t17/401-405, which stays in TIME; REWORKED 2026-07-13 to SLIP-only, plain bypass, no micro-looper — see the CHASE BLISS s3 row above) · ~~65 SHIMMER~~ **REMOVED 2026-07-13** (instr 188; cut at the user's call — a shimmer is achievable by routing the existing pitch shifter into reverb; `shimmer.orc` archived in `effects/_retired/`); **65/188 PROPOSED (not yet wired) for GRAINVERB**, the reverb algorithm extracted from MOOD II v2 (`effects/grainverb.orc` — built + benched, "for later" per the user). **59 is FREE** (TAPESTOP, removed 2026-07-13). · 67 BinScatter · 68 TapeDelay · 69 MoveDelay · 70 Nebula · 71 Shimmer · 73 GrainVerb · **SOUNDMAGIC rack, toggle ids 75..98 = 75 + fx** (set by OSC `/st`, never by a slot button; instrs 241–248 rack groups, 249 control, 250 persistence; `csound/spectral/`): 75 SOUNDMAGIC AVERAGE (Spectral Averaging) · 76 SOUNDMAGIC BLUR (Spectral Blurring) · 77 SOUNDMAGIC DRONE (Spectral DroneMaker) · 78 SOUNDMAGIC FREEZE (Spectral Freezing) · 79 SOUNDMAGIC GATEHOLD (Spectral Gate And Hold) · 80 SOUNDMAGIC WEAVE (Spectral Weave) · 81 SOUNDMAGIC SHIMMER (Spectral Shimmer) · 82 SOUNDMAGIC SHUFFLE (Spectral Shuffle) · 83 SOUNDMAGIC PULSING (Spectral Pulsing) · 84 SOUNDMAGIC HARMONIZ (Spectral Harmonizer) · 85 SOUNDMAGIC BINSHIFT (Spectral Bin Shift) · 86 SOUNDMAGIC PITCH (Spectral Pitch Shift) · 87 SOUNDMAGIC STRETCH (Spectral Stretch) · 88 SOUNDMAGIC TRACING (Spectral Tracing) · 89 SOUNDMAGIC FILTBANK (Spectral Filterbank) · 90 SOUNDMAGIC GLIDEFLT (Spectral Gliding Filters) · 91 SOUNDMAGIC GRANUL (Spectral Granulation) · 92 SOUNDMAGIC EMERGE (Spectral Emergence) · 93 SOUNDMAGIC PARTGLD (Spectral Partial Glide) · 94 SOUNDMAGIC CHORUS (Chorus, phase 3, not yet wired) · 95 SOUNDMAGIC GRAINSTR (Grain Streamer, phase 3, not yet wired) · 96 SOUNDMAGIC IDEEFIX (Idee Fixer, phase 3, not yet wired) · 97 SOUNDMAGIC SUPERFB (Super Filterbank, phase 3, not yet wired) · 98 SOUNDMAGIC spare (fx 23)  **Next free: 74 (75..98 = SOUNDMAGIC). 59 (TAPESTOP, removed) and 65 (SHIMMER moved to 71) are retired, not free. GRAINVERB is integrated at 73 / instr 188.** **Next free instr: 182, 190** (188 reserved for GRAINVERB; 189 = MOOD II, 208 = BLOOPER). (The looper channels are press-fired state machines, not toggle IDs. Instrs 241–250 are the SOUNDMAGIC spectral rack.)

**TAPESTOP removed 2026-07-13.** It was briefly wired as toggle 59 / AIR s3 (inline, stage 12.5). Cut at the user's call: the TAPE machine (toggle 21, `tape.orc`) already owns tape behaviour, including its CC29 transport (heel = play, toe = stop, spin-up on release) — a second tape-brake pedal was redundant. The file is retired to `effects/_retired/tapestop.orc` (not deleted) in case the buffer-brake is ever wanted as a distinct gesture. Toggle 59 and instr 182 are free.

`tests/test_menu_consistency.py` asserts this registry lists every toggle ID the
handler actually fires — if you add a toggle and forget this line, the test fails.

---

## 12. Shared macros & UDOs (globals.orc)

- `FX_BYPASS(kByp'iNum'sendL'sendR'outL'outR'uid)` — launch/kill pattern; bypassed = passthrough + zero CPU.
- `FX_TOGGLE(kByp'iToggle)` — flip bypass, set `gkLastToggle` on ON.
- `GLITCH_PRESS` / `GLITCH_RELEASE` — hold-vs-click semantics (§5 menu 7).
- `ENC_ROT(kVar'kStep'kMin'kMax)` — relative encoder math (needs `kVal` in scope).
- `OSC_F` / `OSC_I` — change-triggered OSC sends via `OSC3x_f/i` (triple-send UDOs to 127.0.0.1:8000).
- `MOOD_BITCRUSH` — semitone-table-driven bitcrush.
- UDOs: `SrReducer`, `SwitchPort`, `DopplerSpin`, `EnvelopeFollower`, `LoFi`, `pitchshifter4`, `STUTiteration`, `STUTfreqshifter`, `StChorus`, `StChorusRspline`, `Flanger`, `PhaserSt`, `OS4Core` (shared 4× oversampling scaffold) + wrappers `OS4Tanh`/`OS4Diode`/`OS4Hard`, `BroadcastChan`, `Preamp`.

---

## 13. Clock, ChucK, UI

**Clock:** DEMIURGE broadcasts BPM on ch16 CC119 (0–127 → 40–200). instr 99 reads it into `gkGLITCH_bpm`. Retune: write ch16 CC118. Transport: ch16 CC117.

**ChucK (chuck/synthi.ck):** LOCKIN 4-saw voice + 16th sequencer, arcade button CC68 — **NOT RUNNING in production** (bypass mode = csound owns the device; synthi.ck commented out in live.conf). Kept for normal-mode use.

**UI (neptrPhase4_UI.py — pygame, Kivy dropped 2026-06-12):** portrait touchscreen, OSC mirror on 127.0.0.1:8000, `neptr-ui.service` (refreshed by sync_to_pi.sh). Also hosts COMPANION mode (joystick-hold dashboard/preset/power menus — `docs/companion.md` is the master UI reference). Endpoints: `/menu`, `/preset`, per-effect `/<fx>_state` + params — full list at the bottom of midi_handler.orc; protocol spec in `docs/ui.md`. There is also a serial bridge (`csound/serial_bridge.py` + SERIAL_BRIDGE.md, `neptr-bridge.service`) for Teensy sensor data.

---

## 14. Sync, deploy, debug

```sh
./scripts/sync_to_pi.sh                                   # Mac → Pi (rsync --delete + service refresh)
ssh gorpi7@demiurge.local 'sudo systemctl restart demiurge'   # apply csound changes (needs tty/password)
ssh gorpi7@demiurge.local 'journalctl -u demiurge --no-pager -n 200 | grep -v "MIDI CC"'  # logs (instr 99 spams CC prints)
ssh gorpi7@demiurge.local 'bash ~/NEPTR_phase4/scripts/claude_midi.sh cc <num> <val> [ch]'  # inject PHYSICAL CCs
csound --opcode-lib=csound/nam/build-macos/csound-nam.so --syntax-check-only -o null csound/neptrPhase4.csd
  # Mac compile check (-o null avoids device "1 error"; the opcode-lib flag loads
  # the NAM plugin so NAMProcess parses — without it parsing stops at signal_chain)
```

Mac-side test renders: neptrPhase4.csd's CsOptions has a laptop block; offline A/B harnesses can `#include` a single effect file (see the BroadcastPre verification in its spec). Subagent sandboxes can't exec csound — run benches in the main session.

---

## 15. Files

| File | Role |
|---|---|
| `live.conf` | boot config; `bypass = on` is production; device inventory auto-written at top |
| `csound/neptrPhase4.csd` | entry — #includes globals, ~36 effect files, signal_chain, midi_handler |
| `csound/globals.orc` | engine header, macros, tables, all gk/ga globals, UDOs incl. OS4Core/OS4* |
| `csound/midi_handler.orc` | instr 99 shell: header tables + `#include`s of `csound/handler/*.orc` |
| `csound/handler/*.orc` | 2026-07-13 mechanical split of instr 99 (zero semantic change): `handler_remap.orc` = physical→logical CC remap; `handler_menus.orc` = per-menu toggle/encoder/preset/expr dispatch; `handler_looper.orc` = ALL looper logic (LP_QCLOSE + `$LP_*` gesture macros expanded verbatim at the marked call sites); `handler_osc.orc` = change-triggered OSC feedback. RULE: effect dispatch edits go in `handler_menus.orc`; never edit `handler_looper.orc` except for looper work. |
| `csound/signal_chain.orc` | instr 1: the full chain (§8) |
| `csound/effects/*.orc` | one effect per file (§9–§11) |
| `docs/specs/*.md` | design specs (drive redesign, broadcast preamp, h90/pitchfork) |
| `docs/ui.md` | OSC protocol |
| `scripts/sync_to_pi.sh`, `scripts/claude_midi.sh` | deploy + remote MIDI |
| `presets/` | named MIDI activation sequences |
| `PHASE4IMPLEMENTATION.md` | OLD architecture doc (PipeWire-era) — superseded by this file for the audio graph |

## 16. Adding a new effect (checklist)

1. `effects/<name>.orc` — `instr N` (next free toggle ID is **66**, next free instr is **189**; check §11 registry and §8). Read `ga<FX>_sendL/R`, write `ga<FX>_outL/R`, `dcblock2` + `limit ±0.99` out, port-smooth k-controls.
2. `globals.orc` — bypass/param/step globals + 4 bus inits.
3. `signal_chain.orc` — FX_BYPASS block at the right position.
4. `csound/handler/handler_menus.orc` — menu slot toggle + rotation + CC20/23/29 dispatch; `csound/handler/handler_osc.orc` — OSC feedback; update the header comment tables in `midi_handler.orc`. (Never `handler_looper.orc` unless it's looper work.)
5. `neptrPhase4.csd` — add the #include. (Anything using the shared `OnsetPick` UDO must be #included AFTER `onset_pick.orc`.)
6. `neptrPhase4_UI.py` — `MENUS` row, `EFFECT_PARAMS` row, `STEP_LABELS` if any param is stepped; `route_osc` only if the generic `/<code>_<param>` handling does not suffice.
7. Drive-style clipping? Use OS4* UDOs. Level-match open-loop. Update THIS DOC (§5, §8, §11 registry).
7b. Spectral (pvs bin-array) kernels do NOT go through steps 1-4: they live in `csound/spectral/` (`sm_<name>.orc` defines `SM_<Name>` + `#define SM_HAVE_<CODE> ##`, list it in `neptrPhase4.csd` before `sm_rack.orc`; registry in `sm_params.orc` is generated). See `csound/spectral/CONTRACT.md`.
8. **Run `pytest tests/test_menu_consistency.py`** — it fails the build if the UI tables, the handler and this doc disagree. It is the cheapest way to catch a half-wired effect.
