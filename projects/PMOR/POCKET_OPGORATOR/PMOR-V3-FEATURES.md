# PMOR v3 — Complete Feature Brief

Everything EMGOR asked for in the 2026-09-14 planning session, laid out so an agent can
implement it exactly — one stage at a time if wanted. Each item carries a status against
the current `PMOR_V3/` tree (stage 7i):
**DONE** = implemented, **HW** = implemented, needs hardware verification, **TODO** = not done.

Ground rules for whoever implements:
- Nothing beyond this document. No extra visuals, menus, features, or "improvements".
- All buttons act on PRESS (impact), never release.
- Locked hardware code in `hw/` is verbatim from PMOR_V2 — do not touch it.
- Suggestions are not decisions. If it is not in here, ask EMGOR.
- Subagents, if any, run on Sonnet.

**Build + flash: `DaisyExamples/seed/PMOR_V3/flash.sh` — use it, nothing else.** It builds, opens the CDC port with DTR asserted, sends `dfu`, polls for the DFU device every 250 ms and writes `build/PMOR_V3.bin` to `0x90040000` (`APP_TYPE = BOOT_SRAM`). Do NOT use `printf 'dfu\n' > /dev/cu.usbmodem*`: a shell redirect opens the port without asserting DTR, macOS never raises the line, the bytes are never delivered, and no DFU device appears — silently, with no error. The port must be opened with pyserial and `dtr = True; rts = True` before writing `dfu\r\n`. Success is the literal string `File downloaded successfully`; dfu-util then prints `Error during download get_status` as the board resets — benign, never gate on the exit code. Poll for the DFU device, never sleep for it (the window is short). If the board is truly unresponsive, BOOT + tap RESET gives the ST ROM DFU, which only exposes internal flash 0x08000000 and rejects the app with "Last page … is not writeable" — then `make program-boot` first, then the app.

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

| **Stuck pads fixed** (7i): the keypad FIFO is drained fully (up to 32 events per poll, and re-polled on the next loop pass if more are waiting — was 8 per 20 ms with ≥32 thrown away); a bus stretch error now RELEASES every held pad instead of dropping the events (a cut beats a drone); and any pad "held" with zero keypad activity for 8 s is force-released. `hw/trellis.cpp` is the one place this lives. | DONE (7i) |
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
| Screensaver | after 25 s with no NAV input (pads do NOT dismiss it; encoder/joystick do). The v2 harmonograph figure rendered as **particles moving along the outline**, size **amplitude-normalized** so quiet and loud both fill the screen, fully audio-reactive, ≤96 pixels, cheap. 6x: figure 25 % larger (~104×58 px at full swing). | DONE (6x) |
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
| Algorithm selector; each algorithm = its own saved preset. **THIEF = the v2 Mid-Air Thief voice VERBATIM** **minus its dry saw stack (6p: 40 % FM pluck / 60 % detuned tri stack — the plain saw branch is gone for good)**, per-note VCF snap, swells, 10 % glide notes, krell drift, its own AR envelope, v2 bus drive — the preset ADSR/filter pages do not apply to Thief; the perf env macro does. Others: FM, WAVE, PLUCK, SUPER, CZ, FOLD, ADD. | DONE |
| **KEY mirror under ALGO** (7c): the synth ALGO page has a second item, KEY (−12…+12 semitones, unquantised) — the GLOBAL key, editable on the fly from the synth without visiting GLOBALS. | DONE (7c) |
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
| **Accel X = note-envelope macro** (plucky/short → long ambient swells). 6p: full swing at ±0.4 g (~25° tilt, smoothed) instead of ±1 g; range ×0.06 … ×36 on the envelope times (6t; Thief's attack also becomes a real swell on the ambient side); on the plucky half the held sustain falls to 0 so notes die under the finger; on the ambient half Thief's release stretches up to ×36. | DONE (6p) |
| **Accel Y filter — REMOVED from the PERF card** (6y, EMGOR): `PERF_ACCEL_FILTER 0` in config.h; the preset cutoff is untouched in PERF. The code stays behind the flag (positive Y = open, negative = closed, floor 25 %) for when he wants it back. Rule for the DASH `A` row: first value negative = shorter notes, positive = longer. |
| **Joystick throw is LEARNED** (7b): each axis normalises against the largest deflection it has actually seen (floor 12000 counts), so ±1.0 is always the real physical edge — the octave curve (below) genuinely spans 80 % of the stick whatever the ADC range is. | DONE (7b) |
| **Joystick Y = octave** (6s): continuous and exponential over 80 % of the throw — shift = 2·t^2.5 octaves with t = \|y\|/0.8, so small movements barely move the pitch and the last stretch does the jump; ±2 octaves at 80 %+; 80 ms glide on every sounding note; symmetrical up/down | DONE (6s) |
| **Accel axes** (6w, final): NO swap (`ACCEL_SWAP_XY 0`). Note length = accel X exactly as it was at 6q (short leaning left, long leaning right). Filter = accel Y with `ACCEL_INVERT_Y 1` (upright = open, leaned over = closed), and the sweep is floored at 25 % — only the top 75 % of the range is used so it never fully chokes. Do not touch these flags again without EMGOR. | DONE (6w) |
| **Synth pad pitch base** (6u): the 16 pads span scale octaves 0–2 (bottom-left = the lowest root), one octave lower than before; drums' kick root unaffected. | DONE (6u) |
| **Chord-constrained GEN keeps its rate** (6z): the per-pad dice are normalised by the number of candidate pads, so a latched 1–3 pad chord generates as many notes as the open 16 — this was the "suddenly sparse until I re-enter PERF" bug (re-entering cleared the chord). | DONE (6z) |
| **Live-play de-doubling** (6z): a hand tap and a sequenced hit of the same pad within 0.45 of a 16th of each other sound ONCE — a tap just before the step cancels the step, a tap just after a step is swallowed. All three engines. | DONE (6z) |
| **Ambient = swells** (7b, final): at full macro every note fades IN over ~1.5 s, is HELD by the sequencer until the swell has arrived, then fades OUT over ~3.5 s; in between the macro interpolates log-wise from the note's own times. Thief's per-note VCF is kept from closing so they stay loud. **7h — the tilt NEVER touches sequencing** (EMGOR: it was breaking the arp/generator): no rate/cap/grid coupling any more. Instead the envelope adapts to the sequencing: a sequenced/arp note's swell attack is budgeted to ~45 % of `poly × grid interval` (so at 8ths/poly 8/120 BPM ≈ 0.9 s) and the note is held 1.3× that — it always arrives at full level and never starves the pool; hand-played notes are unbudgeted. If the pool is full anyway the oldest note is stolen with a 120 ms fade (was 20 ms). | DONE (7h) |
| **POLY** (7a): 1–8 voices, default 8 (16 glitched the board in PERF with spread notes). Voice stealing: past the cap the OLDEST note gets a 20 ms release; when the 4 spare slots are all releasing, the QUIETEST releasing voice is taken, never a loud one; WASH never slows a stolen voice's release. The editor shows the plain voice count. Sampler POLY also 1–8. | DONE (7a) |
| **PERF pads in GEN / arp = note selection only** (6z): with the synth lane in GEN, or PLAY with ARP on, pads do not play directly — they only set the chord the generator/arp draws from. MANUAL and plain PLAY: pads play. | DONE (6z) |
| **Chord memory in synth PERF** (7c): a pad press with nothing else down starts a new combination; every pad pressed while any pad is still down joins it; releasing never removes a note. GEN and the arp play from that set until a fresh press-run in PERF replaces it — **it persists when you leave PERF / change menus** (EMGOR: to be used as a key-centre control later). | DONE (7c) |
| **Generative synth grid = ARPRATE, never finer than 1/8** (6s): default ARPRATE is now 1/8; GEN fires on that grid (was every 16th) | DONE (6s) |
| **Joystick X = generative probability, logarithmic** (6q): centre 50 %, hard left 1.5 % (most of the left travel lives in the sparse region), right climbs linearly to 100 % | DONE (6q) |
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

| **Drum/sampler step entry = v2's seq_place, held-pad edition** (7e): on the SEQ card (from the card ring or inside the list), HOLD a pad (≥0.5 s) — it becomes the write target (glows white) and the other pads become the 16 steps of the bar the playhead is in, for that pad's lane, shown in the lane colour with the playhead sweeping (the v2 PO display). Press = toggle, no sound. Joystick click while holding = toggle the held pad's own step. Release = back to playing. The old "hold 1.5 s erases the lane" is gone (see CLEAR); HANDSEQ remains for the synth only. | DONE (7e) |
| **CLEAR** item in every SEQ list (7e): click = wipe that engine's whole sequence. | DONE (7e) |
| **Held pads belong to the engine whose page is showing** (7e): on FX / PATCHES / GLOBALS pages, held pads are nobody's — the drum GEN roll and sampler roll no longer fire from FX-page or synth-page presses. | DONE (7e) |
| **PERF values persist on exit** (7e, confirmed): probability (stick X), octave shift (stick Y), env macro (accel X) and the chord latch all keep their last value when leaving PERF; only the drum/sampler momentary throws (spring/delay/dist/crush) release. | DONE |
| **Per-pad drum VOL** (7g): every drum pad has its own volume (0–200 %, default 100 %, patch-serialised, shown as VOL on the pad's TIMBRE page). On a focused pad, joystick L/R still swipes the 16 sounds and **U/D flicks step the volume ±10 %** with a preview hit. Patch magic → `OPG7` (first boot starts fresh once). | DONE (7g) |
| **Drum level-setting page** (7e): `tools/drum_levels.html` (published as the "PMOR Drum Levels" artifact) plays every rendered sound through a JS mimic of the exact output chain (per-sound gain × group × engine → master fader/drive → FL soft clipper → MASTER_UNITY) with a LISTEN gain for headphones; EXPORT gives a JSON of multipliers; `python3 tools/apply_levels.py levels.json` multiplies them into `drum_sounds.cpp`. Renders come from `tools/render.cpp` (host build, see tools/README). **7f: EMGOR's first pass applied (`tools/levels/levels_2026-09-15.json`); the page's renders were regenerated so 100 % = the 7f firmware.** | DONE (7f) |

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

## 7. FX — the PMOR_V2 punch-in banks, verbatim (6z)

EMGOR, 2026-09-15: the curated 64-effect space was not performable — *"take the actual exact FX menus from v2 and make sure they work with the new engines"*. So:

| Item | Status |
|---|---|
| **8 banks × 16 pads = v2's FX1..FX8 exactly** (names, order, pad positions): FX1 `ST4 ST8 ST16 ST32 / REV CHOR PAN CRUSH / HALF DBL BZZ MUTE / FLT WASH DIST STOP`, FX2 `DUB GVERB TGATE FLANG / RING FOLD COMB VOWEL / ISO-LO ISO-MD ISO-HI PHONE / GRAIN WOW DOWN SCAT`, FX3 `PLATE SHIMR PONG SLAP / PHASE WAH TREM HAAS / VIBRA DIMEN SPRNG SIREN / WARM SUB AIR PUMP`, FX4 `OCT+ OCT- DSTRY REPT / RGRN FREEZ METAL FRMNT / FSHFT FUZZ GSEQ SRAMP / VSTOP RADIO NGATE SHMOD`, FX5 `RPT2 RPT6 SHUF Q6/8 / PUNCH FILL BUILD BALL / LPF HPF SQUSH SCRTC / SCRTF GLCH BLIND S&H`, FX6 `DELAY PNGPG FADE FBACK / DRONE FREEZ UNI UNILO / DETUN PLFO BND+ BND- / WARBL 5TH SUB LOOP`, FX7 `GATE4 GATE8 GAT16 TRNCE / TREM PAN PUMP SWELL / NOISE WIDE MONO BRITE / DARK DRIVE SGATE KILL`, FX8 `RAT2 RAT3 RAT4 RAT8 / STRUM TRNC2 STAB RINGL / RINGH VIBR CHOP HALF / DBL GLCH REVG SIL`. | DONE (6z) |
| **DSP = PMOR_V2.cpp L2518-3353 copied verbatim** into `engine/fx_v2.cpp` (one per-sample function; the v2 globals became `FxV2Blk` fields; buffers in SDRAM, zeroed at init). Nothing was "improved". The v2 ordered master chain (RING → DIST → FLT → CRUSH) is intact. Effects sit on the full engine bus (v2 fed them the tanh'd mix; v3 hands in the drums+synth+sampler bus before the master fader/clipper). | DONE (6z) |
| Bank-0 "gridTick-side" effects live in the host (`engine/fx.cpp`): ST4/8/16/32 + BZZ = grid-locked retrigger of the content registers; HALF/DBL/STOP = the master-clock-derived shadow steps (see §1); WASH = every synth voice's release forced to v2's coefficient (`0.00012 − FSR·0.00008`). REV = the v2 reverse read of the capture ring, anchored at the step boundary. | DONE (6z) |
| **v2 LIVE LATCH (L1792/L1999-2011)**: a pad played by hand between steps goes into the stutter/BZZ registers, and the registers stay sticky while a retrig FX is held — so a stutter over nothing sequenced still has material. (Was: silence.) | DONE (6z) |
| **A stutter repeats the SAME note**: each pad's last synth note is remembered (pitch, pan, algorithm and the whole per-note character — Thief's type/filter/envelope roll, FM ratios…) and ST/BZZ re-fire that copy; never a new roll of the dice. Drums/sampler re-fire the same pad. | DONE (6z) |
| Navigation: joystick L/R flick = previous/next bank (hard stops, no wrap; v2 used the right encoder). Pads = press/hold, release frees exactly the effect that pad pressed even if the bank changed. Never latches. | DONE (6w/6z) |
| Trellis: one colour per bank from EMGOR's palette (no red/orange), held pads full, idle dim, a touch brighter per row downward. OLED = the v2 FX page: `< FXn >` header, up to 3 held names, FSR bar, `FSR nn%  BPM:nnn`, REC badge. | DONE (6z) |
| **Accel tilt filter on the FX page** (v2 L4993-5002, mirrored per EMGOR): leaning the way that makes notes SHORT (accel X negative) closes it, leaning the way that makes them LONG opens it; 300 Hz–12 kHz log, Q 0.62, on the whole bus before the FX; only moves while the FX page is showing, frozen where you left it otherwise; starts fully open. | DONE (6z) HW |
| Automation = the v2 rule (hold the joystick button on the FX page to record the held banks + FSR per step; empty hands scrub; release = stop). Playback is on as soon as anything is recorded; **spans TWICE the longest engine loop** (7d: a 1-bar pattern gives the FX 2 bars of automation; capped at 16 bars). Patch-serialised as `uint16[steps][8]` (patch magic bumped to `OPG6` — earlier cards start fresh once). | DONE (6z) |
| Anti-click engage/release fades and quantised engage from the curated build are GONE (v2 had none except MUTE/gates). | — |

## 8. PATCHES

| Item | Status |
|---|---|
| 4 pages × 16 = 64 patches on SD in the card folder **`PMOR3/` as `Pnn.PMR` + `Pnn.AUD`** (own folder + own names so nothing collides with v2's `PATCHnn.*`); joystick L/R pages; pads = cursor; click = load (sound only, stays on page); hold 5 s = delete with countdown; boots into the last patch (slot remembered in QSPI). **Clicking an EMPTY slot starts a fresh (boot-default) patch there, saved at once, like v2's sessionNew; loading another slot first flushes the current one.** | DONE (6o) |
| No per-page colours (keep the yellow) | DONE |
| Autosave: the whole patch is CRC'd 4×/s — **any** change (sound swipe, sequence hit, gesture, bars/steps, volumes, automation, sampler audio) arms a 2 s debounce and is written; star shows only when written; no card = silent retry every 5 s. **6o hardening: every PMR write is read back and compared byte-for-byte; a failed verify drops the SD to the slow 5 µs SPI clock for the session and retries; the globals record moved from QSPI offset 0 (inside the bootloader's reserved 256 KB) to the last 64 KB of the chip; if `PMOR3/` can't be created the files go in the root as `V3Pnn.*`. DASH bottom row = `SV:ok/fail Q:n LD:n op+FRESULT F/S` (Q = QSPI mirror writes; LD 1 = boot patch loaded from SD, 5 = restored from the QSPI mirror, 0 = no file, 2 unreadable, 3 wrong size/old build, 4 CRC; op = failing step o/w/v/r/m/d; F/S = fast/slow SPI).** **6w — THE ROOT CAUSE, FIXED: `Patch` has 4 bytes of trailing padding after `crc32` (8-byte alignment from `fx_auto_mask`), so the CRC was computed over `sizeof-4` = including the crc field itself (zero at write time, non-zero at read time) → every load since 6e failed its CRC check and the board came up fresh every boot while the saves themselves were fine. CRC is now over `offsetof(Patch, crc32)`; loads accept both the new and the old CRC so existing cards still open.** **6s: the current patch is ALSO mirrored into on-chip QSPI flash (last 256 KB, ~5 KB record with magic + CRC) on every autosave, patch load and new patch — only when it actually changed. At boot the mirror wins for the boot slot (it can never be older than the SD copy), then SD, then a mirror from another slot. So state comes back after a reboot even when the SD write path fails or the card is out; sampler audio still lives only on the card.** | DONE (6s) |
| SD absent → everything degrades silently | DONE |

---

## 9. GLOBALS — 1-D params only

KEY (continuous ±12) · SCALE · BPM · INGAIN · BRIGHT (trellis, default 0.45, 0 → max) · OLEDBRT (contrast 1–32, default 1 = the locked-safe value) · CLOCK (PO SY0–SY5, wired to the click/sync engine; **default SY0 = plain stereo, no click**) · ROTATE (trellis 0/90/180/270) · SWING · FACTORY — DONE

**Global, not per-patch:** master fader, input gain, trellis brightness, OLED brightness, key/scale, swing, BPM, clock mode. Saved to QSPI 3 s after a change. Everything else is in the patch. — DONE

---

## 10. Output & levels

| **Globals persist across reboots, independent of patches** (7c): KEY, SCALE, **BPM**, input gain, trellis + OLED brightness, clock mode, swing and the last patch live in the on-chip QSPI record (top 64 KB), written 3 s after the last change; patches never touch them. BPM is new in 7c (record version 2 — the first boot on this build resets globals to defaults once). | DONE (7c) |
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
