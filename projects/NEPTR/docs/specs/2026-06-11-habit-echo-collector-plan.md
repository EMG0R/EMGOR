# HABIT Echo Collector + Menu Reshuffle — Implementation Plan

> **STATUS 2026-06-12: Tasks 1–7 COMPLETE and verified** (full-csd syntax check,
> UI parse, all 4 bench scenarios pass — including two real fixes found by the
> bench: size-portk tap flapping and the HP140 infinite-loop killer; wet output
> switched dcblock2 → dcblock). **Task 8 (deploy + on-Pi smoke) PENDING** — Pi
> unreachable at implementation time; user deploys via sync_to_pi.sh + restart,
> then run steps 8.3–8.6 only (8.1 done, full tree incl. concurrent TAPE/NAM
> work compiles green).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> **NOTE:** This project is NOT a git repository — there are no commit steps. Each task ends with a verification step instead. Do not `git init` without asking the user.
> **NOTE:** Subagent sandboxes cannot exec csound — all bench/render steps must run in the main session. Do NOT let agents touch the Pi while the demiurge service may be live.

**Goal:** Move Spectral Delay to PITCH s0 (replacing the duplicate Formant entry) and add a Chase Bliss Habit-style "delay with a memory" at TIME s1 (toggle ID 36, instr 64).

**Architecture:** Faithful two-stage engine per the official manual (`docs/reference/Habit_Manual_Chase_Bliss.pdf`, signal flow p. 50): a ~175 s always-recording mono memory ring buffer with a stochastic AUTO-scan brain (crossfaded playhead jumps), feeding an ~87 s echo ring with dual-tap click-free SIZE (50 ms–60 s, never pitch-warps) and a tape-aging feedback loop (wow/flutter + LP/HP + tanh = the locked "MOD A2 + IN" voice). Spec: `docs/specs/2026-06-11-habit-echo-collector-design.md`.

**Tech Stack:** Csound orchestra files (.orc) in the NEPTR Phase 4 chain (Pi 5, raw ALSA, sr=48000 ksmps=256), Kivy/OSC UI (Python), deploy via rsync + systemd restart.

**Controls (all logical CCs):** toggle = TIME menu s1 button (CC75) · slot encoder CC25 = mix · aux CC20 = SIZE · aux CC23 = REPEATS · expression CC29 = SCAN. OSC namespace `/hab_*` (matches the UI's auto-mapped `/{code}_{param}` convention — the design doc said `/habit_*`; `hab` is the code, this plan wins).

---

## Strategy notes (read first)

- **Why two separate ring buffers:** the manual is explicit that MEMORY (your clean playing) and ECHO (the repeats) are different stores; feedback degradation must never contaminate the memory, or scan resurrects mush instead of your playing.
- **Power-of-2 tables + phasor:** `phasor(sr/2^N)` increments by exactly `1/2^N` per sample (exact in binary floating point) — zero drift, cheap wrap. Memory = 2^23 samples ≈ 174.8 s, echo = 2^22 ≈ 87.4 s @ 48 k.
- **Clear-on-re-engage without wiping 8M samples:** `kValid`/`kEValid` counters (samples written since instr start, reset by `init` on every FX_BYPASS event-start) clamp/gate all reads. Stale audio from a previous engage is simply unreachable.
- **No pitch warp on SIZE** (manual: "the echo simply gets shorter or longer"): never glide a read position — quantize size to a 5 ms grid and equal-power crossfade between two static taps (~50 ms). Same trick for scan jumps (~30 ms, two memory heads).
- **Tape voice lives ONLY in the feedback path** (IN routing): first echo is clean, each recirculation passes wow/flutter (a 5 ms-centred `vdelay3` wobble) + HP140/LP4500 + `tanh` once more. Plain `tanh`, NOT the OS4* UDOs — loop gain ≤ ~1.05 and the clipper barely engages (same no-oversampling rationale as BroadcastPre).
- **Stable infinite repeats:** REPEATS 0–1 → loop gain 0–1.05. Above unity, growth is arrested by tanh compression → settles into a stable, slightly saturated loop that slowly darkens (authentic tape, not a bug).
- **Three read-position concerns never share state:** scan xfade = MEMORY taps; size xfade = ECHO taps; wow/flutter = feedback tap only.

---

### Task 1: Globals (csound/globals.orc)

**Files:**
- Modify: `csound/globals.orc`

- [ ] **Step 1.1: Add Habit control globals** — insert after the SpecDel block (`gkSpecDelPrev init 1`, ~line 324):

```csound
; --- HABIT (echo collector, toggle 36, instr 64) ---
gkHabitBypass  init 1
gkHabitMix     init 0.5
gkHabitSize    init 0.35     ; normalized; 0.05*1200^x s -> ~0.6 s
gkHabitRepeats init 0.45
gkHabitScan    init 0        ; expression pedal CC29
```

- [ ] **Step 1.2: Add buses** — next to the SpecDel bus inits (~line 572):

```csound
gaHabitSendL init 0
gaHabitSendR init 0
gaHabitWetL  init 0
gaHabitWetR  init 0
```

- [ ] **Step 1.3: Add encoder steps** — next to `gkStepSpecDelFB` (~line 693):

```csound
gkStepHabitMix     init 0.02
gkStepHabitSize    init 0.01   ; ~7.3%/tick exp sweep, full range in 100 ticks
gkStepHabitRepeats init 0.01
```

- [ ] **Step 1.4: Add ring-buffer tables** — near the SpecDel ftgen tables (~line 195):

```csound
; HABIT ring buffers. Power-of-2 so phasor wrap is exact and table3 iwrap works.
giHabitMem  ftgen 0, 0, 8388608, 2, 0   ; 2^23 ~= 174.8 s mono memory @48k
giHabitEcho ftgen 0, 0, 4194304, 2, 0   ; 2^22 ~= 87.4 s echo line @48k
```

- [ ] **Step 1.5: Verify** — run `csound --syntax-check-only -o null csound/neptrPhase4.csd` from the project root. Expected: same exit status/warnings as before the change (globals alone add no new errors).

---

### Task 2: The effect (csound/effects/habit.orc + csd include)

**Files:**
- Create: `csound/effects/habit.orc`
- Modify: `csound/neptrPhase4.csd:44` (include list)

- [ ] **Step 2.1: Create `csound/effects/habit.orc`** with exactly:

```csound
; HABIT — echo collector (Chase Bliss Habit port; see docs/reference/Habit_Manual_Chase_Bliss.pdf
; and docs/specs/2026-06-11-habit-echo-collector-design.md)
; Signal chain position: 20.5 (between DELAY 62 and REVERB 70)
; Reads:  gaHabitSendL/R
; Writes: gaHabitWetL/R (100% wet; ntrpol mix in chain)
; Bypass: gkHabitBypass (toggle ID 36)
; Controls: gkHabitMix (slot enc CC25) · gkHabitSize (CC20, 0-1 -> 50ms..60s exp)
;           gkHabitRepeats (CC23, 0-1 -> loop gain 0..1.05; max = stable infinite)
;           gkHabitScan (CC29 expression, AUTO-scan amount: how often + how deep)
; Voice (fixed): tape stability (manual's MOD A2, noise-free side) IN the feedback
; loop — wow/flutter + HP140/LP4500 + tanh applied once per recirculation, so
; repeats age cumulatively. First echo is clean (authentic IN routing).
; Memory clears on re-engage via valid-sample counters (no table wipe needed).

instr 64
  ;--- mono input: the real pedal is mono I/O; NEPTR's chain is mono-summed anyway
  aIn = (gaHabitSendL + gaHabitSendR) * 0.5

  ;--- smoothed controls
  kScan     portk gkHabitScan, 0.05
  kSizeNorm portk gkHabitSize, gkPortTime
  kRepeats  portk gkHabitRepeats, gkPortTime
  kFb       = kRepeats * 1.05
  kSizeSec  = 0.05 * pow(1200, kSizeNorm)   ; 50 ms .. 60 s exponential

  ;=====================================================================
  ; MEMORY stage — ~175 s ring, always recording while engaged
  ;=====================================================================
  iMemLen = ftlen(giHabitMem)
  aWPhs   phasor sr / iMemLen        ; exact 1-sample steps (power-of-2 table)
  aWNdx   = aWPhs * iMemLen
  tablew  aIn, aWNdx, giHabitMem

  ; samples recorded since (re)engage — scan can never reach a previous engage
  kValid init 0
  kValid = (kValid + ksmps > iMemLen ? iMemLen : kValid + ksmps)

  ;--- AUTO-scan brain: HOME (present) <-> OUT (visiting the past)
  ; Pedal kScan sets how OFTEN we depart and how DEEP we may go (manual p.22:
  ; "lower settings introduce the odd departure ... at maximum it will be
  ; auto-scanning at all times"; knob also sets how far back it wanders).
  kOffA  init 0      ; playhead offsets in samples behind the write head
  kOffB  init 0
  kXfTgt init 0      ; which head is active: 0 = A, 1 = B
  kState init 0      ; 0 = HOME, 1 = OUT
  kTimer init 0.5
  kTimer -= ksmps/sr

  if (kTimer <= 0) then
    if (kState == 0 && kScan > 0.02) then
      ; depart: pick a moment up to kScan*memory back (clamped to recorded span)
      kMaxBack = kScan * kValid
      kMaxBack = (kMaxBack < 0.5 * sr ? 0.5 * sr : kMaxBack)
      kOff     random 0.25 * sr, kMaxBack
      kOff     = (kOff > kValid ? kValid : kOff)
      if (kXfTgt == 0) then
        kOffB = kOff
        kXfTgt = 1
      else
        kOffA = kOff
        kXfTgt = 0
      endif
      kState = 1
      ; visit length rides SIZE (manual p.8: scan is rhythmically tied to SIZE)
      kMult  random 1, 4
      kVisit = kSizeSec * kMult
      kTimer = (kVisit < 0.25 ? 0.25 : (kVisit > 8 ? 8 : kVisit))
    else
      ; snap back to the present
      if (kXfTgt == 0) then
        kOffB = 0
        kXfTgt = 1
      else
        kOffA = 0
        kXfTgt = 0
      endif
      kState = 0
      if (kScan > 0.02) then
        ; pause at home shrinks fast as the pedal comes up:
        ; p=1 -> ~0.3 s, p=0.5 -> ~1.2 s, p=0.1 -> capped 30 s
        kWait  random 0.5, 1.5
        kTimer = 0.3 * kWait / (kScan * kScan)
        kTimer = (kTimer > 30 ? 30 : kTimer)
      else
        kTimer = 0.5          ; idle poll while pedal is at heel
      endif
    endif
  endif

  ;--- two memory heads, ~30 ms equal-power crossfade on every jump
  kXf  portk kXfTgt, 0.015
  aXf  interp kXf
  aRNdxA wrap aWNdx - kOffA, 0, iMemLen
  aRNdxB wrap aWNdx - kOffB, 0, iMemLen
  aHeadA table3 aRNdxA, giHabitMem, 0, 0, 1
  aHeadB table3 aRNdxB, giHabitMem, 0, 0, 1
  aMemOut = aHeadA * sqrt(1 - aXf) + aHeadB * sqrt(aXf)

  ;=====================================================================
  ; ECHO stage — ~87 s ring, dual-tap click-free SIZE, tape-aged feedback
  ;=====================================================================
  iEchoLen = ftlen(giHabitEcho)
  aEPhs    phasor sr / iEchoLen
  aENdx    = aEPhs * iEchoLen

  aFbMem init 0                       ; aged feedback from the previous pass
  aSum   = aMemOut + aFbMem * kFb
  tablew aSum, aENdx, giHabitEcho

  ; SIZE changes NEVER glide a read position (that would tape-warp the pitch —
  ; manual p.7: "no pitch warping ... the echo simply gets shorter or longer").
  ; Quantize to a 5 ms grid; on change, hop the INACTIVE tap and crossfade ~50 ms.
  kSizeQ  = round(kSizeSec * 200) / 200
  kTapA   init 0.6                    ; matches gkHabitSize default (~0.6 s)
  kTapB   init 0.6
  kTapTgt init 0
  if (changed(kSizeQ) == 1) then
    if (kTapTgt == 0) then
      kTapB = kSizeQ
      kTapTgt = 1
    else
      kTapA = kSizeQ
      kTapTgt = 0
    endif
  endif
  kTapXf portk kTapTgt, 0.025
  aTapXf interp kTapXf

  ; gate a tap that reaches past what's been recorded since engage
  kEValid init 0
  kEValid = (kEValid + ksmps > iEchoLen ? iEchoLen : kEValid + ksmps)
  kGateA  = (kTapA * sr > kEValid ? 0 : 1)
  kGateB  = (kTapB * sr > kEValid ? 0 : 1)

  aRTapA wrap aENdx - kTapA * sr, 0, iEchoLen
  aRTapB wrap aENdx - kTapB * sr, 0, iEchoLen
  aEchoA table3 aRTapA, giHabitEcho, 0, 0, 1
  aEchoB table3 aRTapB, giHabitEcho, 0, 0, 1
  aEcho  = aEchoA * kGateA * sqrt(1 - aTapXf) + aEchoB * kGateB * sqrt(aTapXf)

  ;--- tape-age IN the loop (the fixed A2 voice, noise-free side): a short
  ; wobbled delay + band-limit + tanh, applied ONLY to the feedback path so
  ; every recirculation warbles and dulls a little more.
  aWow   oscili 2.0, 0.45             ; +/-2.0 ms wow @ 0.45 Hz
  kFlut  randi 0.35, 7                ; +/-0.35 ms flutter (band-limited noise)
  aFlut  interp kFlut
  aWobMs = 5 + aWow + aFlut           ; 2.65..7.35 ms around a 5 ms centre
  aAged  vdelay3 aEcho, aWobMs, 15
  aAged  = butterhp(aAged, 140)
  aAged  = butterlp(aAged, 4500)
  aAged  = tanh(aAged)                ; tape squish; guarantees loop stability
  aFbMem = aAged

  ;--- output: the clean (first-pass) echo; aging accumulates per repeat
  aWet = aEcho
  aWet dcblock2 aWet
  aWet limit aWet, -0.99, 0.99
  gaHabitWetL = aWet
  gaHabitWetR = aWet
endin
```

- [ ] **Step 2.2: Add the include** to `csound/neptrPhase4.csd` after line 44 (`#include "effects/spectraldelay.orc"`):

```csound
#include "effects/habit.orc"
```

- [ ] **Step 2.3: Verify** — `csound --syntax-check-only -o null csound/neptrPhase4.csd`. Expected: clean (instr 64 parses; no "used before defined" on the new globals). If `oscili` complains about a missing table on the installed csound version, change the wow line to use an explicit sine table (add `giHabitSine ftgen 0, 0, 8192, 10, 1` in Task 1 step 1.4 and use `aWow oscili 2.0, 0.45, giHabitSine`).

---

### Task 3: Signal chain (csound/signal_chain.orc)

**Files:**
- Modify: `csound/signal_chain.orc:291` (between the DELAY and REVERB blocks)

- [ ] **Step 3.1: Insert chain block** between `; 20. DELAY (62)` block (ends `aPrevR = gaDelOutR`, line ~291) and `; 21. REVERB (70)`:

```csound
; 20.5 HABIT (64) — echo collector (Chase Bliss Habit port)
gaHabitSendL = aPrevL
gaHabitSendR = aPrevR
$FX_BYPASS(gkHabitBypass'64'gaHabitSendL'gaHabitSendR'gaHabitWetL'gaHabitWetR'habit)
aPrevL ntrpol gaHabitSendL, gaHabitWetL, gkHabitMix
aPrevR ntrpol gaHabitSendR, gaHabitWetR, gkHabitMix
```

- [ ] **Step 3.2: Update the chain-order comment** at the top of signal_chain.orc (the numbered list around line 13): add `20.5 HABIT (64)` after the DELAY entry.

- [ ] **Step 3.3: Verify** — `csound --syntax-check-only -o null csound/neptrPhase4.csd`. Expected: clean.

---

### Task 4: MIDI handler rewiring (csound/midi_handler.orc)

**Files:**
- Modify: `csound/midi_handler.orc` (6 sites + header comments)

- [ ] **Step 4.1: PITCH s0 toggle (CC74, menu 2)** — replace the FORMANT branch at ~line 231–240:

```csound
      elseif (gkCurrentMenu == 2) then
        ; SPECTRAL DELAY (moved from TIME s1; Formant now lives only in FILTERING s3)
        $FX_TOGGLE(gkSpecDelBypass'16)
        if (gkSpecDelBypass < 0.5) then
          printks "SPECTRAL DELAY: ON\n", 0
        else
          printks "SPECTRAL DELAY: OFF\n", 0
        endif
```

(Note: drop the old `gkLastToggle29 = 5` line — SpecDel has no CC29 mapping.)

- [ ] **Step 4.2: PITCH s0 rotation (CC24, menu 2)** — replace at ~line 693–695:

```csound
      elseif (gkCurrentMenu == 2) then
        ; Spectral Delay dry/wet (moved from TIME s1)
        $ENC_ROT(gkSpecDelMix'gkStepSpecMix'0'1)
```

- [ ] **Step 4.3: TIME s1 toggle (CC75, menu 6)** — replace the Spectral Delay branch at ~line 338–345:

```csound
      elseif (gkCurrentMenu == 6) then
        ; HABIT — echo collector (Chase Bliss Habit port)
        $FX_TOGGLE(gkHabitBypass'36)
        if (gkHabitBypass < 0.5) then
          gkLastToggle29 = 36
          printks "HABIT: ON\n", 0
        else
          printks "HABIT: OFF\n", 0
        endif
```

- [ ] **Step 4.4: TIME s1 rotation (CC25, menu 6)** — replace at ~line 740–742:

```csound
      elseif (gkCurrentMenu == 6) then
        ; HABIT dry/wet (amount)
        $ENC_ROT(gkHabitMix'gkStepHabitMix'0'1)
```

- [ ] **Step 4.5: CC20 secondary param** — in the `kCC == 20` dispatch, after the `gkLastToggle == 35` branch (~line 920):

```csound
      elseif (gkLastToggle == 36) then
        ; HABIT size (echo time, exp 50 ms .. 60 s inside the instr)
        $ENC_ROT(gkHabitSize'gkStepHabitSize'0'1)
```

- [ ] **Step 4.6: CC23 tertiary param** — in the `kCC == 23` dispatch, after the `gkLastToggle == 35` branch (~line 969):

```csound
      elseif (gkLastToggle == 36) then
        ; HABIT repeats (feedback; max = stable infinite build-up)
        $ENC_ROT(gkHabitRepeats'gkStepHabitRepeats'0'1)
```

- [ ] **Step 4.7: CC29 expression dispatch** — in the `kCC == 29` chain, after the `gkLastToggle29 == 15` branch (~line 1057):

```csound
      elseif (gkLastToggle29 == 36) then
        ; HABIT scan (auto-scan amount: how often + how deep into the memory)
        gkHabitScan = kNorm
```

- [ ] **Step 4.8: OSC feedback** — in the OSC block (after the SpecDel lines at ~1195–1198):

```csound
  ; Toggle 36: HABIT — echo collector
  $OSC_I(gkHabitBypass'/hab_state'habByp)
  $OSC_F(gkHabitMix'/hab_mix'habMix)
  $OSC_F(gkHabitSize'/hab_size'habSiz)
  $OSC_F(gkHabitRepeats'/hab_repeats'habRep)
  $OSC_F(gkHabitScan'/hab_scan'habScn)
```

- [ ] **Step 4.9: Header comment tables** (top of midi_handler.orc, lines ~32 and ~36) — update:

```
; MENU 2 (PITCH):     s0=SpecDly(74)      s1=OldPS(75)        s2=GPS(76)           s3=PitchFork(71)  s4=H90(72)
; MENU 6 (TIME):      s0=Delay(74)        s1=Habit(75)        s2=Reverb(76)        s3=Granular(71)   s4=MOOD(72)
```

Also update the line-27 note ("Formant is also still reachable via PITCH s0") to say Formant is FILTERING-only now.

- [ ] **Step 4.10: Verify** — `csound --syntax-check-only -o null csound/neptrPhase4.csd`. Expected: clean. Then grep sanity: `grep -c "gkLastToggle == 36\|gkLastToggle29 == 36\|gkHabitBypass'36" csound/midi_handler.orc` → expected `3`; `grep -n "FORMANT" csound/midi_handler.orc` → only FILTERING-menu (menu 3) and OSC/CC-dispatch hits remain (toggle ID 5 dispatch lines are shared and stay).

---

### Task 5: Offline bench (verify the DSP before deploying)

**Files:**
- Create: `scripts/bench/habit_bench.csd` (kept; useful for future tuning)

- [ ] **Step 5.1: Create `scripts/bench/habit_bench.csd`:**

```csound
<CsoundSynthesizer>
<CsOptions>
-o /tmp/habit_bench.wav -W
</CsOptions>
<CsInstruments>
#include "../../csound/globals.orc"
#include "../../csound/effects/habit.orc"

; test driver: 2 s sine-blip phrase into the send buses, scenario via p4
instr 1
  ; p4 scenarios: 1 = plain echo + size sweep, 2 = infinite repeats,
  ;               3 = scanning,               4 = re-engage clears
  iScn = p4

  ; a recognizable phrase: 0.2 s blips at 220/330/440 Hz every 0.5 s for 2 s
  kPhr  = phasor:k(0.5)
  kStep = int(phasor:k(1/6) * 3)            ; 0,1,2 over 6 s
  kFrq  = 220 * (kStep == 0 ? 1 : (kStep == 1 ? 1.5 : 2))
  aOsc  poscil 0.3, kFrq
  kGate = (kPhr < 0.4 && timeinsts() < 2 ? 1 : 0)
  aSig  = aOsc * interp(kGate)
  gaHabitSendL = aSig
  gaHabitSendR = aSig

  if (iScn == 1) then
    gkHabitScan = 0
    gkHabitRepeats = 0.5
    ; size hops 0.3 -> 0.6 norm at t=8 (listen: echoes shorten, NO pitch bend)
    gkHabitSize = (timeinsts() < 8 ? 0.42 : 0.30)
  elseif (iScn == 2) then
    gkHabitScan = 0
    gkHabitSize = 0.30
    gkHabitRepeats = 1.0      ; must settle stable, not run away or die
  elseif (iScn == 3) then
    gkHabitSize = 0.30
    gkHabitRepeats = 0.3
    gkHabitScan = (timeinsts() < 10 ? 0 : 0.7)   ; ghosts after t=10
  endif

  outs gaHabitSendL * 0.5 + gaHabitWetL, gaHabitSendR * 0.5 + gaHabitWetR
  gaHabitSendL = 0
  gaHabitSendR = 0
  gaHabitWetL = 0
  gaHabitWetR = 0
endin
</CsInstruments>
<CsScore>
; scenario chosen by editing this line: instr 64 runs alongside the driver
i 64 0 30
i 1  0 30 1
</CsScore>
</CsoundSynthesizer>
```

(Note: globals.orc contains instr-independent setup only — it is the engine header the real csd uses, so sr/ksmps come along for free. The driver mixes dry+wet manually because the bench has no signal_chain. `outs` writes /tmp — run from the main session, not a sandboxed agent.)

- [ ] **Step 5.2: Scenario 1 — plain echo + clean SIZE change.** Run: `cd scripts/bench && csound habit_bench.csd`. Listen/inspect `/tmp/habit_bench.wav` (`afplay /tmp/habit_bench.wav` or open in an editor): expect the blip phrase echoing at ~0.4 s, decaying; at t=8 the echo spacing shortens with **no pitch slur** (a brief 50 ms crossfade is OK; a tape-style downward warble is a FAIL → check the dual-tap xfade logic).

- [ ] **Step 5.3: Scenario 2 — stable infinite.** Edit score line to `i 1 0 30 2`, re-render. Expect: repeats build slightly, then hold at a stable level for the full file, getting progressively darker/warblier (tape aging). FAIL = runaway clipping (check kFb mapping/tanh placement) or fade to silence (check loop filters).

- [ ] **Step 5.4: Scenario 3 — scan ghosts.** Score `i 1 0 30 3`. Expect: normal echo until t=10, then fragments of the t<2 phrase resurfacing, each entrance/exit click-free (crossfaded). FAIL = clicks on jumps (check kXf portk) or silence (check kValid clamping).

- [ ] **Step 5.5: Scenario 4 — clear on re-engage.** Score: replace the two lines with `i 64 0 12`, `i 64 14 16`, `i 1 0 30 3`. Expect: after the instr restarts at t=14, NO material from before t=14 ever resurfaces (counters reset; scan only reaches post-14 audio). FAIL = old blips return (check `kValid init 0` placement).

---

### Task 6: UI (neptrPhase4_UI.py)

**Files:**
- Modify: `neptrPhase4_UI.py:71` (PITCH row), `:75` (TIME row), `:107` (params table)

- [ ] **Step 6.1: PITCH row** (line 71) — currently 4 entries (Formant was already dropped from display, leaving the s4→s0 ordering off by one). Replace with the explicit 5-slot row, same ordering convention as the TIME row (s4 leftmost → s0 rightmost):

```python
    {"name": "PITCH",   "effects": [("h90", "H90"), ("pf", "P-FORK"), ("gps", "GPS"), ("ps", "OLD PS"), ("sdl", "SPECDLY")]},
```

- [ ] **Step 6.2: TIME row** (line 75) — swap `sdl` for `hab`:

```python
    {"name": "TIME",    "effects": [("mod", "MOOD"),    ("grn", "GRANULAR"),("vrb", "REVERB"),  ("hab", "HABIT"),   ("dly", "DELAY")]},
```

- [ ] **Step 6.3: Params table** (next to the `"sdl"` entry, line 107) — add:

```python
    "hab": [("mix", "Mix", 0, 1), ("size", "Size", 0, 1), ("repeats", "Repeats", 0, 1), ("scan", "Scan", 0, 1)],
```

(The UI auto-maps `/hab_state` and `/hab_<param>` from these tables — no dispatcher code to write.)

- [ ] **Step 6.4: Verify** — `python3 -c "import ast; ast.parse(open('neptrPhase4_UI.py').read())"` from the project root. Expected: no output (parses clean). Full UI behavior is verified on-Pi in Task 8.

---

### Task 7: Docs + memory

**Files:**
- Modify: `docs/neptr_full_reference.md` (§5 PITCH + TIME tables, §6, §7, §8, §11)
- Modify: memory file `neptr-add-effect-checklist.md` (toggle registry note)

- [ ] **Step 7.1: §5 Menu 2 PITCH table** — replace the s0 row: `| s0 | 74 | Spectral Delay (also see TIME-era params) | 16 | CC24: mix |` and delete Formant from the PITCH table + the "(left→right: …)" header line → `(left→right: H90 · PITCH FORK · GPS · OLD PS · SPECTRAL DELAY)`. **§5 Menu 3 FILTERING:** drop the "(also in PITCH s0)" note on Formant.

- [ ] **Step 7.2: §5 Menu 6 TIME table** — replace the s1 row: `| s1 | 75 | HABIT (echo collector) | 36 | CC25: mix |`.

- [ ] **Step 7.3: §6 CC20/CC23 table** — add row: `| 36 Habit | size (50 ms–60 s exp) | repeats (max = infinite) |`.

- [ ] **Step 7.4: §7 CC29 table** — add row: `| 36 | Habit scan (auto-scan amount) |`.

- [ ] **Step 7.5: §8 chain listing** — add `20.5 HABIT 64` between DELAY 62 and REVERB 70.

- [ ] **Step 7.6: §11 summary + registry** — add the effect row: `| Habit (64, habit.orc) | two-stage echo collector: ~175 s always-recording memory ring + auto-scan playhead (exp pedal) → 87 s echo w/ dual-tap clean SIZE + tape-aged feedback (wow/flutter/LP/tanh in loop); manual: docs/reference/Habit_Manual_Chase_Bliss.pdf | gkHabit* |`. Registry line: append `· 36 HABIT`, change **Next free: 37**. Update §16 checklist note "next free toggle ID is 37".

- [ ] **Step 7.7: Memory file** — update `~/.claude/.../memory/neptr-add-effect-checklist.md`: toggle IDs in use now "1-28, 30-36", and note Habit (TIME s1, instr 64, toggle 36, exp=scan) + that SpecDel moved to PITCH s0 and Formant is FILTERING-only.

- [ ] **Step 7.8: Verify** — `grep -n "36" docs/neptr_full_reference.md | grep -i habit` returns the new rows; `grep -n "Next free" docs/neptr_full_reference.md` says 37.

---

### Task 8: Deploy + on-Pi smoke test

**Files:** none (deploy + manual verification)

- [ ] **Step 8.1: Final syntax check** — `csound --syntax-check-only -o null csound/neptrPhase4.csd`. Expected: clean.

- [ ] **Step 8.2: Deploy** — `./scripts/sync_to_pi.sh` then `ssh gorpi7@demiurge.local 'sudo systemctl restart demiurge'` (needs tty/password; passwordless fallback: `ssh gorpi7@demiurge.local 'kill -9 $(systemctl show demiurge -p MainPID --value)'` — Restart=on-failure respawns it).

- [ ] **Step 8.3: Boot log check** — `ssh gorpi7@demiurge.local 'journalctl -u demiurge --no-pager -n 100 | grep -v "MIDI CC"'`. Expected: csound running, no ftgen/parse errors, the two big tables allocate (~100 MB RSS jump is normal).

- [ ] **Step 8.4: Habit smoke test via physical CCs.** `claude_midi.sh` speaks PHYSICAL CCs; per the instr 99 remap: menu-next = physical cc74, slot1 toggle = physical cc73 (→ logical 75), slot1 rotation = physical cc23 (→ logical 25), secondary = physical cc26 (E7 rot → logical 20), tertiary = physical cc24 (E5 rot → logical 23):

```sh
SSH() { ssh gorpi7@demiurge.local "bash ~/NEPTR_phase4/scripts/claude_midi.sh $*"; }
# navigate PRESETS -> TIME = 6 x menu-next (physical cc74)
for i in 1 2 3 4 5 6; do SSH cc 74 127; done
SSH cc 73 127          # TIME s1 toggle -> journal: "HABIT: ON"
SSH cc 29 100          # expression -> scan deep
SSH cc 23 70           # slot1 enc CW -> mix up
SSH cc 26 70           # E7 rot -> logical CC20 -> size up
SSH cc 24 70           # E5 rot -> logical CC23 -> repeats up
SSH cc 73 127          # toggle off -> "HABIT: OFF"
```

Watch `journalctl -f` during this; play guitar through it for the real test: echoes present, scan resurrects playing, size turns don't pitch-warp.

- [ ] **Step 8.5: Spectral Delay regression from PITCH** — menu to PITCH (2 x menu-next from PRESETS: `SSH cc 74 127` twice after a restart, or navigate back), then `SSH cc 70 127` (physical slot0 button → logical CC74) → journal: "SPECTRAL DELAY: ON"; `SSH cc 20 70` (physical E1 rot → logical CC24) → mix changes. Verify FILTERING s3 Formant still toggles (`SSH cc 71 127` on menu 3).

- [ ] **Step 8.6: CPU + UI check** — `ssh gorpi7@demiurge.local 'top -bn1 | head -20'` with Habit engaged: csound CPU increase < ~8% of a core vs bypassed. Touchscreen UI: TIME menu shows HABIT, params move with the encoders, PITCH shows SPECDLY.

---

## Self-review check (done at plan time)

- **Spec coverage:** §4 menu rewiring → Task 4; §5.1–5.4 engine → Task 2; §5.5 globals → Task 1; §5.6 chain → Task 3; §5.7 csd → Task 2.2; §6 UI → Task 6; §7 docs → Task 7; §8 testing → Tasks 5 + 8. OSC naming deviates from spec (`/hab_*` not `/habit_*`) deliberately — UI auto-mapping requires `/{code}_*`.
- **No placeholders:** every code step is complete and paste-able.
- **Name consistency:** `gkHabit{Bypass,Mix,Size,Repeats,Scan}`, `gkStepHabit{Mix,Size,Repeats}`, `ga Habit{Send,Wet}{L,R}`, `giHabitMem/giHabitEcho`, instr 64, toggle 36 — consistent across Tasks 1–8.
