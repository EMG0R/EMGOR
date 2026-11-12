# SOUNDMAGIC spectral rack for NEPTR — design (locked 2026-10-04)

Re-implementation of Michael Norris's **SoundMagic Spectral** suite (23 free AU
plugins, v1.5) inside NEPTR Phase 4, in Csound, as a chainable spectral rack
on one touch page. Full parameter catalog (exact AU names/ranges/defaults read
from the binaries + his manual text verbatim): `docs/research/soundmagic_spectral_catalog.md`.

Credit: the algorithms, parameter sets and descriptions are Michael Norris's
(michaelnorris.info). The code is ours: his source is **not public** (freeware,
closed, C++/vDSP; a JUCE port is in progress with only Blurring done). We
re-implement from behaviour + descriptors. The page carries a visible
"SoundMagic by Michael Norris" credit line.

## 0. Decisions (locked)

| # | Decision | Why |
|---|---|---|
| D1 | **No parameter is dropped.** Every AU parameter of every plugin exists in the engine, is settable, saved, and reachable from the UI deep sheet. | Emory's rule. |
| D2 | Each effect shows **1–3 "hero" controls** on the page + encoders; the rest live in a per-effect **deep sheet**. Extraneous built-ins (DroneMaker flanger/gate/comb, Shimmer pulse wave, Gate&Hold stages) are deep-sheet only. | "Condensed to the most useful controls, nothing lost." |
| D3 | **Rack architecture = shared analysis per FFT size.** All active spectral effects sharing an FFT size run on ONE `pvsanal` → bin arrays → kernels in rack order → ONE `pvsynth`. Different FFT sizes form serial groups. | One analysis latency however many effects are on; no re-analysis smear between effects. Collapses to his per-plugin behaviour when each has its own size. |
| D4 | **Phase model = his**: `pvsanal` format 1 (amp+phase, Hann, 4× overlap), NO phase unwrapping, per-bin **Randomize phases** switch exactly as his. Not Csound's pvscale/pvsblur family. | That's the SoundMagic sound (comb ringing vs smeared transients trade-off, see catalog §Shared). |
| D5 | **Page = menu 2 SPECTRAL becomes a custom page** (UTILITY-style special case). HG stays at 20. The 5 legacy spectral effects (FORMS, FORMANT, BINSCATR, BIN REDU, BCRUSHER) stay as 5 tiles on the same page; slot buttons keep toggling them exactly as today. | Zero engine nav churn, no HG index move, no test rewrite. |
| D6 | **Params live in one dedicated ftable `giSMP`** (24 effects × 32 = 768 slots), written by OSC `/sp fx idx val`, persisted via its own dual-file ftsave, snapshotted in a parallel preset bank `giSMPBank`. Not in `giNeptrState` (512 cap, 192 free, PB constants fragile). | One mechanism for ~250 params; presets still cover them. |
| D7 | UI→engine for the new effects = **OSC to 7770** (`/st fx 0|1` toggle, `/sp fx idx val` absolute). Hardware encoders drive hero params via the existing `gkLastToggle` dispatch (tap tile = focus). | Absolute writes, Teensy-safe (no `/cc` double-feed), no ch16 CC scarcity. |
| D8 | ~~Time-domain four into MOD/GRANULAR/FILTERING~~ **SUPERSEDED 2026-10-06 by Emory: Norris effects NEVER go into other menus or replace his own (he has his own chorus). They stay on the NORRIS page / web rack only.** | Emory's call. |
| D9 | FFT size keeps his full index 0..7 (512..65536) in the table; page shows 512..8192 chips, larger via deep sheet. Hop = FFT/4, always ≥ ksmps 64. | Fidelity + Pi reality (latency ≈ 2× hop, DroneMaker-class smoothing wants 16k+). |
| D10 | Rack order fixed = page tile order (his guide categories). | Predictable; reorder is a later param. |

## 1. Engine

### 1.1 Files
```
csound/spectral/sm_core.orc        shared UDOs: SM_Common (cutoffs/brightness/randphase/gain/feedback),
                                   SM_Variance (None/Random/LFO/LinkAmp/LinkInvAmp), SM_Peaks (Dolson/Laroche
                                   peak regions), SM_Param (table read), ring-buffer helpers
csound/spectral/sm_<effect>.orc    one kernel UDO per effect: SM_Averaging, SM_Blurring, ... (k[] in, k[] out,
                                   state in per-instance ftables allocated at i-time)
csound/spectral/sm_rack.orc        instrs 241..248 = rack groups for FFT 512..65536; instr 249 = param/OSC/persist
csound/spectral/sm_params.orc      giSMP layout: effect ids, param indices, min/max/default tables, names (strset)
```
Includes added to `neptrPhase4.csd` after the existing effects. Instr block 241–299 (free per audit).

### 1.2 Rack group instr (one per FFT size N, started by $FX_BYPASS-style gate when ≥1 effect of that size is on)
```
aL, aR  = gaSMSendL/R (from signal_chain.orc, SPECTRAL block position)
fL = pvsanal aL, N, N/4, N, 1          ; format 1 = amp+phase
kMagL[], kPhaL[] = pvs2tab fL          ; (same for R)
for fx in rackOrder: if SM_on(fx) && SM_fft(fx)==N: kMag,kPha = SM_<fx>(kMag,kPha, ...SM_Param reads)
                                         then SM_Common(fx) (cutoffs, brightness, randphase, gain, feedback)
fOut = tab2pvs kMag,kPha ...; aOut = pvsynth fOut; dcblock; limit ±0.99
gaSMOutL/R += aOut  (groups chain serially by FFT-size ascending: group N feeds group 2N's send)
```
Per-effect Wet/Dry (where he has one) is applied inside the kernel on the bin arrays against the pre-kernel arrays, as his does. Per-effect **Feedback** = output frame magnitude fed back into the input frame of the same kernel (his "output fed back to input"). **Bin Shift's "instantaneous feedback"** = same-frame re-shift loop (Num instant feedback × ).

CPU: k-loops over N/2+1 bins per hop. Pi 5 budget: 4 simultaneous 2048 effects ≈ one existing pvs effect's cost. Groups not in use cost zero (turnoff2). Both channels processed (his is per-channel; stereo randomness = his rotating-pan character).

### 1.3 Kernel mapping (what each UDO does, from his manual + param set)
| Effect | Core (bin-array operation) |
|---|---|
| Averaging | running mean of magnitudes over n frames (ring of n frames, or exponential equivalent when n>64 for memory) |
| Blurring | one-pole low-pass on per-bin magnitude across frames; coefficient from Blur amount, modulated by SM_Variance |
| DroneMaker | magnitude interpolation between captured frames over Interp length (±variance), optional peak-only capture; extras: comb filterbank pre-stage (scale table 0..9, octaves, resonance), output flanger, gate |
| Emergence | partial tracker (fuzziness, max bins/partial); each tracked partial gets fade-in/hold/fade-out envelope in windows; recycle option |
| Filterbank | keep bins at base·interval^k (Transposition type: cents/Hz/harmonic), Harmonic gain, side bins with gain, Feedback, Wet/Dry |
| Freezing | per-bin hold with Freeze factor (0 = live, 100 = full freeze); threshold-gated capture; zero-under-threshold |
| Gate And Hold | gate type (0..4), threshold (+brightness tilt), N partials; held partials get fade/hold/fade/silence scheduler, decay, bin shift (±var), delay-vs-freq, pulsing, normalize, hold-silence-first |
| Gliding Filters | N band-pass bins of width w that glide (cents/s ± var) with duration/silence scheduler; Wet/Dry |
| Granulation | spectral grains = (bins-per-grain band × grain length) delayed by grain delay (±var), bin shift (±var), density, fade; Feedback |
| Harmonizer | up to 3 interval copies (cents, gain, on/off) via peak-region shift; Transposition type; transpositions per interval (stacking); Feedback; Wet/Dry |
| Partial Glide | tracked partials glide in pitch (cents/s ± var); mirror bin; fade-in; zero transients |
| Pitch Shift | peak-region shift by cents OR fixed Bins to Shift; Rotate (wrap) option; Feedback |
| Pulsing | time-domain-like on/off scheduler in spectral domain with freq/time multipliers, min/max durations, modulo type; only Brightness+Gain common |
| Stretch | bin k → k·alpha·(k/N)^(beta-1) style frequency stretch via peak regions; beta variance (SM_Variance) with quantization |
| Shuffle | swap magnitudes of bin groups within Shuffle range with probability Shuffle factor |
| Bin Shift | integer bin translation ±200, Rotate wrap, instant feedback (×n same frame), Feedback, original/shifted multipliers |
| Shimmer | per-bin amplitude jitter (amount, only-above Hz, LF/HF jitter dB, gate, hold windows ± var) + spectrum-wide amplitude pulse (type, min/max, width, rate) + per-bin delay variance (type, LFO, amount); Brightness, Feedback |
| Tracing | keep/remove N loudest (or quietest) bins; 8 tracing types; two N values; threshold |
| Weave | threads = stable partials above threshold (stability dB over min length) sustained for thread life with hold/fade/silence scheduler; pulse; normalize; Wet/dry |

### 1.4 Params table `giSMP` (768) + meta
`idx = fx*32 + p`. Param order = AU parameter order from the catalog (so the catalog IS the registry). Meta tables `giSMPmin/max/def` same layout; `giSMPname` strset per idx. Effect ids 0..18 = spectral in rack order, 19..22 = time-domain (phase 3), 23 spare.

Persistence: instr 249 compares `giSMP` to a shadow every 2 s → `ftsavek` to `$STATE_DIR/sm_params.ftab` / `.b.ftab` (same dual-file pattern as `state_persist.orc`). Preset capture/recall instrs additionally copy `giSMP` ↔ `giSMPBank[slot*768]` (17 slots). On/off state of each SM effect = `giSMon` 24-wide table, persisted and preset-snapshotted the same way. Stamp constant `SM_STAMP 1` in slot 767.

### 1.5 Control dispatch
- `/st fx val` (ii): toggle. Toggle-ON sets `gkLastToggle = 75+fx` (ids 75..98 claimed) so encoders focus it; also `gkLastToggle29 = 75+fx` if the effect declares a pedal hero (table `giSMpedal`).
- `/sp fx idx val` (iif): absolute write, clamped to min/max.
- Rotations (CC24/25/26/21/22/20/23) for `gkLastToggle == 75+fx`: P1..P7 = that effect's hero params in order (generic branch in `rot_*.orc`: one `elseif (gkLastToggle >= 75 && gkLastToggle <= 98)` → `SM_RotStep(fx, p, dir)` using per-param step = (max-min)/128, doubled with ENC_ROT_EXP). Seven new branches total, not ~170.
- Expression CC29 for `gkLastToggle29 == 75+fx`: writes the pedal hero param absolutely (one generic branch in `expr_targets.src.orc`? NO — the generator lifts bodies verbatim per id; add ONE branch range check at the top of `expr_cc29.orc` before the registry call instead).
- OSC out to UI (8000): `/sm_state fx val`, `/sm_p fx idx val` — **two** `$OSC` sites with a drained change-queue, not 250 sockets (fd-limit incident on record).

## 2. UI — the SPECTRAL page (menu 2, custom)

Canvas 720×1280 portrait. Layout:
```
┌ SPECTRAL ─────────────────────── ┐  header (existing style)
│ [FORMS][FORMANT][BINSCATR][BIN REDU][BCRUSHER]   legacy row = the 5 slot buttons (unchanged)
│ ── SOUNDMAGIC · Michael Norris ──             credit line
│ 4 × 5 tile grid (19 tiles + RACK tile):         tile = name + on-state + 1 mini fader (hero 1)
│   AVERAGE  BLUR    DRONE   FREEZE   GATE&HOLD
│   WEAVE    SHIMMER SHUFFLE PULSING  HARMONIZE
│   BINSHIFT PITCH   STRETCH TRACING  FILTERBANK
│   GLIDEFLT GRANUL  EMERGE  PARTGLD  [RACK]
│ focused-effect strip: hero 1–3 as Max-style faders (existing slider widgets)   ← gkLastToggle-keyed
│ expression strip (existing EXPR_H)
└──────────────────────────────────┘
```
- Tap tile = toggle (`/st`) and focus. **Long-press tile** (or tap the focused tile's name) = **deep sheet**: full-screen scroll list of every AU param (name, value, unit, enum labels) as faders/steppers, sends `/sp`. Back = swipe down. Deep sheet is generated from the param registry (one JSON export `csound/spectral/sm_params.json` produced by a tiny script from `sm_params.orc`; UI loads it, test guards drift) — the catalog→orc→json chain is one source.
- [RACK] tile = page settings: default FFT size for new activations, randomize-phases global override (off/on/per-effect), rack output trim, "chain latency" readout (sum of 2×hop per active group, computed in UI).
- Hooks (from the UI audit): `SPECTRAL_MENU` constant, branches in `_render_menu`, `_signature`, touch-down beside the UTILITY branch, drag target in `touch_move` with `PARAM_TICK` rate-limit; OSC client reuses the wavetable sender (python-osc, 127.0.0.1:7770).
- Swipes left/right still page menus (unchanged engine nav). Web UI: `neptr_model.py` regen + `app.js` special-case for menu 2 (render tiles from `sm_params.json`), phase 2.

### Hero picks (page/encoders; everything else deep sheet)
| Effect | Hero 1 (tile fader, P1) | P2 | P3 | pedal |
|---|---|---|---|---|
| Averaging | Number of frames | Brightness | — | frames |
| Blurring | Blur amount | Blur variance | LFO rate | blur |
| DroneMaker | Interpolation length | Interp variance | Brightness | interp |
| Freezing | Freeze factor | Threshold | — | freeze |
| Gate And Hold | Threshold | Hold length | Decay factor | threshold |
| Weave | Threshold | Thread life | Hold length | threshold |
| Shimmer | Jitter amount | HF jitter | Delay variance | jitter |
| Shuffle | Shuffle factor | Shuffle range | Bins per group | factor |
| Pulsing | On duration | Off duration | On mult freq | on dur |
| Harmonizer | Interval 1 | Interval 2 | Wet/Dry | interval 1 |
| Bin Shift | Bins to Shift | Feedback | Shifted multiplier | bins |
| Pitch Shift | Pitch shift | Feedback | — | shift |
| Stretch | Alpha | Beta | Beta variance | alpha |
| Tracing | Number of bins 1 | Tracing type | Threshold | bins |
| Filterbank | Base frequency | Interval | Number of filters | base |
| Gliding Filters | Pitch glide/s | Number of filters | Filter width | glide |
| Granulation | Grain length | Grain delay | Bin shift | length |
| Emergence | Fade in | Hold | Fuzziness | fade in |
| Partial Glide | Pitch shift/s | Variance | Mirror bin | shift |

## 3. Phases (each = subagent-partitioned, Mac bench before Pi, docs + tests per phase)

**Phase 1 — core + 6 effects + page skeleton.** `sm_core`, `sm_params` (ALL 19 effects' params registered now, even for kernels not yet written — D1), rack instrs, persistence, OSC `/st` `/sp`, generic rotation/pedal branches. Kernels: Freezing, Blurring, Averaging, Bin Shift, Pitch Shift, Shuffle. UI: tile grid, focused strip, deep sheet (generic, so it already exposes every param of the 6). Bench: `scripts/bench/sm_bench.csd` + `analyze_sm.py` scenarios per kernel (bin-identity test with all params at neutral, freeze holds RMS, shift moves centroid). Tests: registry ↔ json ↔ catalog param-count guard, menu-consistency, OSC routing.
**Phase 2 — the rest of the spectral 13.** Harmonizer, Stretch, Tracing, Filterbank, Granulation, Gate And Hold, Weave, Shimmer, Pulsing, Gliding Filters, DroneMaker (+extras), Emergence, Partial Glide (peak tracker shared via SM_Peaks). Web UI menu-2 special case.
**Phase 3 — time-domain four** into existing menus: Super Filterbank → FILTERING, Chorus → MOD, Grain Streamer + Idee Fixer → GRANULAR. Same `giSMP` registry (ids 19..22).
**By-ear on the Pi happens per phase with Emory at the bench; no audio restarts without his say.**

## 4. Risks / open
- Pi CPU at FFT ≥ 16384 with several groups: measure in phase 1 bench; `giSMP` FFT index is per effect so he can trade.
- `pvs2tab`/`tab2pvs` with format 1 (amp+phase): verify Csound 6.18 on the Pi supports format 1 round-trip (format 0 amp+freq is the common path). Fallback: amp+freq with phase randomization done as per-bin frequency jitter — documented deviation if needed.
- Deep-sheet param count ≈ 250: generated, not hand-written, or it rots.
- Docs to update on ship: `docs/neptr_full_reference.md` §5 menu 2, §11 toggle registry (75..98), §16 checklist (+ "spectral kernels go in `csound/spectral/`"), `docs/ui.md` (page + `/st` `/sp` + `/sm_*`), memory `spectral-menu.md`.
