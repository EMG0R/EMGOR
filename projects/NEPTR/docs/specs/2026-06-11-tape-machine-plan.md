# TAPE Machine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace DROPOUT with TAPE — an inline Gen Loss MKII-style tape machine (hysteresis saturation, single varispeed transport with expression-pedal tape stop, morphable voicing, failure engine, generative AMOUNT macro) per `docs/specs/2026-06-11-tape-machine-design.md`.

**Architecture:** Inline UDOs (drive-pedal pattern, NOT a bus instrument) called conditionally in instr 1 at DROPOUT's old chain position. One ~5.5 s circular tape buffer; wow/flutter/snags/pedal all modulate a single read head. Build additively first (new files + bench, live system untouched), cut over in one task, then deploy.

**Tech Stack:** Csound 6.18 orchestra code, python3+numpy/scipy bench analyzer, ssh deploy to gorpi7@demiurge.local.

---

## Cross-session coexistence (NAM)

Another session is integrating NAM (new menu + `csound/` folder work). Rules for every task below:

1. **Re-read the target region of any shared file (`globals.orc`, `signal_chain.orc`, `midi_handler.orc`, `neptrPhase4.csd`, both UI .py) immediately before editing it** — its line numbers/content may have shifted since this plan was written.
2. **Targeted `Edit`s only, never whole-file rewrites** of shared files.
3. **Claim nothing new:** TAPE inherits toggle 21, MENU 1 slot 4, CC72. Toggle ID 37+ belongs to NAM/future. Do not touch any line mentioning NAM, RTNeural, or unfamiliar new effects.
4. **Before restarting the Pi service (Task 8), STOP and confirm with the user** — the NAM session may be mid-deploy. Never `pkill` csound on the Pi.
5. This tree is **not a git repo** — there are no commit steps. Task 7 makes a tar backup before the cutover instead.

`csound/globals.csd` is a dead legacy copy (not included anywhere) — ignore it entirely.

---

## File map

| File | Action | Responsibility |
|---|---|---|
| `csound/effects/tape.orc` | Create | All TAPE UDOs: `TapeTransport`, `TapeDropGain`, `TapeCrinkle`, `TapeMachine` |
| `csound/globals.orc` | Edit | Swap dropout globals → `gkTape*`; add `giTapeBufL/R` ftables; remove `gaDrop*` buses + dropout steps |
| `csound/signal_chain.orc` | Edit | Replace DROPOUT bus block with inline conditional call |
| `csound/midi_handler.orc` | Edit | Toggle/encoder/CC20/CC23/CC29/OSC branches for toggle 21 |
| `csound/neptrPhase4.csd` | Edit | Swap include |
| `csound/effects/dropout.orc` | Delete | (at cutover) |
| `neptrPhase4_UI.py`, `neptrPhase4_UI_pi.py` | Edit | Menu label + OSC prefix |
| `scripts/bench/tape_bench.csd`, `scripts/bench/analyze_tape.py` | Create | Offline numeric verification |

Run all bench renders **locally on the Mac** (csound 6.18 confirmed at `csound`). Subagents' sandboxes cannot exec csound or write /tmp — if executing via subagents, the orchestrator runs every bench step itself.

---

### Task 1: Tape buffers + control globals (additive — live system unaffected)

**Files:** Modify: `csound/globals.orc`

- [ ] **Step 1.1:** Re-read the dropout control block (search `; Dropout controls`, was near line 469) and the ftgen section of `globals.orc`. Do NOT remove anything yet (cutover is Task 7). Add immediately AFTER the dropout control block:

```csound
; TAPE machine controls (Gen Loss-style; replaces DROPOUT at cutover, toggle 21)
; AMOUNT = saturation + generative spikes + drift macro. FAIL = drop/crinkle
; density. MODEL = voicing morph reel(0)->cassette(0.5)->broken(1).
; EXPR = CC29 transport: 0 = heel = full speed, 1 = toe = stopped.
gkTapeBypass init 1
gkTapeAmount init 0.4
gkTapeFail   init 0.25
gkTapeModel  init 0.35
gkTapeExpr   init 0
```

- [ ] **Step 1.2:** Next to the other `ftgen` table definitions in `globals.orc`, add:

```csound
; TAPE transport buffers — 2^18 samples ≈ 5.46 s @ 48 k. Deliberately bigger
; than the transport needs (~4 s worst-case stop gap): a future granular
; layer reads this same tape (spec §9).
giTapeBufL ftgen 0, 0, 262144, 2, 0
giTapeBufR ftgen 0, 0, 262144, 2, 0
```

- [ ] **Step 1.3:** Verify globals still parse: `cd "NEPTR phase4" && csound --syntax-check-only -m0 scripts/bench/habit_bench.csd` (it includes globals.orc). Expected: no orchestra errors (device/score complaints are fine).

### Task 2: `tape.orc` — transport UDO

**Files:** Create: `csound/effects/tape.orc`

- [ ] **Step 2.1:** Create the file with header + `TapeTransport`. House style: header comment, while-loop per-sample body like `OS4Tanh` (globals.orc ~line 1134) with `asig[kndx]` indexing.

```csound
; TAPE — Generation Loss MKII-style tape machine (replaces DROPOUT)
; Chain position: 12 (INLINE in instr 1 — drive-pedal pattern, no instr number)
; Reads/Writes: none (inline args/returns). Buffers: giTapeBufL/R (globals.orc)
; Controls: gkTapeBypass (toggle 21), gkTapeAmount, gkTapeFail, gkTapeModel,
;           gkTapeExpr (CC29 transport)
; Spec: docs/specs/2026-06-11-tape-machine-design.md
; NOTE: no portk anywhere near event triggers (Habit lesson); pedal/speed
; smoothing is continuous-control smoothing, which is fine.

;------------------------------------------------------------------
; TapeTransport: write head always advances; read head chases it at
; aSpd. Gap self-heals toward iBase via catch-up term (this recovery
; IS the tape-spin-up / snag-release pitch-up sound). Output scaled
; by min(speed,1): head EMF -> silence at full stop (also kills the
; frozen-sample DC a naive stopped read would hold).
; 4-point Catmull-Rom read interpolation (varispeed exposes linear).
;------------------------------------------------------------------
opcode TapeTransport, aak, aaak
  aInL, aInR, aSpd, kCatchW xin
  iN     = ftlen(giTapeBufL)
  iBase  = 120                  ; rest gap: 2.5 ms @ 48k = entire added latency
  iMinG  = 8
  iMaxG  = iN - 4800            ; 100 ms guard before eating the write head's tail
  iCRate = 1 / (0.35 * sr)      ; catch-up: gap decays with tau ~0.35 s
  kWr   init 0
  kGap  init 120
  aOutL init 0
  aOutR init 0
  kS = 0
  while kS < ksmps do
    tabw(aInL[kS], kWr, giTapeBufL)
    tabw(aInR[kS], kWr, giTapeBufR)
    kSpd = aSpd[kS] + limit:k(kCatchW * (kGap - iBase) * iCRate, 0, 2)
    kSpd = limit:k(kSpd, 0, 4)
    kGap = limit:k(kGap + 1 - kSpd, iMinG, iMaxG)
    kRd  = kWr - kGap
    kRd  = (kRd < 0 ? kRd + iN : kRd)
    kI   = int(kRd)
    kF   = kRd - kI
    kIm  = (kI == 0 ? iN - 1 : kI - 1)
    kI1  = (kI + 1 >= iN ? kI + 1 - iN : kI + 1)
    kI2  = (kI + 2 >= iN ? kI + 2 - iN : kI + 2)
    ; left
    k0 = tab:k(kIm, giTapeBufL)
    k1 = tab:k(kI,  giTapeBufL)
    k2 = tab:k(kI1, giTapeBufL)
    k3 = tab:k(kI2, giTapeBufL)
    kL = k1 + 0.5*kF*(k2 - k0 + kF*(2*k0 - 5*k1 + 4*k2 - k3 + kF*(3*(k1 - k2) + k3 - k0)))
    ; right
    k0 = tab:k(kIm, giTapeBufR)
    k1 = tab:k(kI,  giTapeBufR)
    k2 = tab:k(kI1, giTapeBufR)
    k3 = tab:k(kI2, giTapeBufR)
    kR = k1 + 0.5*kF*(k2 - k0 + kF*(2*k0 - 5*k1 + 4*k2 - k3 + kF*(3*(k1 - k2) + k3 - k0)))
    kGn = limit:k(kSpd, 0, 1)
    aOutL[kS] = kL * kGn
    aOutR[kS] = kR * kGn
    kWr = (kWr + 1 >= iN ? 0 : kWr + 1)
    kS += 1
  od
  xout aOutL, aOutR, kGap
endop
```

- [ ] **Step 2.2:** Syntax-check by compiling a minimal scratch csd that includes globals.orc + tape.orc (or proceed — Task 5's bench compiles it; fastest is `csound --syntax-check-only -m0` on the Task 5 bench file once it exists). If checking now, a throwaway `/tmp/chk.csd` with the two includes and an empty instr is enough.

### Task 3: `tape.orc` — failure + crinkle helper UDOs

**Files:** Modify: `csound/effects/tape.orc` (append)

- [ ] **Step 3.1:** Append `TapeDropGain` — Poisson drop events with fast-fall / hold / lumpy-recovery gain envelope. Instantiated once per channel with different `iMul` to decorrelate L/R (the Gen Loss SPREAD trick).

```csound
;------------------------------------------------------------------
; TapeDropGain: stochastic playhead-contact drops (the old dropout.orc
; done properly). Shaped dip: ~4 ms fall, 15-90 ms hold, 60-250 ms
; lumpy recovery. All randoms drawn unconditionally (trandom needs to
; run every cycle); the state machine just latches them on fire.
;------------------------------------------------------------------
opcode TapeDropGain, k, kki
  kFail, kAmt, iMul xin
  iFall = 0.004
  kPh    init 0               ; 0 idle / 1 fall / 2 hold / 3 recover
  kT     init 0
  kGain  init 1
  kDepth init 20
  kHold  init 0.05
  kRec   init 0.12
  kTrig metro (0.2 + 4.3 * kFail) * iMul
  kP    trandom kTrig, 0, 1
  kDpC  trandom kTrig, 10, 42
  kHdC  trandom kTrig, 0.015, 0.09
  kRcC  trandom kTrig, 0.06, 0.25
  kWob  randi 1, 37, 2
  kFire = kTrig * (kP < (0.15 + 0.75 * kFail) ? 1 : 0) * (kPh == 0 ? 1 : 0) * (kFail > 0.01 ? 1 : 0)
  if (kFire == 1) then
    kDepth = kDpC
    kHold  = kHdC
    kRec   = kRcC
    kPh = 1
    kT  = 0
  endif
  kT = kT + 1/kr
  if (kPh == 1) then
    kGain = ampdb(-kDepth * limit:k(kT / iFall, 0, 1))
    if (kT >= iFall) then
      kPh = 2
      kT  = 0
    endif
  elseif (kPh == 2) then
    kGain = ampdb(-kDepth)
    if (kT >= kHold) then
      kPh = 3
      kT  = 0
    endif
  elseif (kPh == 3) then
    kRecF = limit:k(kT / kRec, 0, 1)
    kGain = ampdb(-kDepth * (1 - kRecF) * (1 + 0.25 * kWob * (1 - kRecF)))
    if (kRecF >= 1) then
      kPh   = 0
      kGain = 1
    endif
  else
    kGain = 1
  endif
  xout kGain
endop

;------------------------------------------------------------------
; TapeCrinkle: dust impulses -> bandpassed audible rustle (plays even
; with no input, like the real pedal) + an AM "crumble" envelope that
; briefly chews the program signal.
;------------------------------------------------------------------
opcode TapeCrinkle, ak, kki
  kFail, kAmt, iMul xin
  kDens = (0.5 + 18 * kFail) * iMul
  aImp  dust2 1, kDens
  aCrk  butterbp aImp, 3300, 2600
  aCrk  = aCrk * (0.15 + 0.5 * kFail) * (kFail > 0.01 ? 1 : 0)
  kEnv  follow2 aImp, 0.001, 0.04
  kCrumb = 1 - limit:k(kEnv * (2.5 + 2 * kAmt), 0, 0.55)
  xout aCrk, kCrumb
endop
```

### Task 4: `tape.orc` — main `TapeMachine` UDO

**Files:** Modify: `csound/effects/tape.orc` (append)

- [ ] **Step 4.1:** Append the main UDO. Stage order per manual p.44: saturate → transport (wow/flutter/snags/pedal) → model → drops → noise.

```csound
;------------------------------------------------------------------
; TapeMachine: the whole machine, 100% wet. Call site: signal_chain
; instr 1, position 12.  aL, aR TapeMachine aL, aR
;------------------------------------------------------------------
opcode TapeMachine, aa, aa
  aInL, aInR xin

  ; ---- controls ----
  kAmt  port gkTapeAmount, 0.05
  kFail port gkTapeFail,   0.05
  kMdl  port gkTapeModel,  0.08
  kPed  port gkTapeExpr,   0.04

  ; ---- generative drift: 3 ultra-slow walks orbiting the set values ----
  kDrA = kAmt * kAmt
  kDr1 jspline 1, 0.005, 0.05
  kDr2 jspline 1, 0.004, 0.04
  kDr3 jspline 1, 0.006, 0.06
  kWowDepth = (0.0010 + 0.0035 * kAmt) * (1 + 0.8 * kDr1 * kDrA)
  kMdlEff   = limit:k(kMdl + 0.25 * kDr2 * kDrA, 0, 1)
  kFailEff  = limit:k(kFail * (1 + 0.6 * kDr3 * kDrA), 0, 1)

  ; ---- SATURATE: pre-emph -> biased OS4Tanh (magnetic memory) -> de-emph ----
  ; bias = sub-audio lowpass of the saturated output, fed back (one-block
  ; feedback is fine: the bias loop lives below 30 Hz). Asymmetry/program-
  ; dependence comes from the bias shifting the tanh operating point.
  kDrv  = 1.2 + 7.8 * kAmt * kAmt
  kBias = 0.25 * kAmt
  aBiasL init 0
  aBiasR init 0
  aPreL pareq aInL, 3000, ampdb(4.5), 0.707, 2
  aPreR pareq aInR, 3000, ampdb(4.5), 0.707, 2
  aSatL OS4Tanh aPreL - kBias * aBiasL, kDrv
  aSatR OS4Tanh aPreR - kBias * aBiasR, kDrv
  aBiasL butterlp aSatL, 28
  aBiasR butterlp aSatR, 28
  aSatL pareq aSatL, 3000, ampdb(-4.5), 0.707, 2
  aSatR pareq aSatR, 3000, ampdb(-4.5), 0.707, 2
  ; makeup — volume law: unity at any AMOUNT. Initial analytic guess;
  ; Task 6 replaces with the bench-fitted quartic (drive-pedal pattern).
#ifdef TAPE_BENCH_CAL
  kMk = 1
#else
  kMk = pow(kDrv, -0.55)
#endif
  aSatL = aSatL * kMk
  aSatR = aSatR * kMk

  ; ---- TRANSPORT speed: pedal x wow x flutter x snags ----
  kWow  jspline kWowDepth, 0.5, 2
  kFlD  = 0.0006 + 0.0024 * kAmt
  kFl1  randi kFlD, 8.1, 2
  kFl2  randh kFlD * 0.5, 23
  ; generative snags/stretches: Poisson speed DIPS; the transport catch-up
  ; turns every recovery into the pitch-up spike (physically how a stuck
  ; tape behaves), so up-spikes can never eat past the write head.
  kRate = 0.05 + 2.2 * kAmt
  kTrg  metro kRate
  kPr   trandom kTrg, 0, 1
  kDuC  trandom kTrg, 0, 1
  kDpC  trandom kTrg, 0, 1
  kFire = kTrg * (kPr < (0.15 + 0.75 * kAmt) ? 1 : 0) * (kAmt > 0.02 ? 1 : 0)
  kSnagEnv init 0
  kSnagDec init 0.99
  if (kFire == 1) then
    kIsStr   = (kDuC > 0.8 ? 1 : 0)
    kSnagT   = (kIsStr == 1 ? 0.4 + 0.8 * kDuC : 0.03 + 0.09 * kDuC)
    kSnagEnv = (kIsStr == 1 ? 0.05 + 0.18 * kDpC : 0.25 + 0.6 * kDpC) * (0.3 + 0.7 * kDrA)
    kSnagDec = exp(-1 / (kSnagT * kr))
  endif
  kSnagEnv = kSnagEnv * kSnagDec
  ; pedal: heel(0)=play, toe(1)=stop; rubbery curve + slew
  kPedSpd = pow(limit:k(1 - kPed, 0, 1), 1.6)
  kPedSpd portk kPedSpd, 0.09
  kCatchW = pow(kPedSpd, 4)          ; catch-up only when pedal ~released
  kSpd = kPedSpd * limit:k(1 + kWow + kFl1 + kFl2 - kSnagEnv, 0, 2)
  aSpd interp kSpd
  aTpL, aTpR, kGap TapeTransport aSatL, aSatR, aSpd, kCatchW
  ; flutter AM twitch (what makes flutter read as tape, not vibrato)
  kAMn randi 1, 11.3, 2
  kAMg = 1 - (0.01 + 0.05 * kAmt) * (0.5 + 0.5 * kAMn)
  aTpL = aTpL * kAMg
  aTpR = aTpR * kAMg

  ; ---- MODEL: piecewise anchor morph reel -> cassette -> broken ----
  k0 = limit:k(kMdlEff * 2, 0, 1)
  k1 = limit:k(kMdlEff * 2 - 1, 0, 1)
  kBumpF = 50 + 35 * k0 + 135 * k1          ; head-bump center
  kBumpG = 0.5 + 2.5 * k0 + 2.0 * k1        ; dB
  kLPF   = 18000 - 10000 * k0 - 4600 * k1   ; bandwidth collapse
  kResG  = 0 + 1.5 * k0 + 3.5 * k1          ; resonant peak near cutoff, dB
  kHPF   = 22 + 33 * k0 + 105 * k1
  aL pareq aTpL, kBumpF, ampdb(kBumpG), 1.0, 0
  aR pareq aTpR, kBumpF, ampdb(kBumpG), 1.0, 0
  aL butterhp aL, kHPF
  aR butterhp aR, kHPF
  aL pareq aL, kLPF * 0.85, ampdb(kResG), 2.0, 0
  aR pareq aR, kLPF * 0.85, ampdb(kResG), 2.0, 0
  aL butterlp aL, kLPF
  aR butterlp aR, kLPF

  ; ---- FAILURE: drops + crinkles, decorrelated L/R ----
  kDropL TapeDropGain kFailEff, kAmt, 1.0
  kDropR TapeDropGain kFailEff, kAmt, 1.13
  aCrkL, kCrmL TapeCrinkle kFailEff, kAmt, 1.0
  aCrkR, kCrmR TapeCrinkle kFailEff, kAmt, 0.91
  aL = aL * kDropL * kCrmL + aCrkL
  aR = aR * kDropR * kCrmR + aCrkR

  ; ---- NOISE: hiss follows MODEL (darker+louder toward broken), scaled
  ; by FAIL so it can vanish; faint motor only near the broken end ----
  aNzL noise 1, 0
  aNzR noise 1, 0
  kHsCf = 8000 - 4800 * kMdlEff
  kHsLv = ampdb(-57 + 9 * kMdlEff) * (0.2 + 0.8 * kFailEff)
  aHsL butterlp aNzL, kHsCf
  aHsR butterlp aNzR, kHsCf
  kMot = limit:k((kMdlEff - 0.7) / 0.3, 0, 1)
  aHum poscil ampdb(-52) * kMot, 55.7
  aL = aL + aHsL * kHsLv + aHum
  aR = aR + aHsR * kHsLv + aHum

  aL dcblock2 aL
  aR dcblock2 aR
  aL limit aL, -0.99, 0.99
  aR limit aR, -0.99, 0.99
  xout aL, aR
endop
```

- [ ] **Step 4.2:** Note the taste constants marked by ranges (snag depths, hiss level, crinkle level) are real initial values, tuned later in Task 9's listening pass — do not "improve" them while implementing.

### Task 5: Bench harness + numeric scenarios

**Files:** Create: `scripts/bench/tape_bench.csd`, `scripts/bench/analyze_tape.py`

- [ ] **Step 5.1:** Write `scripts/bench/tape_bench.csd`. Scenario selected by orchestra macro `SCN` (no score editing, unlike habit_bench). The driver calls `TapeMachine` directly (inline pattern — no buses, so the habit "don't zero the buses" gotcha doesn't apply, but the driver still must not portk anything feeding triggers).

```csound
<CsoundSynthesizer>
<CsOptions>
-o /tmp/tape_bench.wav -W -m0
</CsOptions>
<CsInstruments>
#include "../../csound/globals.orc"
#include "../../csound/effects/tape.orc"

; Scenarios (set with --omacro:SCN=N):
;  1 volume law: 8 s multitone, AMOUNT steps 0/.25/.5/.75/1 every 1.5 s
;  2 latency: single impulse at t=1, everything stochastic off
;  3 transport: 440 sine; pedal dive 2..3 s, hold to 5 s, release; 10 s
;  4 generative stats: 440 sine, 30 s, AMOUNT per SCN4AMT macro (0 or 1)
instr 1
  iScn = $SCN
  gkTapeBypass = 0
  ; bench input level: match live DRIVELEV journal value (memory: bench
  ; levels differ; saturation comp is level-dependent). 0.06 rms default —
  ; UPDATE from `journalctl -u demiurge | grep DRIVELEV | tail -1` in Task 6.
  iAmp = 0.06 * 1.414

  if (iScn == 1) then
    kT = int(timeinsts() / 1.5)
    gkTapeAmount = limit:k(kT * 0.25, 0, 1)
    gkTapeFail = 0
    gkTapeModel = 0
    gkTapeExpr = 0
    a1 poscil iAmp*0.6, 196
    a2 poscil iAmp*0.5, 587
    a3 poscil iAmp*0.25, 2330
    aSig = a1 + a2 + a3
  elseif (iScn == 2) then
    gkTapeAmount = 0
    gkTapeFail = 0
    gkTapeModel = 0
    gkTapeExpr = 0
    aSig mpulse 0.7, 999, 1
  elseif (iScn == 3) then
    gkTapeAmount = 0
    gkTapeFail = 0
    gkTapeModel = 0
    kPd = limit:k((timeinsts() - 2), 0, 1)          ; dive 2..3 s
    kPd = (timeinsts() > 5 ? 0 : kPd)               ; release at 5 s
    gkTapeExpr = kPd
    aSig poscil iAmp, 440
  else
    gkTapeAmount = $SCN4AMT
    gkTapeFail = 0
    gkTapeModel = 0
    gkTapeExpr = 0
    aSig poscil iAmp, 440
  endif

  aL, aR TapeMachine aSig, aSig
  outs aL, aR
endin
</CsInstruments>
<CsScore>
i 1 0 30
</CsScore>
</CsoundSynthesizer>
```

- [ ] **Step 5.2:** Write `scripts/bench/analyze_tape.py` (pattern: `analyze_habit.py`). Renders are driven by the shell, analyzer takes scenario + wav path. Checks:

```python
#!/usr/bin/env python3
"""Numeric checks for tape_bench renders. Usage: analyze_tape.py <scn> <wav> [<wav_amt0>]
scn 1: per-1.5s-segment out/in RMS in dB -> PASS if every AMOUNT step within +-0.5 dB of segment 1 (AMOUNT=0 reference; absolute unity enforced after Task 6 calibration)
scn 2: impulse group delay: argmax(|x|) - 48000 samples -> PASS if <= 144 (3 ms)
scn 3: (a) STFT f0 track of 440 Hz: monotonic fall (within tracker noise) during 2..3 s;
       (b) f0 back to 440 +-2 Hz and level steady by 6.5 s (<= 1.5 s wind-up);
       (c) click check: max |diff(x)| in 4.5..7 s window < 8x the max |diff| of the steady 1..2 s window
scn 4: f0 deviation events (>30 cents from median, debounced 50 ms): AMOUNT=1 wav must
       have >= 5 events in 30 s; AMOUNT=0 wav must have 0 events and f0 std < 6 cents
"""
import sys, numpy as np
from scipy.io import wavfile
from scipy.signal import stft

def f0_track(x, sr):
    f, t, Z = stft(x, sr, nperseg=8192, noverlap=6144)
    mag = np.abs(Z)
    band = (f > 100) & (f < 1500)
    fb = f[band]
    pk = fb[np.argmax(mag[band, :], axis=0)]
    return t, pk

def main():
    scn = int(sys.argv[1]); sr, x = wavfile.read(sys.argv[2])
    x = x[:, 0].astype(float)
    if x.dtype != float: x /= 32768.0
    if scn == 1:
        segs = [x[int((1.5*i+0.4)*sr):int((1.5*i+1.4)*sr)] for i in range(5)]
        rms = [20*np.log10(np.sqrt(np.mean(s**2))) for s in segs]
        rel = [r - rms[0] for r in rms]
        ok = all(abs(r) <= 0.5 for r in rel)
        print(f"scn1 rel dB per AMOUNT step: {[f'{r:+.2f}' for r in rel]} -> {'PASS' if ok else 'FAIL'}")
    elif scn == 2:
        d = np.argmax(np.abs(x)) - sr  # impulse fired at t=1
        print(f"scn2 group delay {d} samples ({1000*d/sr:.2f} ms) -> {'PASS' if d <= 144 else 'FAIL'}")
    elif scn == 3:
        t, f0 = f0_track(x, sr)
        dive = f0[(t > 2.1) & (t < 2.9)]
        mono = np.all(np.diff(dive) < 5)
        back = f0[(t > 6.5) & (t < 8)]
        ret = np.all(np.abs(back - 440) < 2)
        w = lambda a, b: np.max(np.abs(np.diff(x[int(a*sr):int(b*sr)])))
        click_ok = w(4.5, 7) < 8 * w(1, 2)
        print(f"scn3 dive-monotonic {mono}, return-by-6.5s {ret}, no-click {click_ok} -> "
              f"{'PASS' if (mono and ret and click_ok) else 'FAIL'}")
    elif scn == 4:
        def events(path):
            sr2, y = wavfile.read(path); y = y[:, 0].astype(float)
            t, f0 = f0_track(y, sr2)
            cents = 1200*np.log2(np.maximum(f0, 1)/np.median(f0))
            dev = np.abs(cents) > 30
            return int(np.sum(np.diff(dev.astype(int)) == 1)), float(np.std(cents))
        n1, _ = events(sys.argv[2]); n0, sd0 = events(sys.argv[3])
        ok = n1 >= 5 and n0 == 0 and sd0 < 6
        print(f"scn4 events@AMT1={n1} events@AMT0={n0} std@AMT0={sd0:.1f}c -> {'PASS' if ok else 'FAIL'}")

main()
```

- [ ] **Step 5.3:** Render + analyze each scenario (from `scripts/bench/`):

```bash
csound --omacro:SCN=1 --omacro:SCN4AMT=0 -o /tmp/tape_s1.wav tape_bench.csd && python3 analyze_tape.py 1 /tmp/tape_s1.wav
csound --omacro:SCN=2 --omacro:SCN4AMT=0 -o /tmp/tape_s2.wav tape_bench.csd && python3 analyze_tape.py 2 /tmp/tape_s2.wav
csound --omacro:SCN=3 --omacro:SCN4AMT=0 -o /tmp/tape_s3.wav tape_bench.csd && python3 analyze_tape.py 3 /tmp/tape_s3.wav
csound --omacro:SCN=4 --omacro:SCN4AMT=1 -o /tmp/tape_s4a.wav tape_bench.csd
csound --omacro:SCN=4 --omacro:SCN4AMT=0 -o /tmp/tape_s4b.wav tape_bench.csd && python3 analyze_tape.py 4 /tmp/tape_s4a.wav /tmp/tape_s4b.wav
```

Expected first run: scn2/scn3/scn4 PASS; scn1 likely FAIL (analytic makeup guess) — that is what Task 6 fixes. Debug any csound compile error or non-scn1 FAIL before proceeding (fix tape.orc, re-render).

- [ ] **Step 5.4:** Note render wall-time per 30 s scenario as a rough CPU sanity check (should be a small fraction of realtime on the Mac; the Pi check happens in Task 8).

### Task 6: Volume-law calibration (drive-pedal pattern)

**Files:** Modify: `csound/effects/tape.orc` (one line), `scripts/bench/tape_bench.csd` (input level)

- [ ] **Step 6.1:** Get the player's real level: `ssh gorpi7@demiurge.local 'journalctl -u demiurge --no-pager | grep DRIVELEV | tail -3'`. Update `iAmp` in tape_bench.csd to `<rms> * 1.414`. (If the Pi is unreachable, keep 0.06 and flag for re-cal.)
- [ ] **Step 6.2:** Render scn1 with makeup disabled: `csound --omacro:SCN=1 --omacro:SCN4AMT=0 --omacro:TAPE_BENCH_CAL=1 -o /tmp/tape_cal.wav tape_bench.csd`, then read the 5 per-segment out/in dB numbers via `analyze_tape.py 1 /tmp/tape_cal.wav`.
- [ ] **Step 6.3:** Fit: with raw gains `g_i` (dB) at AMOUNT = 0, .25, .5, .75, 1, the makeup must be `-g_i` dB. Fit a quartic `c0..c4` through those 5 points (numpy `polyfit`, degree 4, exact through 5 points) and replace the `kMk = pow(kDrv, -0.55)` line:

```csound
  kMk = ampdb($C0 + $C1*kAmt + $C2*kAmt*kAmt + $C3*kAmt*kAmt*kAmt + $C4*kAmt*kAmt*kAmt*kAmt)
```

(with the fitted numeric literals — same gain-stage-fit convention as the drive pedals; recalibrate whenever saturation DSP or the drive curve changes.)
- [ ] **Step 6.4:** Re-render scn1 normally; `analyze_tape.py 1` must PASS within ±0.5 dB. Also re-run scn2 (makeup must not break latency) — PASS.

### Task 7: Cutover — wire TAPE in, remove DROPOUT

**Files:** Modify: `csound/signal_chain.orc`, `csound/midi_handler.orc`, `csound/globals.orc`, `csound/neptrPhase4.csd`. Delete: `csound/effects/dropout.orc`.

- [ ] **Step 7.0:** Backup first (no git): `tar czf "../neptr_pre_tape_$(date +%H%M).tar.gz" csound/ neptrPhase4_UI.py neptrPhase4_UI_pi.py` from the project root. **Re-read every region below before editing — the NAM session may have shifted lines.**
- [ ] **Step 7.1:** `signal_chain.orc` (was ~line 207): replace the DROPOUT block:

```csound
; 12. TAPE (inline) — Gen Loss-style tape machine, 100% wet when active
if (gkTapeBypass < 0.5) then
  aPrevL, aPrevR TapeMachine aPrevL, aPrevR
endif
```

Also update the chain-order comment at the top of the file (was line 29: `;   14. DROPOUT (500)` → `;   14. TAPE (inline)`).
- [ ] **Step 7.2:** `midi_handler.orc` toggle branch (was ~line 616, `gkCurrentMenu == 1` under the slot-4 toggle CC):

```csound
      elseif (gkCurrentMenu == 1) then
        ; TAPE machine (Gen Loss-style; replaced DROPOUT)
        $FX_TOGGLE(gkTapeBypass'21)
        if (gkTapeBypass < 0.5) then
          ; toggle-on defaults: "nice and wistful" + claim the exp pedal
          gkTapeAmount = 0.4
          gkTapeFail   = 0.25
          gkTapeModel  = 0.35
          gkLastToggle29 = 21
          printks "TAPE: ON\n", 0
        else
          printks "TAPE: OFF\n", 0
        endif
```

- [ ] **Step 7.3:** `midi_handler.orc` slot rotation (kCC == 22 branch, `gkCurrentMenu == 1`, was ~line 845): `; TAPE amount macro (saturation + spikes + drift)` / `$ENC_ROT(gkTapeAmount'0.02'0'1)`.
- [ ] **Step 7.4:** `midi_handler.orc` CC20 branch (`gkLastToggle == 21`, was ~line 919): `; TAPE fail (drop/crinkle density)` / `$ENC_ROT(gkTapeFail'0.02'0'1)`.
- [ ] **Step 7.5:** `midi_handler.orc` CC23 branch (`gkLastToggle == 21`, was ~line 980): `; TAPE model morph (reel->cassette->broken)` / `$ENC_ROT(gkTapeModel'0.02'0'1)`.
- [ ] **Step 7.6:** `midi_handler.orc` CC29 dispatch (inside `kCC == 29` chain, was ~line 1030): add

```csound
      elseif (gkLastToggle29 == 21) then
        ; TAPE transport: heel = play, toe = stop (spin-up on release)
        gkTapeExpr = kNorm
```

- [ ] **Step 7.7:** `midi_handler.orc` OSC block (was ~line 1265): replace the four `/drp_*` lines:

```csound
  ; Toggle 21: TAPE
  $OSC_I(gkTapeBypass'/tape_state'tapeByp)
  $OSC_F(gkTapeAmount'/tape_amt'tapeAmt)
  $OSC_F(gkTapeFail'/tape_fail'tapeFail)
  $OSC_F(gkTapeModel'/tape_model'tapeMdl)
```

Also update the menu map comment (was line 32): `s4=Dropout(72)` → `s4=Tape(72)`.
- [ ] **Step 7.8:** `globals.orc`: delete the old dropout control block (`gkDropoutBypass/Amount/Depth/Smoothing`), the `gaDrop*` 4-line bus block, and the `gkStepDropoutSmoothing`/`gkStepDropoutDepth` lines. (The gkTape block from Task 1 stays.)
- [ ] **Step 7.9:** `neptrPhase4.csd` line ~40: `#include "effects/dropout.orc"` → `#include "effects/tape.orc"`.
- [ ] **Step 7.10:** Delete `csound/effects/dropout.orc`.
- [ ] **Step 7.11:** Grep for stragglers: `grep -rn "gkDropout\|gaDrop\|dropout" csound/ --include="*.orc" --include="*.csd"` → only the dead `globals.csd` and the unrelated comment in `pitchfork.orc:80` may remain.
- [ ] **Step 7.12:** Re-run the full Task 5.3 bench suite (it includes globals.orc, catching any globals breakage) — all PASS.

### Task 8: UI labels + deploy + smoke test

**Files:** Modify: `neptrPhase4_UI.py:70`, `neptrPhase4_UI_pi.py:70`

- [ ] **Step 8.1:** In BOTH UI files, first grep how the short code in the effects tuple is consumed (it builds the OSC subscription, e.g. `/{code}_state`). Then change `("drp", "DROPOUT")` → `("tape", "TAPE")` so the addresses match midi_handler's `/tape_*`. If the UI also shows the 3 param values, point them at `/tape_amt`, `/tape_fail`, `/tape_model` with labels AMT/FAIL/MODEL.
- [ ] **Step 8.2:** **PAUSE — confirm with the user that the NAM session is not mid-deploy before touching the Pi.**
- [ ] **Step 8.3:** Deploy: `./scripts/sync_to_pi.sh` then `ssh gorpi7@demiurge.local 'sudo systemctl restart demiurge'`. Check compile: `ssh gorpi7@demiurge.local 'journalctl -u demiurge -n 80 --no-pager'` — no orchestra errors, DRIVELEV reporter ticking. Rollback if broken: restore from the Task 7.0 tar, resync, restart.
- [ ] **Step 8.4:** Remote MIDI smoke test (PHYSICAL CCs via `scripts/claude_midi.sh`, menu-next = cc74, slot toggles start cc70): navigate to MENU 1, toggle slot 4 on → journal shows `TAPE: ON`; sweep the slot encoder and CC20/CC23 → values move in journal/OSC; sweep CC29 0→127→0 → journal stays clean, no xruns in `journalctl`.
- [ ] **Step 8.5:** CPU check while engaged: `ssh gorpi7@demiurge.local 'top -bn2 | grep csound'` — headroom comparable to pre-TAPE (no xrun spam in journal).

### Task 9: Listening pass + memory update

- [ ] **Step 9.1:** User listening checklist (on hardware): rest-state feel (no audible latency), pedal dive rubberiness + spin-up character, AMOUNT sweep (clean glue → gnarl, spikes appearing, drift breathing), FAIL texture (drops + crinkle rustle, stereo movement), MODEL morph (reel → cassette → broken), hiss taste. Tune the flagged taste constants in tape.orc on request; re-run bench scn1+scn2 after ANY saturation/drive-curve change (volume law).
- [ ] **Step 9.2:** Update memory `neptr-add-effect-checklist.md`: toggle 21 = TAPE (Gen Loss-style, inline, spec + plan paths, exp = transport, CC20 = fail, CC23 = model, slot enc = amount); DROPOUT removed. Do not touch the NAM memory entries.

---

## Self-review notes (done at write time)

- **Spec coverage:** §3 controls → Task 7.2-7.7; §4 path → Tasks 2-4; §5 transport/latency → Task 2 + scn2/scn3; §6 macro/drift/volume-law → Task 4 + Task 6 + scn4; §7 wiring → Task 7; §8 testing → Tasks 5/6/8/9; §9 future granular → buffer sized in Task 1. Hidden-option-style extras from the manual (mechanical noise mix, crinkle level knobs) are deliberately fixed taste constants — scope per spec.
- **Names:** `gkTapeBypass/Amount/Fail/Model/Expr`, `giTapeBufL/R`, `TapeMachine/TapeTransport/TapeDropGain/TapeCrinkle`, `/tape_state|amt|fail|model` — consistent across all tasks.
- **Known risks for the implementer:** `tabw`/`tab:k` and `a-var[index]` idioms must match the codebase's csound 6.18 (OS4Tanh already uses `ain[kndx]` — mirror it). `pareq` gain args are amplitude (hence `ampdb(...)`). If `--omacro` doesn't reach `$SCN` in your csound build, fall back to the habit_bench score-editing pattern.
