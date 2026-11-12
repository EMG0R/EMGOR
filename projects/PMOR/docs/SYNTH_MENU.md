# SYNTH menu: audit and redesign

Counts are physical actions from the SYNTH ring (menu already selected): E = encoder detent, F = joystick flick, C = joystick click,
S = hold the stick (the value follows the stick, 1 action per value). "Back" = one encoder detent per level, not counted.
Measured on the real nav code by `tests/test_ui.cpp` where marked (T).

## 1. Audit of the old menu (commit d84a734)

Structure: ring (3 cards) -> click -> CARD with **7 horizontal pages** (L/R) -> cursor (U/D) -> click -> editor (stick).
Pages in order: ALGO (ALGO, KEY) / algo knobs (4, named per algorithm) / ENV / FILTER / PITCH / WIDTH / MIX.

| task | old actions |
|---|---|
| change algorithm | C (card) C (edit ALGO) S C = **4** (+E to leave). The value is the stick position over 8 algorithms: imprecise, and entering the editor snaps ALGO to wherever the stick rests (centre = algo 4). |
| tweak an algorithm knob (p1) | C F(R) C S C = **5** |
| CUTOFF/RESO | C F F F C S = **6** (FILTER is page 3) |
| release time | C F F F(D) C S = **6** (ENV page 2, RELEASE is the Y of a pair under SUSTAIN) |
| volume | C F x6 C S = **9** (MIX is the last page) |
| hear a different sound entirely | no concept: only ALGO + hand-tuning; no presets exist |

Why it is hard to navigate:
1. **Seven pages in a strip with only a title** and no indication of the others; the first page is titled ALGO and the second is also titled ALGO (the knobs).
2. **The most-used parameters are at the wrong end**: FILTER is 3 flicks away, VOLUME 6.
3. **Three levels before you touch anything** (ring -> card -> editor) plus the back trip.
4. **ALGO is picked with an analog stick over 8 items**: slow to land on, and it changes value the moment the editor opens.
5. **Pairs hide a parameter**: RELEASE/DECAY/RESO/GLIDEIN/WIDTHPRB can only be reached by moving the cursor onto their X partner.
6. **No presets**: every algorithm has exactly one stored sound. Changing character = editing 10-20 numbers.
7. **Three one-item/two-item pages** (MIX, WIDTH, PITCH) are navigation cost for almost no content.

## 2. Redesign (implemented)

**A. Sound browser (OP-1 style), zero levels deep.** On the SYNTH ring, TIMBRE card, flick the joystick **up or down**:
`L_BROWSE` opens. **L/R = engine (algorithm), U/D = its presets** (`INIT PLUCK PAD LEAD BASS`, 5 per algorithm, 40 sounds).
Pads play so you hear each pick. Click or encoder leaves. The same gesture on DRUMS browses KICK/SNARE/HAT/PERC engines and their 24 presets
(auditioned on every step; tap a pad to choose which pad/engine you are browsing).
The algorithm is no longer picked with the analog stick.

**B. Four pages instead of seven**, ordered by use:
1. **SOUND** : the algorithm's own 4 knobs, KEY, VOL, ALGO (numeric fallback, last)
2. **FILTER** : CUTOFF x RESO, filter envelope, FAMOUNT
3. **ENV** : ATTACK x DECAY, SUSTAIN x RELEASE
4. **MOTION** : SEMIS x GLIDEIN, GLIDEOUT, WIDTH x WIDTHPRB

Pairs stay (XY editing is the fast gesture), but every page title is unique and the first thing you reach after one click is the thing you tweak most.

**C. Factory presets** are transforms of the algorithm's own boot sound (envelopes, filter, width, glide), so the algorithm knobs keep their meaning.
PLUCK on the Karplus-Strong algorithm keeps an instant attack in PAD (a slow attack would just fade the string out).

### New action counts

| task | old | new |
|---|---|---|
| change algorithm | 4 | **2** (F up/down, F right) (T) |
| pick algorithm + PAD preset from another menu | n/a | **6** (E, F, F, F, F, F) (T) |
| algorithm knob p1 | 5 | **3** (C C S) |
| CUTOFF/RESO | 6 | **3 + S** (C F C) (T) |
| release time | 6 | **5** (C F F C S: ENV is flat, RELEASE is item 0 and a plain 1-D slider) or **3** from anywhere on SYNTH via the PERFORM ring: F F then U/D (x1.25 / x0.8 per flick, repeats) |
| volume | 9 | **7** (C F x5 C S; the two-button chord is faster: 1) |
| hear a different character | impossible | **1 flick** |

## 3. Open items (not done)
* Presets are tuned per algorithm from the meaning of each algorithm's own knobs and verified by measurement (attack, sustain, release, brightness, stability on three pitches) in `tests/test_engines.cpp`; not by ear. THIEF only changes width (its sound is a random per-note patch that ignores preset envelopes/filter/knobs).
* Pre-existing defect found while measuring: the voice filter is a Chamberlin SVF whose coefficient is clamped at f=0.9 regardless of Q and keytracked up to ~2.8x, so high cutoff + filter-envelope amount + high notes goes unstable (Nyquist tone). The boot sounds of ADD, PLUCK(Karplus) and FOLD show it on high notes. Fixing it changes the synth goldens, so it is not touched here; the new presets keep clear of it.

## 4. Editors (all menus)
Opening an editor no longer moves the value. The stick is **relative**: it adds an offset to the value the editor opened with, a full throw sweeps the whole range from that anchor, and passing the stick through centre re-anchors (push, return, push again). 3 actions-from-ring numbers above are unchanged.
