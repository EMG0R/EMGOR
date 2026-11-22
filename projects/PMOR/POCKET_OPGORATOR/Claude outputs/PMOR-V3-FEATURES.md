# PMOR v3 — Complete Feature Brief

Everything EMGOR asked for in the 2026-09-14 planning session, laid out so an agent can
implement it exactly — one stage at a time if wanted. Each item carries a status against
the current `PMOR_V3/` tree (stage 6o):
**DONE** = implemented, **HW** = implemented, needs hardware verification, **TODO** = not done.

Ground rules for whoever implements:
- Nothing beyond this document. No extra visuals, menus, features, or "improvements".
- All buttons act on PRESS (impact), never release.
- Locked hardware code in `hw/` is verbatim from PMOR_V2 — do not touch it.
- Suggestions are not decisions. If it is not in here, ask EMGOR.
- Subagents, if any, run on Sonnet.

---

## 0. Hardware facts and calibration

| Item | Value | Status |
|---|---|---|
| Board | Daisy Seed 3, libDaisy v4.0.0 (patched), TinyUSB (patched), `BOOT_SRAM` | DONE |
| FSR | 330 R pulldown, cubed curve, **idle raw ≈ 1600 = zero** (FsrScale auto-learns) | DONE |
| Joystick | usable max ≈ 950 on the ±1000 scale; centre measured at boot | DONE |
| Joystick orientation | module is mounted rotated. Logical axes everywhere: **+X = RIGHT, +Y = UP**. Set at ONE place: `config.h` flags `JOY_SWAP_XY=1, JOY_INVERT_X=1, JOY_INVERT_Y=1` (test 3). If still wrong, flip a flag there — nothing else. | HW |
| Lidar | driver overhauled (per-read DSS update, manual calibration, SHORT mode 33 ms / 40 ms, centre ROI, hold last value 3 reads) for the "reads 0 at 4 in" bug | HW — register `0x00D8` unverified |
| SD | bit-bang, 3.3 V, init clock `DelayUs(5)` — all locked | DONE (card itself is flaky — separate issue) |

---

## 1. Physical controls — the global rules

| Control | Rule | Status |
|---|---|---|
| **Encoder turn** | ONLY moves the main menu ring, hard stops both ends. Anywhere below the root it is the **BACK** button (one level per detent: editor → list → root; focused pad → un-focus first). | DONE |
| **Encoder press** | **TAP TEMPO, always, everywhere.** Nothing else on this button. | DONE |
| **Joystick press** | Context: push into a card/submenu; set a parameter & exit; back out; un-focus a pad. | DONE |
| **Both buttons** (within 40 ms) | **VOLUME shortcut.** The second press UNDOES what the first single press did (a tap is un-tapped; a level change is restored), then the volume overlay opens. Pads or joystick X set the fader; any button closes. | DONE |
| **Joystick move** | Context: nav, parameter edit, performance, FX push. Game-console nav: one flick = one step, hold = repeat 8/s after 350 ms. | DONE |
| **Tap tempo** | Live from the 2nd tap, rolling average of the latest 2→5 taps, next beat snaps to the latest tap, taps expire after 2 s. Tapped BPM is unquantized. Feeds the master clock and clock-out. | DONE |
| **BPM via editor** | quantized to whole BPM | DONE |

---

## 2. Screen (OLED 128×64 landscape) — universal widgets

| Item | Rule | Status |
|---|---|---|
| Top menu-map strip | REMOVED | DONE |
| Saving star | kept EXACTLY as v2 (spinner while busy, 5-point star otherwise), nothing ever draws over it | DONE |
| Boot | v2 tunnel intro, name = **"PMOR"** | DONE |
| Menu switch | v2 dive/tunnel transition with the menu name, 540 ms | DONE |
| In-play background animation | **NONE.** No waveform, no particles while playing. | DONE (removed) |
| Screensaver | after 25 s with no NAV input (pads do NOT dismiss it; encoder/joystick do). The v2 harmonograph figure rendered as **particles moving along the outline**, size **amplitude-normalized** so quiet and loud both fill the screen, fully audio-reactive, ≤96 pixels, cheap. | DONE |
| Three-card menus | engines show TIMBRE · SEQ · PERFORM as three big rounded rectangles left→right; joystick L/R selects, click enters. | DONE |
| Universal 1-D editor | click a param → full screen, title top, horizontal slider; value **jumps to the stick immediately**; ±55 % deflection already = full range (the edge is hard to hold, the button is hard to click there); joystick click = set & exit; encoder turn = back. | DONE |
| Universal 2-D editor | only where two params pair naturally: joystick = XY pad in a big square. Same set/exit. | DONE |
| 2-D pairs approved | octave amount × octave probability · WIDTH × width probability · ADSR (2×2) · filter cutoff × resonance · filter env · pitch env (glide in × out) · sampler pitch × formant · per-algorithm timbre pairs at discretion. **Nothing 2-D in GLOBALS.** | DONE |
| Quantized vs continuous | per param at discretion; **KEY is continuous (±12 st, microtonal)**; BPM quantized. | DONE |
| DASH | reorganized: accel, joystick raw+calibrated, lidar mm+status, FSR, CPU, BPM, SD, USB, pads held. No magnetometer/gyro rows. | DONE |

---

## 3. Menu ring

`DRUMS · SYNTH · SAMPLER · FX · PATCHES · GLOBALS · DASH` — hard stops, boots on the last-loaded patch. Leaving a menu releases all FX and performance state. — DONE

---

## 4. SYNTH

### TIMBRE
| Item | Status |
|---|---|
| Algorithm selector; each algorithm = its own saved preset. **THIEF = the v2 Mid-Air Thief voice VERBATIM** (30 % FM pluck / 55 % detuned saw stack / 15 % tri stack, per-note VCF snap, swells, 10 % glide notes, krell drift, its own AR envelope, v2 bus drive) — the preset ADSR/filter pages do not apply to Thief; the perf env macro does. The removed "basic saw" is the standalone SAW wave, not Thief's stack. Others: FM, WAVE, PLUCK, SUPER, CZ, FOLD, ADD. | DONE |
| 4 algorithm-specific knobs each | DONE |
| ENVELOPE (real ADSR, 2-D pages) | DONE |
| FILTER cutoff × resonance, key-tracked lowpass; FILTER ENV; PITCH ENV = SEMIS (−24..+24, where the glide starts/ends) × GLIDEIN time, plus GLIDEOUT time; **default SEMIS 0 = no pitch glide on any note** | DONE |
| WIDTH 2-D: width × probability of going that wide (mono → fully generative panning) | DONE |
| Engine volume 0–125 % | DONE |

### SEQUENCING
| Item | Status |
|---|---|
| Modes: **MANUAL** (always first: direct play, no recording) · PLAY (played in, not typed in; live record quantized to the nearest 16th) · GEN. PROB (how likely a generative event fires) and DENSITY (how many per bar) both default to 50 %. Synth SEQ list = MODE, PROB, DENSITY, POLY, OCTAVE×OCTPROB, ARP, ARPRATE, BARS, STEPS. | DONE |
| ARP: plain arpeggiator over held/sequenced notes, rate 1/16 · 1/8 · 1/4 | DONE |
| POLYPHONY (min 1) next to PROBABILITY (master generative amount, also drives the arp) | DONE |
| poly 1 = intelligent (20 ms release on steal, never a hard cut) · poly 2 = 25 %/75 % · poly 3 = 15/50/85 · poly 4+ = even spread; WIDTH ignored at 2 and 3 | DONE |
| GEN = generative arpeggiator over the scale; holding pads constrains it to those notes; release all → full scale | DONE |
| Octave displacement 2-D: amount (continuous, up to all 5 octaves equal) × probability | DONE |
| BARS (≤16) and STEPS (≤16) chosen on the pads (pad N = count), joystick click sets & exits; shrinking keeps hidden data; growing tiles the loop to fill | DONE |
| **REC** (OFF/ON, default OFF) on every SEQ page: in PLAY mode pads only get recorded while REC is ON | DONE (6l) |
| **HANDSEQ** item on every SEQ page: click it → the v2/PO-style step editor: the 16 pads are the 16 steps of the shown bar for the chosen pad/lane; joystick L/R picks the pad being written, U/D picks the bar; OLED shows the 16 steps + playhead; trellis shows the lane; joystick click or encoder = back | DONE (6l) |
| In the SEQ list (drums & sampler): **hold a pad 1.5 s = erase that pad's whole sequence** | DONE (6l) |

### PERFORMANCE
| Item | Status |
|---|---|
| Joystick **Y = direct octave up/down** with glide (Thief-style slide); release → home | DONE |
| Joystick **X = master probability** | DONE |
| **Accel X = note-envelope macro** (plucky/short → long ambient swells, scales the ADSR proportionally) | DONE |
| No recording/erasing here | DONE |

---

## 5. DRUMS — synthesized only, no samples

| Item | Status |
|---|---|
| Pads in quadrants: BL kicks · BR snares/claps · TR hats · TL percussion | DONE |
| 16 sounds per pad. **EMGOR's v2 kits (KIT + KIT2, all 32) are in, ported verbatim including the v2 tanh bus drive**, and come FIRST in every bank; the new recipes fill the rest. Kicks are tuned to F1 (43.65 Hz × key) exactly like v2 — the tom-like kicks were a bug (they sat at F3). Order: KICK = KSUB, KCLUB, 808, 909, KCK1, KCK2, KCK3, K808, K909, KHARD, PHONE, SUB, BOOM, DIST, RUMBL, GLICH · SNARE = SNP1, 808, GATE, CLAP9, SNP2, SNRF, CHOP, CLAP, ESNR, RIM, 909, CLAP8, TIGHT, MODAL, FM, ZAPN · HAT = HCL, CH808, CRASH, HOP, HMD, WASH, EHAT1-3, ECYM, OH808, CH909, OH909, RIDE, NOH, FMCYM · PERC = TOML, CHIRP, COWB, CLAVE, TOMM, TOMH, BELL, BLIP, LZTOM, FMBEL, MTL, COSMO, TOM9, CONGA, LASER, BONGO. No "REV" sounds — reverse is a per-pad setting. **All kicks share the same v2-style tanh saturation** (a fixed pre-gain drive per sound so each hits the tanh like the v2 bus did; the level trim comes after, so matching never changes the grit). Levels are set by **perceived loudness from offline renders**: kicks by 150 ms RMS, snares/hats/perc by A-weighted RMS (so bright snares no longer dominate dull ones), with a hard peak cap. Targets (L+R summed, A-weighted): **kick, snare and perc all 0.19 — the level of the harsh noise snares (ESNR/SNP1) is the reference; hats 0.075**; peak cap raised to 2.2 summed so the kicks and dark toms come up to the snares instead of being held back by their peaks, and at 100 %+ master the drums drive the Fruity clipper. Tool: `PMOR_V3/tools/loud.cpp` + `retune.py` (script in `tools/`) re-fit every `.gain`. | DONE (6o) |
| Default patch: kicks = KSUB (v2 beefiest sub), KCLUB (v2 phone-speaker), 808, 909 · hats = HCL, CH808 (short, 25/75), CRASH, HOP · snares = SNP1, 808, GATE, CLAP9 · perc = 808 tom, chirp, cowbell, clave | DONE |
| Kick = root always; toms = circle-of-fifths-weighted scale degree (never 1; 5 most likely, then 2, 3, 6, 7); chirp = random scale note; snares/noise ignore key | DONE |
| Pan: kick & snare centre; hats 25/75; percussion new random 25–75 % per hit; no WIDTH for drums | DONE |
| TIMBRE: pads play; **hold 500 ms → pad goes white and stays focused after release**; joystick L/R swipes its 16 sounds (hard stops); other pad moves focus; joystick click or encoder turn un-focuses | DONE |
| Per-sound uniform params: LENGTH (scales every envelope time together), PITCH, TONE, REVERSE | DONE |
| Reverse gesture: **two full joystick circles** (720° accumulated, radius > 0.6 the whole way; a flick through the centre or any jump > 90° between polls restarts the count) while focused — CCW = reverse (time-reversed envelopes, works on ANY sound), CW = forward | DONE (fixed: flicks used to count as half-turns) |
| Group volumes KICK/SNARE/HAT/PERC 0–125 % at the bottom of TIMBRE | DONE |
| SEQ: grid, all 16 pads independent lanes, MANUAL first, bars/steps as synth. **GEN is monophonic**: at most one generative hit per 16th (whether the step fires = PROB×density, which pad = groove-weighted pick: kick 1&3, snare 2&4, hats 8ths, perc sparse); held-pad rolls add at most 2 more. Never floods the board. | DONE (6m) |
| PERF joystick: UP = spring reverb · RIGHT = delay throw (~50/50) · DOWN = FL-style distortion (moderate) · LEFT = bitcrush; release cuts the input, tails ring | DONE |
| No accel/gyro for drums | DONE |
| GEN + PERF: a HELD pad becomes a live generative roll on top of the bed (fires each 16th with chance 0.35 + 0.65 × PROBABILITY → straight 16th roll at max, sparse stutter at low); release stops it. Same for the sampler. Pads stay one-shot in every other mode. | DONE |

---

## 6. SAMPLER

| Item | Status |
|---|---|
| 16 slots, 2 s each, on SD | DONE |
| Record starts **exactly on press**, stops on release, anti-click fades | DONE |
| Cannot record over a slot: **hold 5 s = delete** (pad fills as a countdown), then re-record | DONE |
| TIMBRE: filled pad plays; hold 500 ms focuses; focused: **joystick Y = pitch ±1 oct, X = formant**, live/snapping; filter kept, open by default | DONE |
| Reverse gesture (two circles) reverses the audio | DONE |
| WIDTH / poly-pan rules same as synth | DONE |
| TIMBRE and PERF never record/erase sequences; SEQ records in PLAY | DONE |
| PERF: same throws as drums | DONE |
| Level tamed (v2 was hot) | DONE |
| Filled pads always visibly brighter, even in SEQ | DONE |

---

## 7. FX — one master menu, one continuous space

| Item | Status |
|---|---|
| 64 pads: home 16 + 12 per direction. Families: **UP = stutter/glitch/stabs + spatial · RIGHT = modulation · DOWN = distortion/ring mod · LEFT = reverse/time** | DONE |
| Push the stick → that family's 12 pads appear (family row slides to the bottom). **No latch, ever** — the view snaps home on release, and releasing a pad frees the exact effect it pressed even if the stick moved in between (this was the latch bug). A pad held while the stick moves stays held (stack across directions). | DONE (fixed 6b) |
| **It is ONE space, not pages**: trellis colours tilt continuously toward the family from the first millimetre and crossfade layouts around half deflection. Four family colours; home blends all four. | DONE |
| OLED on the FX page: **no catalogue of names.** A compass map of the space with the stick as a dot; only the currently HELD effects are named; FSR bar; REC badge. | DONE |
| Every effect is **FSR-modulated only**. LFO-type effects: FSR = amount, accel X = speed, smooth on release. No effect may go silent or do nothing with the FSR untouched — FSR shapes, it never enables (gates = 50 % duty tightened by FSR; stab = audible, tightened by FSR). | DONE (6c) |
| Weak effects cut (mono, noise, fake spring, fake shifter, etc.); strongest nearest the centre | DONE |
| FILTER pad: hold → joystick L/R sweeps; release leaves it where it is | DONE |
| SIREN = theremin on the lidar: **2 octaves (A3–A5) across the ~9–27 cm hand window, capped outside it**, ~35 ms portamento so it never steps, slow vibrato deepened by the FSR, sine + 2nd harmonic + sub octave, slow stereo drift, fades in. Lidar itself is smoothed at the source (25 Hz sensor → one-pole) for every lidar effect (GRAIN, RING…). | DONE (6d) |
| Automation = the v2 rule: **hold the joystick button on the FX page to record** the held effects + FSR into each step you pass; empty hands scrub those steps clean; release the button = stop recording. Playback is on as soon as anything is recorded. Spans the LONGEST engine loop, not one bar. | DONE (6b) |
| Anti-click fades on every engage/release and dry/wet; 5 ms crossfade on distortion-algorithm switch | DONE |
| **Timing rule:** clocked effects engage on the NEXT boundary of their subdivision (ST4 = beat, ST8 = 8th, ST16/BZZ/gates/repeats = 16th, REV/HALF/DBL = beat); a press within 12 % of a 16th after a boundary counts as on time; release immediate; release before the boundary cancels | DONE |
| Grid-locked effects read the MASTER step phase (`clk.step_phase` is derived from the master `next_step` every block, never a free-running accumulator — that drift made the gates go silent). | DONE (6c) |
| **HALF / DBL / STOP are master-clock-derived**: DBL plays the bar twice per master bar (`shadow = base + 2i mod 16`, second event at mid-step); HALF plays the first half of the bar stretched (`shadow = base + i/2`, fires on even steps); STOP fires nothing. The "1" of every bar always lands together; release at any moment drops straight back onto the master grid. Sampler pitch follows the varispeed. | DONE (this session) — HW |
| Distortion algorithm set locked: TANH_ASYM, FOLD, FUZZ, WARM, FL, CRUSH | DONE |

---

## 8. PATCHES

| Item | Status |
|---|---|
| 4 pages × 16 = 64 patches on SD in the card folder **`PMOR3/` as `Pnn.PMR` + `Pnn.AUD`** (own folder + own names so nothing collides with v2's `PATCHnn.*`); joystick L/R pages; pads = cursor; click = load (sound only, stays on page); hold 5 s = delete with countdown; boots into the last patch (slot remembered in QSPI). **Clicking an EMPTY slot starts a fresh (boot-default) patch there, saved at once, like v2's sessionNew; loading another slot first flushes the current one.** | DONE (6o) |
| No per-page colours (keep the yellow) | DONE |
| Autosave: the whole patch is CRC'd 4×/s — **any** change (sound swipe, sequence hit, gesture, bars/steps, volumes, automation, sampler audio) arms a 2 s debounce and is written; star shows only when written; no card = silent retry every 5 s. **6o hardening: every PMR write is read back and compared byte-for-byte; a failed verify drops the SD to the slow 5 µs SPI clock for the session and retries; the globals record moved from QSPI offset 0 (inside the bootloader's reserved 256 KB) to the last 64 KB of the chip; if `PMOR3/` can't be created the files go in the root as `V3Pnn.*`. DASH bottom row = `SV:ok/fail LD:n op+FRESULT F/S` (LD 1 = boot patch loaded, 0 = no file, 2 unreadable, 3 wrong size/old build, 4 CRC; op = failing step o/w/v/r/m/d; F/S = fast/slow SPI).** | DONE (6o) |
| SD absent → everything degrades silently | DONE |

---

## 9. GLOBALS — 1-D params only

KEY (continuous ±12) · SCALE · BPM · INGAIN · BRIGHT (trellis, default 0.45, 0 → max) · OLEDBRT (contrast 1–32, default 1 = the locked-safe value) · CLOCK (PO SY0–SY5, wired to the click/sync engine; **default SY0 = plain stereo, no click**) · ROTATE (trellis 0/90/180/270) · SWING · FACTORY — DONE

**Global, not per-patch:** master fader, input gain, trellis brightness, OLED brightness, key/scale, swing, BPM, clock mode. Saved to QSPI 3 s after a change. Everything else is in the patch. — DONE

---

## 10. Output & levels

| Item | Status |
|---|---|
| 0–75 % of the old fader = the new 0–100 %; **75 % (0.2848 lin) is the absolute ceiling** | DONE |
| Fader 0–100 % is applied BEFORE the clipper (half volume = clean half volume) | DONE (fixed this session) |
| 100–133 % does not get louder, it only drives the always-on Fruity-Soft-Clipper-style stage (parabolic knee, −4.4 dB default, no lookahead) | DONE |
| Engine and group volumes max 125 % → lean into the clipper | DONE |
| Drums level-matched by A-weighted loudness to the noise-snare reference (see §5), peak cap 2.2 summed | DONE (6o) |

---

## 11. Trellis visuals

| Item | Status |
|---|---|
| Palette = **EMGOR's cyan / pink / yellow scheme** (the v2 lava colours), variations of those plus white/light pink and blue-greens; **no red, no orange anywhere**. FX: UP cyan · RIGHT pink · DOWN yellow · LEFT mint · home light pink. Engines: drums pink, synth cyan, sampler mint, patches yellow, globals light blue, dash lavender. Drums: kicks pink, snares light pink, hats yellow, perc cyan. Delete ramp is pink→white-pink. Lava = v2's blue/yellow/purple. (`ui/palette.cpp`; `research/palette.md` is superseded.) | DONE (6f) |
| Lava-lamp base under everything; no pad ever fully off | DONE |
| Brightness applied exactly once (hw), default noticeably lower than v2 | DONE |
| Focus = white; delete countdown = red ramp; sampler filled = brighter | DONE |
| 90° rotation option | DONE |

---

## 12. Nothing left unimplemented. Hardware checks and one open question

1. Verify on hardware: joystick orientation flags (`config.h`), lidar `0x00D8` register, PO sync click levels, CPU load with many FX stacked, OLED contrast > 1 vs the USB brownout.
2. Open question for EMGOR: a tap currently snaps a STEP boundary to the tap instant; should a tap also be treated as the bar's "1"? (Not changed — not asked.)
3. SD card reliability (hardware / bulk caps at the module) — separate from firmware.

## 13. Suggested staging if rebuilt one step at a time

1. Hardware port + boot (`hw/`, `main.cpp`) — already verbatim, keep.
2. Output stage + drums + level match (§10, §5) — the part that must sound right first.
3. Nav rules (§1, §2, §3) with the three-card engine menus.
4. Synth (§4). 5. Sampler (§6). 6. FX space (§7). 7. Patches/Globals (§8, §9). 8. Visuals (§11), screensaver, intro/dive.
