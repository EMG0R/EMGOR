; TAPE — Generation Loss MKII-style tape machine (replaces DROPOUT)
; Chain position: 12 (INLINE in instr 1 — drive-pedal pattern, no instr number)
; Reads/Writes: none (inline args/returns). Buffers: giTapeBufL/R (globals.orc)
; Controls: gkTapeBypass (toggle 21), gkTapeAmount, gkTapeFail, gkTapeModel,
;           gkTapeExpr (CC29 velocity-spring: pitch throws while moving, springs back to unity at any hold)
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
opcode TapeTransport, aak, aaakkk
  aInL, aInR, aSpd, kCatchW, kPedS, kBase xin
  iN     = ftlen(giTapeBufL)
  ; kBase = rest gap = the effect's ENTIRE added latency. Adaptive: the
  ; caller scales it with AMOUNT (wow/flutter headroom is only needed in
  ; proportion to how much wobble is dialed in). ~32 smp (0.7 ms) clean,
  ; ~152 smp (3.2 ms) at full falling-apart.
  iMinG  = 8
  iMaxG  = iN - 4800            ; 100 ms guard before eating the write head's tail
  iCRate = 1 / (0.18 * sr)      ; catch-up: gap decays with tau ~0.18 s
  kWr   init 0
  kGap  init 80                 ; matches base at the AMOUNT=0.4 toggle-on default
  aOutL init 0
  aOutR init 0
  kS = 0
  while kS < ksmps do
    tabw(aInL[kS], kWr, giTapeBufL)
    tabw(aInR[kS], kWr, giTapeBufR)
    ; bidirectional gap regulation: above base -> catch up (max +0.4, the
    ; "tape catching" spin-up character); below base -> weak spring re-opens
    ; headroom (max -0.012 = ~ -20 cents, below the wow's own motion)
    kSpd = aSpd[kS] + limit:k(kCatchW * (kGap - kBase) * iCRate, -0.5, 0.8)
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
    aOutL[kS] = kL
    aOutR[kS] = kR
    kWr = (kWr + 1 >= iN ? 0 : kWr + 1)
    kS += 1
  od
  xout aOutL, aOutR, kGap
endop

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
  kDpC  trandom kTrig, 6, 20
  kHdC  trandom kTrig, 0.015, 0.055
  kRcC  trandom kTrig, 0.04, 0.14
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
  kDens = (0.1 + 3 * kFail) * iMul
  aImp  dust2 1, kDens
  aCrk  butterbp aImp, 3300, 2600
  aCrk  = aCrk * (0.015 + 0.06 * kFail) * (kFail > 0.01 ? 1 : 0)
  aEnv  follow2 aImp, 0.001, 0.04
  kEnv  downsamp aEnv
  kCrumb = 1 - limit:k(kEnv * (0.5 + 0.3 * kAmt), 0, 0.12)
  xout aCrk, kCrumb
endop

;------------------------------------------------------------------
; TapeMachine: the whole machine, 100% wet. Call site: signal_chain
; instr 1, position 12.  aL, aR TapeMachine aL, aR
; Stage order per Gen Loss manual p.44:
; saturate -> transport (wow/flutter/snags/pedal) -> model -> drops -> noise
;------------------------------------------------------------------
opcode TapeMachine, aa, aa
  aInL, aInR xin

  ; ---- controls (isig inits: no startup transient through the transport) ----
  kAmt  port gkTapeAmount, 0.05, i(gkTapeAmount)
  kFail port gkTapeFail,   0.05, i(gkTapeFail)
  kMdl  port gkTapeModel,  0.08, i(gkTapeModel)
  kPed  port gkTapeExpr,   0.04, i(gkTapeExpr)

  ; ---- generative drift: 3 ultra-slow walks orbiting the set values ----
  kDrA = kAmt * kAmt
  kDr1 jspline 1, 0.005, 0.05
  kDr2 jspline 1, 0.004, 0.04
  kDr3 jspline 1, 0.006, 0.06
  ; halved 2026-07-13 (was 0.014 + 0.050*kAmt) — chiller background wow per emgor
  kWowDepth = (0.007 + 0.025 * kAmt) * (1 + 0.8 * kDr1 * kDrA)
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
  ; makeup — volume law: unity at any AMOUNT (gain-stage fit, drive-pedal
  ; convention; recalibrate via scripts/bench whenever the saturation DSP
  ; or drive curve changes)
#ifdef TAPE_BENCH_CAL
  kMk = 1
#else
  ; gain-stage fit 2026-06-11 @ bench rms 0.06 (Pi unreachable — refit from a
  ; live DRIVELEV value when convenient; saturation comp is level-dependent)
  kMk = ampdb(-1.3419 - 0.4962*kAmt - 59.6659*kAmt*kAmt + 70.0513*kAmt*kAmt*kAmt - 25.6242*kAmt*kAmt*kAmt*kAmt)
#endif
  aSatL = aSatL * kMk
  aSatR = aSatR * kMk

  ; ---- TRANSPORT speed: pedal x wow x flutter x snags ----
  kWow  jspline kWowDepth, 0.75, 3
  kFlD  = 0.002 + 0.008 * kAmt   ; halved 2026-07-13 (was 0.004 + 0.016*kAmt) — chiller

  kFl1  randi kFlD, 6, 2
  kFl2  randh kFlD * 0.5, 18
  ; tape stiction — sparse fast wobble bursts (~every 3-5 s on average)
  ; models scrape-flutter: short bursts of rapid speed irregularity at head contact
  kStictTrg   metro 0.3
  kStictRnd   trandom kStictTrg, 0, 1
  kStictFire  = kStictTrg * (kStictRnd < 0.12 ? 1 : 0)
  kStictEnv   init 0
  kStictDecay init 0.99
  if (kStictFire == 1) then
    kStictEnv   = 0.005 + 0.010 * kAmt
    kStictDecay = exp(-1 / (0.3 * kr))      ; ~300 ms burst duration
  endif
  kStictEnv = kStictEnv * kStictDecay
  kStict    randi kStictEnv, 22             ; 22 Hz fast irregular flutter
  ; generative snags/stretches: Poisson speed DIPS; the transport catch-up
  ; turns every recovery into the pitch-up spike (physically how a stuck
  ; tape behaves), so up-spikes can never eat past the write head.
  kRate = 0.012 + 0.45 * kAmt
  kTrg  metro kRate
  kPr   trandom kTrg, 0, 1
  kDuC  trandom kTrg, 0, 1
  kDpC2 trandom kTrg, 0, 1
  kDirC trandom kTrg, 0, 1           ; direction: >0.5 = pitch down, <0.5 = pitch up
  kSnagEnv  init 0
  kSnagSign init 1                   ; +1 subtracts from speed (dn), -1 adds (up)
  kSnagDec  init 0.99
  kFire = kTrg * (kPr < (0.15 + 0.75 * kAmt) ? 1 : 0) * (kAmt > 0.02 ? 1 : 0)
  if (kFire == 1) then
    kIsStr   = (kDuC > 0.8 ? 1 : 0)
    kSnagT   = (kIsStr == 1 ? 0.04 + 0.08 * kDuC : 0.003 + 0.009 * kDuC)
    kSnagEnv = (kIsStr == 1 ? 0.05 + 0.18 * kDpC2 : 0.25 + 0.6 * kDpC2) * (0.3 + 0.7 * kDrA)
    kSnagSign = (kDirC > 0.5 ? 1 : -1)
    kSnagDec = exp(-1 / (kSnagT * kr))
  endif
  kSnagEnv = kSnagEnv * kSnagDec
  ; pedal: velocity-spring BIDIRECTIONAL — pitch throws in both directions while moving,
  ; inaudible return when still. 20ms snap-back so pitch tracks pedal motion, not position.
  ; Dynamic kCatchW: aggressive during throw (responsive), near-zero at rest (gap drifts
  ; back imperceptibly, no audible pitch glide). kBase=800smp (16ms) gives upward headroom.
  kPedSm     portk kPed, 0.022, i(kPed)
  kPedPrev   init i(kPed)
  kPedRate   = (kPedSm - kPedPrev) * kr
  kPedPrev   = kPedSm
  kPedSpring init 0
  kIsMoving  = (abs(kPedRate) > 0.1 ? 1 : 0)     ; true while pedal is actively swept
  kSpDcy     = (kIsMoving == 1 ? exp(-1/(0.15*kr)) : exp(-1/(0.35*kr)))
  kPedSpring = kPedSpring * kSpDcy - kPedRate * (2.0 + 8.0 * kAmt)
  kPedSpring = limit:k(kPedSpring, -0.75, 0.50)  ; -0.75=~2oct dn, +0.50=~8st up
  kPedSpd    = 1.0 + kPedSpring
  kBaseAmt   portk 800 + 120 * kAmt, 4, 800 + 120 * i(gkTapeAmount)
  kBase      = kBaseAmt
  kCatchW    = 0.1 + 5.9 * limit:k(abs(kPedSpring) / 0.75, 0, 1)
  kSpd = kPedSpd * limit:k(1 + kWow + kFl1 + kFl2 + kStict - kSnagSign * kSnagEnv, 0, 2)
  aSpd interp kSpd
  aTpL, aTpR, kGap TapeTransport aSatL, aSatR, aSpd, kCatchW, kPedSpd, kBase
#ifdef TAPE_BENCH_DBG
  printks "DBG gap=%8.1f base=%6.1f spd=%7.4f ped=%7.4f wow=%8.5f\n", 0.25, kGap, kBase, kSpd, kPedSpd, kWow
#endif
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

  ; ---- FAILURE: drops + crinkles, decorrelated L/R (SPREAD stereo) ----
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
  ; 2026-09-25: hiss + motor hum are OFF by default (giTapeNoise in
  ; globals.orc). The user's call: "no noise to it, otherwise it is chill".
  ; The crackle/dropout dust above is tape CHARACTER tied to FAIL and stays.
  aL = aL + (aHsL * kHsLv + aHum) * giTapeNoise
  aR = aR + (aHsR * kHsLv + aHum) * giTapeNoise

  ; dcblock (IIR), NOT dcblock2: dcblock2 is a linear-phase FIR with a real
  ; 254-sample (5.3 ms) delay — measured 2026-06-11 — which would silently
  ; break this effect's zero-latency promise
  aL dcblock aL
  aR dcblock aR
  aL limit aL, -0.99, 0.99
  aR limit aR, -0.99, 0.99
  xout aL, aR
endop
