# HABIT — Echo Collector (Chase Bliss Habit port) + menu reshuffle

Date: 2026-06-11
Status: IMPLEMENTED 2026-06-12 (all code + bench verified on Mac; on-Pi smoke test
pending — Pi was unreachable; deploy via sync_to_pi.sh + service restart, then run
plan Task 8.3–8.6). Implementation deviations from this spec: OSC namespace is
`/hab_*` not `/habit_*` (UI auto-maps `/{code}_*`); in-loop HP is 80 Hz not 140
(140 ate ~7%/pass at 220 Hz and killed infinite loops); loop gain max 1.08 not
1.05; size is NOT portk-smoothed (quantize+crossfade with settle-gated tap hops
is the smoothing — portk caused tap flapping = pitch smear); wet DC blocker is
`dcblock` (dcblock2 = 254-smp FIR latency). Plan + bench: see
`2026-06-11-habit-echo-collector-plan.md`, `scripts/bench/habit_bench.csd`.
Primary reference: `docs/reference/Habit_Manual_Chase_Bliss.pdf` (the complete official
web guide, pp. 1–51 — signal flow diagram on p. 50 is the ground truth) and
`docs/reference/Habit_MIDI_Manual_Chase_Bliss.pdf`. When this spec and the manual
disagree about *behavior*, the manual wins; when they disagree about *scope*, this
spec wins (we deliberately implement a subset).

---

## 1. Goal

Two changes, one session:

1. **Menu move:** Spectral Delay leaves TIME s1 and takes PITCH s0 (replacing the
   duplicate Formant entry — Formant remains reachable at FILTERING s3, which the
   user confirmed is its only home now).
2. **New effect:** a Chase Bliss Habit-style "delay with a memory" fills TIME s1.
   Toggle ID **36**, new **instr 64**, chain position between DELAY (20) and
   REVERB (21).

Controls available per slot effect: toggle button, slot encoder, the two aux
encoders (logical CC20/CC23), expression pedal (CC29). The Habit has 6 knobs +
3 toggles + dips, so this port locks several of them to a chosen voice (user
decisions recorded in §3).

---

## 2. How the real Habit works (condensed from the manual)

Signal flow (manual p. 50):

```
INPUT ─►(+)─► MEMORY (3-min tape, always recording) ──►(+)─► ECHO ─►[MOD out]─►(+)─► OUTPUT
          ▲     │ primary playhead   │ secondary playhead  ▲    │                │
          │     └──────── SPREAD ────┘ (summed)            │  [MOD in]           │
          │                                          REPEATS◄┘ (feedback loop)   │
          └────────────────────────── FEED ──────────────────────────────────────┘
```

- **MEMORY**: a 3-minute circular buffer recording the input whenever engaged.
  Bypassing dumps it (default; COLLECT/ALWAYS dips change that).
- **SCAN**: moves the primary playhead back into memory. Two modes (dip):
  **AUTO** (default) — an auto-scanner stochastically "wanders off and plays bits
  of the memory, then snaps back to real time"; the knob sets how *often* it scans
  and how *far back* it may wander. **MANUAL** — knob = playhead position
  ("radio dial" / pre-delay into your own history); moving it scrubs audibly.
- **SPREAD**: a second playhead offset behind the first (multi-tap → "two moments
  at once"), rhythmically synced to SIZE.
- **ECHO**: the delay line that the memory playheads feed. **SIZE** = 50 ms–60 s,
  with *clean* adjustment — explicitly no pitch warping when turned. Settable by
  tap tempo on the pedal.
- **REPEATS**: echo feedback; "at maximum you will get stable, infinite build-up,
  like looping."
- **MODIFY**: 6 modifiers, 2 banks × 3, each neutral at noon with different
  CW/CCW behavior: A1 stepped speed (octaves/fifths, reverse 4x↔forward 4x),
  A2 **stability** (tape wow/flutter/filtering; CW side adds noise, CCW side is
  noise-free), A3 trimmer (rhythmic slicing synced to SIZE), B1 smooth speed
  (±2 oct incl. reverse), B2 LP/HP filter, B3 dropper (dropout patterns).
- **IN/OUT/FEED** toggle: modifier placed *inside* the feedback loop (effect
  accumulates each repeat) / at the echo *output* (consistent) / **FEED**: the
  pedal's output is re-recorded into the memory (closed loop, "full Habit").
  Manual's "Tape Delay — old thing sounds" recipe = **MOD A2 + IN**.
- LEVEL = wet level; analog dry-through. Mono I/O pedal.

## 3. Locked user decisions

| Decision | Choice |
|---|---|
| PITCH free slot | Replace Formant at PITCH s0; Formant lives only in FILTERING ("formant should only be in filtering") |
| Control map | **Memory surfer**: exp CC29 = SCAN · slot enc = LEVEL (dry/wet) · CC20 = SIZE · CC23 = REPEATS |
| SCAN mode | **AUTO** — wandering auto-scanner; pedal = how often / how deep |
| Baked-in voice | **Tape stability (A2), noise-free side, IN routing** — wow/flutter + aging filter inside the feedback loop |
| SPREAD | Off (no secondary playhead in v1; see §10 future work) |
| FEED / COLLECT / loop-hold | Out of scope v1 |
| Memory on re-toggle | **Clear on re-engage** (pedal's factory default) |

---

## 4. Menu / handler changes (midi_handler.orc)

All references are LOGICAL CCs (instr 99 remap happens upstream).

### PITCH menu (gkCurrentMenu == 2)

- **CC74 toggle branch** (currently FORMANT, ~line 231): becomes Spectral Delay —
  `$FX_TOGGLE(gkSpecDelBypass'16)`, printks SPECTRAL DELAY ON/OFF. Do **not** set
  `gkLastToggle29` (SpecDel has no CC29 mapping today; unchanged).
- **CC24 rotation branch** (menu 2): becomes
  `$ENC_ROT(gkSpecDelMix'gkStepSpecMix'0'1)` (same line as the old TIME-menu
  rotation, just relocated).
- Remove the FORMANT toggle/rotation from menu 2 entirely. FILTERING s3 (CC71,
  menu 3) keeps Formant untouched — toggle ID 5, CC20 shift dispatch, CC29 — all
  dispatched by `gkLastToggle`, so nothing else moves.
- SpecDel's CC20 (maxdel) / CC23 (FB) dispatch keys off `gkLastToggle == 16` —
  unchanged by the menu move.

### TIME menu (gkCurrentMenu == 6)

- **CC75 toggle branch** (~line 338, currently Spectral Delay): becomes HABIT —
  `$FX_TOGGLE(gkHabitBypass'36)`, and on ON: `gkLastToggle29 = 36` (the exp pedal
  is part of the core mapping, so grabbing the pedal on toggle-on is correct
  NEPTR semantics), printks HABIT ON/OFF.
- **CC25 rotation branch** (menu 6, ~line 737): becomes
  `$ENC_ROT(gkHabitMix'gkStepHabitMix'0'1)`.

### Aux/expression dispatch

- CC20 (`gkLastToggle == 36`): `$ENC_ROT(gkHabitSize'gkStepHabitSize'0'1)` —
  normalized 0–1, mapped exponentially to 50 ms–60 s inside the instr.
- CC23 (`gkLastToggle == 36`): `$ENC_ROT(gkHabitRepeats'gkStepHabitRepeats'0'1)`.
- CC29 (`gkLastToggle29 == 36`): absolute → `gkHabitScan = kVal/127`.

### OSC feedback (bottom of instr 99)

```
$OSC_I(gkHabitBypass'/habit_state'habByp)
$OSC_F(gkHabitMix'/habit_mix'habMix)
$OSC_F(gkHabitSize'/habit_size'habSiz)
$OSC_F(gkHabitRepeats'/habit_repeats'habRep)
$OSC_F(gkHabitScan'/habit_scan'habScn)
```

Update the header comment tables at the top of midi_handler.orc:
`MENU 2 (PITCH): s0=SpecDly(74) …` and `MENU 6 (TIME): s1=Habit(75) …`.

---

## 5. The HABIT effect (csound/effects/habit.orc, instr 64)

Header comment per house style: chain position 20.5, reads `gaHabitSendL/R`,
writes `gaHabitWetL/R`, controls listed, toggle ID 36.

### 5.1 Topology (faithful two-stage engine — approved approach)

```
mono in ──► MEMORY ring buffer ──► playhead(s) ──►(+)──► ECHO ring buffer ──► wet out
(sendL+R)*.5  (always writing)        ▲ scan jumps   ▲                │
                                      │              │   feedback ◄───┤
                                      │           [TAPE-AGE: wow/flutter
                                   AUTO-SCAN          + LP/HP + softclip]
                                     brain            (the IN-loop modifier)
```

Wet is mono, sent equally to `gaHabitWetL/R` (the real pedal is mono I/O and the
NEPTR chain is mono-summed at input). Mix happens in the chain via `ntrpol`,
matching the TIME-menu convention (dry path = the manual's analog dry-through).

### 5.2 Memory stage

- `giHabitMem` ftgen table, **2^23 = 8,388,608 samples ≈ 174.8 s @ 48 k** (mono).
  Power-of-2 so circular wrap is cheap masking. ("3 minutes" rounds to ~175 s; the
  point of the length — phrase-scale recall — is preserved.)
- A-rate write head `kWrite` advances every sample, always recording the mono
  input while the instr is alive (engaged). Bypassed = instr killed by FX_BYPASS
  = recording stops (zero CPU, per house pattern).
- **Clear-on-re-engage without clearing:** keep `kValidSamps`, the count of
  samples written since the instr (re)started, reset at i-time. Scan depth is
  clamped to `min(scanDepth, kValidSamps)`. Stale audio from a previous engage
  is unreachable, so no 8M-sample wipe is needed and re-engage is instant.
- Primary playhead = `kWrite - kScanOffset`. `kScanOffset = 0` → memory stage is
  a transparent pass-through ("leaving scan at minimum = classic delay
  experience," manual p. 4).

### 5.3 AUTO-scan brain (k-rate)

Pedal value `p = gkHabitScan` (0–1, portk-smoothed lightly; it's absolute CC29).

- **Event rate:** stochastic trigger (`metro` with randomized phase or
  `trandom`-style) whose rate scales with p: p=0 → never; p=1 → effectively
  continuous scanning ("at maximum it will be auto-scanning at all times").
  Midpoint ≈ an excursion every few seconds ("the odd departure").
- **Excursion:** on trigger, pick `kScanOffset = uniform(0, p · memLen)` clamped
  to `kValidSamps` (lower p stays recent/harmonious, higher p digs minutes back —
  manual p. 22). Hold the visit for a duration tied to the current SIZE
  (manual p. 8: "SCAN and SPREAD are always rhythmically related … SIZE setting
  the tempo"): visit length = SIZE × small random 1–4 multiple, clamped to
  ~0.25–8 s. Then snap back to offset 0 (real time).
- **Clickless jumps:** two memory read taps + ~30 ms equal-power crossfade on
  every offset change (jump out, jump back). While p is high and excursions
  overlap, this naturally produces the "cloudy" scrubbed quality the manual
  describes — acceptable and authentic.

### 5.4 Echo stage

- `giHabitEcho` ftgen table, **2^22 = 4,194,304 samples ≈ 87 s @ 48 k** — covers
  the 60 s max SIZE.
- **SIZE mapping:** `gkHabitSize` 0–1 → `0.05 · (60/0.05)^x` seconds
  (exponential, 50 ms → 60 s). Default init 0.35 ≈ **0.6 s**. Encoder step
  `gkStepHabitSize = 0.01` → ~7.3%/tick, full sweep in 100 ticks.
- **Click-free, non-warping SIZE changes** (manual is explicit: "no pitch warping
  … the echo simply gets shorter or longer"): dual read taps. Quantize the target
  delay; when it changes, crossfade tap A→tap B over ~50 ms at the new length.
  Never glide the read position (that's vdelay tape-warp — wrong pedal).
- **REPEATS / feedback:** `gkHabitRepeats` 0–1 → loop gain 0–1.02. With the
  in-loop aging filter and soft clip, net loop gain at max settles ≤ 1.0 →
  "stable, infinite build-up, like looping," never runaway. Default init 0.45.
- **IN-loop tape-age modifier (the locked A2 voice, noise-free side, gentle
  depth):** applied to the feedback tap each pass so repeats age cumulatively:
  - **Wow:** slow sine ≈ 0.45 Hz, depth ≈ 2.5 ms, on the feedback read position
    (cubic-interpolated fractional read).
  - **Flutter:** jitter/randi ≈ 7 Hz, depth ≈ 0.35 ms, summed with wow.
  - **Aging EQ:** in-loop `tone` LP ≈ 4.5 kHz + `atone`-style HP ≈ 140 Hz
    (house spring-delay band-limit precedent in delay.orc is HP180/LP4k).
  - **Soft clip:** `tanh` at unity-ish drive — tape compression + the stability
    guarantee for infinite repeats. Plain tanh, NOT the OS4* UDOs: loop gain ≤1
    and the clipper barely engages, so oversampling buys nothing here (same
    rationale as the BroadcastPre no-oversampling decision).
- Output: `dcblock2` + `limit ±0.99` per house style; `balance` against the dry
  send is NOT needed (no resonant gain stage; feedback is self-limiting).

### 5.5 Globals (globals.orc)

```
gkHabitBypass  init 1
gkHabitMix     init 0.5
gkHabitSize    init 0.35      ; ≈0.6 s
gkHabitRepeats init 0.45
gkHabitScan    init 0
gkStepHabitMix     init 0.02
gkStepHabitSize    init 0.01
gkStepHabitRepeats init 0.01
gaHabitSendL init 0
gaHabitSendR init 0
gaHabitWetL  init 0
gaHabitWetR  init 0
giHabitMem  ftgen 0, 0, 8388608, 2, 0   ; ~174.8 s mono memory
giHabitEcho ftgen 0, 0, 4194304, 2, 0   ; ~87 s echo line
```

(ftgen GEN02 zero-fill; table numbers auto-assigned — 300/301 are taken by
SpecDel, avoid fixed numbers.)

### 5.6 Signal chain (signal_chain.orc)

New block at **20.5**, between DELAY (62) and REVERB (70) — Habit echoes can
bloom into the reverb:

```
; 20.5 HABIT (64) — echo collector
gaHabitSendL = aPrevL
gaHabitSendR = aPrevR
$FX_BYPASS(gkHabitBypass'64'gaHabitSendL'gaHabitSendR'gaHabitWetL'gaHabitWetR'habit)
aPrevL ntrpol gaHabitSendL, gaHabitWetL, gkHabitMix
aPrevR ntrpol gaHabitSendR, gaHabitWetR, gkHabitMix
```

Instr **64** is free (62 delay, 65 ringmod, 70 reverb; registry §11 of
neptr_full_reference.md confirms).

### 5.7 csd

`#include "effects/habit.orc"` added to neptrPhase4.csd alongside the other
effect includes.

---

## 6. UI (neptrPhase4_UI.py)

`MENUS` table (display rows are ordered s4→s0 left→right, cf. the TIME row):

- PITCH row currently has only 4 entries (Formant was already dropped from the
  UI but the slot indexing may be off by one) — make it 5, explicit:
  `[("h90","H90"), ("pf","P-FORK"), ("gps","GPS"), ("ps","OLD PS"), ("sdl","SPECDLY")]`.
  Verify slot→display index mapping against the TIME row convention while in
  there (pre-existing inconsistency, fix in passing).
- TIME row: replace `("sdl","SPECDLY")` with `("hab","HABIT")`:
  `[("mod","MOOD"), ("grn","GRANULAR"), ("vrb","REVERB"), ("hab","HABIT"), ("dly","DELAY")]`.
- Add `/habit_*` OSC handlers + param display (mix/size/repeats/scan), mirroring
  the sdl pattern. Size display formatted adaptively ("250 ms" / "12.4 s").

---

## 7. Doc/memory updates

- `docs/neptr_full_reference.md`: §5 menu tables (PITCH s0, TIME s1), §6 CC20/23
  table (36 Habit: size / repeats), §7 CC29 table (36 → scan), §8 chain (20.5
  HABIT 64), §11 effect summary + toggle registry (36 HABIT; next free: 37).
- Memory file `neptr-add-effect-checklist`: bump toggle-ID note.

---

## 8. Testing & verification

1. `csound --syntax-check-only -o null csound/neptrPhase4.csd` on the Mac.
2. **Offline bench** (run in main session — subagent sandboxes can't exec
   csound): a small csd that includes globals + habit.orc, feeds a click train +
   a sine phrase, and renders scenarios: (a) scan=0 plain echo at three SIZEs —
   verify no pitch warp on SIZE change (spectrogram/listen), repeats decay;
   (b) repeats=1.0 for 60 s — verify stable amplitude (no runaway, no
   silence-collapse); (c) scan=0.7 — verify excursions resurrect earlier
   material and crossfades are click-free; (d) toggle off/on — verify old
   material is unreachable (clear-on-re-engage).
3. On-Pi: `sync_to_pi.sh`, restart, then `claude_midi.sh` (PHYSICAL CCs:
   menu-next = cc74 127; TIME slot1 toggle = cc73 127): toggle Habit, sweep
   cc29, turn CC-encoders, watch journal + OSC UI. Verify SpecDel from PITCH
   menu (slot0 toggle = physical cc70) still works end-to-end.
4. CPU: `top` on Pi while engaged; budget < 8% of a core (plain table I/O +
   filters; no FFT).

## 9. Risks / mitigations

- **8M-sample tables**: ~100 MB RAM total in doubles — fine on the Pi 5; ftgen
  at orchestra load happens once at boot (negligible vs current load time).
- **Crossfade complexity** (scan jumps × size changes × wow/flutter all touching
  read positions): keep the three concerns in separate code paths — scan xfade
  on the MEMORY taps, size xfade on the ECHO taps, wow/flutter ONLY on the
  feedback tap. They never share state.
- **Infinite repeats drift**: the in-loop LP nibbles highs each pass, so at
  repeats=max a "loop" slowly darkens. That's authentic tape behavior and the
  chosen voice; not a bug.
- **CC29 is sticky** (`gkLastToggle29`): toggling Habit ON steals the pedal from
  e.g. wah; standard NEPTR behavior, no special handling.

## 10. Future work (explicitly out of scope v1)

- SPREAD secondary playhead (multi-tap / "two moments at once").
- MANUAL scan mode (radio-dial scrub) behind a UI/OSC switch.
- FEED routing, COLLECT overdub, loop-hold footswitch semantics, ramping.
- Other modifiers (stepped/smooth speed, trimmer, filter, dropper).
