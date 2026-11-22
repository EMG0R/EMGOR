; MOOD Effects Chain (Pitch Shifter + Bitcrush 1 + Granular + Grain Gen + Final Bitcrush)
; Signal chain position: 401-405
; Reads: gaMOOD_SendL/R
; Writes: gaMOOD_FinalL/R
; Uses dedicated giMOOD_BufL/R (separate from standalone granular)

;==============================================================
; MOOD PITCH SHIFTER (H90 algorithm — pvscale sinusoidal model)
; Mono-summed input: granular stage (403/404) restores stereo width.
; Semitone source: giSemi table driven by gkMOOD_Clock (unchanged).
;==============================================================
instr 401
    aIn = (gaMOOD_SendL + gaMOOD_SendR) * 0.5

    klen   = ftlen(giSemi) - 1
    kIndex = round(gkMOOD_Clock * klen)
    ksemi  table kIndex, giSemi

    ; SMOOTH THE SEMITONE (2026-08-12) — h90.orc:79 does exactly this
    ; (`kSemi portk kSemiC, 0.02`) and mood never picked it up.
    ;
    ; giSemi is a 10-entry table of 24, 19, 12, 7, 0, -5, -12, -17, -24, -29 and
    ; kIndex is ROUNDED, so sweeping the expression pedal does not glide the
    ; pitch — it steps it, in jumps of 5 to 7 semitones. Each step is an
    ; instantaneous ratio change handed straight to pvscale, which has to
    ; re-track every partial's phase advance in one hop. That is heard as a hard
    ; crackle on every pedal step, and it is worst exactly when the player is
    ; moving the pedal, which is when the effect is meant to shine.
    ;
    ; 20 ms is short enough that the stepped, quantised character is preserved
    ; — this is not turned into a portamento — but long enough that the phase
    ; vocoder sees a ramp it can follow instead of a discontinuity. It also
    ; keeps the branch gate honest: a glide re-arms kWarmBoth every cycle it
    ; moves, so both branches stay warm across a unity crossing rather than one
    ; being switched to cold.
    kSemiSm portk ksemi, 0.02
    kRatio = pow(2, kSemiSm / 12)

    ; BRANCH GATING (2026-08-12, perf) — ported from h90.orc, which fixed this
    ; same waste on 2026-08-10. The selector below picks exactly ONE branch, but
    ; both were computed unconditionally every cycle: three 1024-point FFT/IFFT
    ; pairs plus an 8-deep butterlp cascade, two thirds of it discarded. This is
    ; the heaviest instrument in the chain and the one that crackles.
    ;
    ; A cold pvsanal needs a full 1024-sample frame (~21 ms) before its output
    ; is valid, so the incoming branch has to be spun up BEFORE the selector
    ; crosses unity. Two mechanisms, both from h90:
    ;   * a +/-1 semitone hysteresis band around unity (0.9439..1.0595) where
    ;     both run — and which the small-interval blend below already pulls
    ;     toward dry, so any residual warm-up is masked;
    ;   * kWarmBoth, which forces both branches to run for a fixed window after
    ;     ANY ratio movement, since a jump can cross the band faster than a cold
    ;     analysis can fill.
    ;
    ; The window is 50 ms here where h90 uses 25: MOOD's up branch is a TWO-stage
    ; cascade, so its second pvsanal cannot begin filling until the first is
    ; already producing, and it needs ~2x21 ms to be valid end to end.
    ;
    ; Cost in practice is near zero. gkMOOD_Clock is a persisted knob position
    ; (state slot 179), not an LFO — kRatio is static while playing, so the
    ; steady state runs one branch and the warm-up window only opens while the
    ; encoder is actually moving.
    aShiftD   init 0
    aShiftU   init 0
    kRatPrev  init 1
    kWarmBoth init 0
    if (abs(kRatio - kRatPrev) > 0.0005) then
      kWarmBoth = 0.050 * kr
    elseif (kWarmBoth > 0) then
      kWarmBoth = kWarmBoth - 1
    endif
    kRatPrev = kRatio

    ; --- DOWN BRANCH (kRatio <= 1.0): peak-locked pvscale ---
    if (kRatio < 1.0595 || kWarmBoth > 0) then
      fAnalD    pvsanal  aIn, 1024, 256, 1024, 1
      fLockedD  pvslock  fAnalD, 1
      fShiftedD pvscale  fLockedD, kRatio, 0, 4
      aShiftD   pvsynth  fShiftedD
    endif

    ; --- UP BRANCH (kRatio > 1.0): cascaded 2-stage pvscale ---
    ;
    ; A single pvscale at kRatio=4 (+24 semi) forces the phase vocoder to track
    ; phases advancing 4x per hop — it can't do that cleanly and produces harshness.
    ; Cascading two stages of sqrt(kRatio) each means each PV stage only does ×2
    ; maximum (one octave), which pvscale handles well at every semitone value.
    ; Anti-alias pre-filter per stage; each stage uses the per-stage ratio so the
    ; cutoff at +24 semi is 11500 Hz per stage instead of 5500 Hz in one shot —
    ; more bandwidth, less pre-filter ringing going into the PV analysis.
    ; Latency doubles to ~32 ms but MOOD's granular stage already adds far more.
    if (kRatio > 0.9439 || kWarmBoth > 0) then
      kRatio1 = sqrt(kRatio)
      kCutA1  = sr * 0.5 - 500
      kCutB1  = sr * 0.5 / kRatio1 - 500
      if (kCutB1 < kCutA1) then
        kCut1 = kCutB1
      else
        kCut1 = kCutA1
      endif

      ; Stage 1
      aP1 butterlp aIn,  kCut1
      aP2 butterlp aP1,  kCut1
      aP3 butterlp aP2,  kCut1
      aP4 butterlp aP3,  kCut1
      fA1  pvsanal  aP4, 1024, 256, 1024, 1
      fS1  pvscale  fA1, kRatio1, 0, 1
      aS1  pvsynth  fS1

      ; Stage 2
      aQ1 butterlp aS1,  kCut1
      aQ2 butterlp aQ1,  kCut1
      aQ3 butterlp aQ2,  kCut1
      aQ4 butterlp aQ3,  kCut1
      fA2     pvsanal  aQ4, 1024, 256, 1024, 1
      fS2     pvscale  fA2, kRatio1, 0, 1
      aShiftU pvsynth  fS2

      ; Transient sidechain — decay 0.89 matched to ~32 ms cascade latency
      ; (0.89^6 ≈ 0.50 at 32 ms at krate=187.5 Hz, hands off cleanly to the PV body)
      ; Stays INSIDE the gate: it only ever feeds aShiftU, so running it while
      ; the down branch is selected would be more discarded work — and its
      ; kAtkEnv decay is re-armed by kDelta on the first cycle after the gate
      ; reopens, well inside the 50 ms warm window.
      aEnv     follow2  aIn, 0.001, 0.015
      kEnv     downsamp aEnv
      kEnvPrev init 0
      kDelta   = kEnv - kEnvPrev
      kEnvPrev = kEnv
      kAtkEnv  init 0
      ; KSMPS-INVARIANT DECAY (2026-08-12). This was a bare `* 0.89` applied
      ; once per k-cycle, which is a per-BLOCK decay pretending to be a per-TIME
      ; one: halve ksmps and the k-rate doubles, so the envelope decays twice as
      ; fast in wall-clock for the same written constant. That silently
      ; re-tunes the effect whenever the block size changes, which is exactly
      ; what a move to ksmps 32 does.
      ;
      ; h90.orc:176 already writes this correctly as exp(-1/(tau*kr)); this is
      ; the same sidechain that never got migrated.
      ;
      ; The exponent PRESERVES TODAY'S SOUND rather than the comment's stated
      ; intent. The header claims 0.89 gives a ~32 ms half-life "at krate=187.5
      ; Hz" — but 187.5 Hz is ksmps 256, and this patch runs ksmps 64 (kr 750),
      ; where 0.89 is really an ~11 ms half-life. The constant has therefore
      ; been ~4x faster than documented for as long as it has shipped, and the
      ; user tuned MOOD by ear against THAT. So the reference point is 0.89 at
      ; kr 750, held constant in wall-clock terms at any ksmps:
      ;
      ;   coeff = 0.89 ^ (750 / kr)
      ;
      ; identical at ksmps 64, and 0.9434 at ksmps 32 — the same decay per
      ; millisecond, so the block size stops being audible. Correcting it to
      ; the documented 32 ms is a separate, deliberate tone change, not this.
      if (kDelta > 0.05) then
        kAtkEnv = 1.0
      else
        kAtkEnv = kAtkEnv * exp(log(0.89) * 750 / kr)
      endif
      aDryAtk butterlp aIn,     4000
      aDryAtk butterlp aDryAtk, 4000
      aShiftU = aShiftU + aDryAtk * kAtkEnv * 0.35
    endif

    ; Branch select
    if (kRatio > 1.0) then
      aShift = aShiftU
    else
      aShift = aShiftD
    endif

    ; Small-interval blend — transparent at |semi| < 1
    ; Smoothed, so the dry blend tracks the pitch actually being produced
    ; rather than the stepped target — otherwise the blend snaps to dry a beat
    ; before the shifter has finished gliding there.
    kAbsSemi = abs(kSemiSm)
    kBlend   init 0
    if (kAbsSemi < 1.0) then
      kBlend = 1.0 - kAbsSemi
    else
      kBlend = 0.0
    endif
    kBlend portk kBlend, 0.02
    aOut = aShift * (1.0 - kBlend) + aIn * kBlend

    gaMOOD_PitchL = aOut
    gaMOOD_PitchR = aOut
endin

;==============================================================
; MOOD BITCRUSHER 1
;==============================================================
instr 402
aInL = gaMOOD_PitchL
aInR = gaMOOD_PitchR
$MOOD_BITCRUSH(aInL'aInR'gaMOOD_Bit1L'gaMOOD_Bit1R)
endin

;==============================================================
; MOOD GRANULAR PROCESSOR
;==============================================================
instr 403
aInL = gaMOOD_Bit1L
aInR = gaMOOD_Bit1R
ilen = ftlen(giMOOD_BufL)
aptr phasor sr / ilen
aptr *= ilen
tablew aInL, aptr, giMOOD_BufL
tablew aInR, aptr, giMOOD_BufR
kptr downsamp aptr
kGSize1 = 0.005
kGSize2 = 0.8
kDens = gkMOOD_Dens
; MINIMUM DELAY 10 ms (2026-08-12) — this, not the pitch shifter, is what made
; MOOD crackle. Same fault as effects/granular.orc; see the long note there.
;
; A grain reads kDly behind the write head, which is safe — but `tablei`
; interpolates toward index+1, and at kDly = 0 that neighbour is one sample
; PAST the write head, holding audio from the previous lap of this 1048576-frame
; ring: 21.8 seconds old. The grain splices current signal against 21.8-second-
; old signal, which is a hard click. kRndDly runs through `expcurve ..., 100`
; below, biasing the draw hard toward zero, so most grains landed exactly there.
;
; MOOD is hit harder than the standalone granular because kDens can be high and
; kGSize2 is 0.8 s, so the clicks arrive densely and under long grains.
kDly1 = 0.01
kDly2 = gkMOOD_Dly
kPanSpread = 1
kAmpSpread = 1
kFiltSpread = 1
kampdecay = 0.5
kreverse = 0.8
kwindow = 1
kDlyDst = 1
klevel = 1
kkr = sr / ksmps
kmaxfreq = kkr
kfreq limit kDens, 0, kmaxfreq
ktrig metro kfreq
if (ktrig == 0) kgoto skip
kdens_per_trig = kDens / kfreq
kint = int(kdens_per_trig)
kfrac = kdens_per_trig - kint
krnd random 0, 1
knum = kint + (krnd < kfrac ? 1 : 0)
kcount = 0
while (kcount < knum) do
kRnd random 0, 1
kGSize = kGSize1 + kRnd * (kGSize2 - kGSize1)
kRndDly random 0, 1
kRndDly expcurve kRndDly, 100
kDly = kDly1 + kRndDly * (kDly2 - kDly1)
schedkwhen 1, 0, 0, 404, kDly + 0.0001, kGSize, kptr, kPanSpread, kreverse, kDly, kDly1 + 1/sr, kDly2 + 0.0001, kampdecay, klevel, kwindow, kAmpSpread, kFiltSpread
kcount += 1
od
skip:
endin

;==============================================================
; MOOD GRANULAR GRAIN GENERATOR
;==============================================================
instr 404
iGStart = p4
ispread = p5
ireverse = (random(0, 1) > p6 ? 1 : -1)
iwindow table p12 - 1, giwindows
idly = p7
iMinDly = p8
iMaxDly = p9
iampdecay = p10
ilevel = p11
iAmpSpr = p13
iFiltSpr = p14
iRtoAmp divz idly - iMinDly, iMaxDly - iMinDly, 0
iamp = (1 - iRtoAmp) ^ 2
iamp ntrpol 1, iamp, iampdecay
iRndAmp random 1 - iAmpSpr, 1
iamp = iamp * iRndAmp
aenv oscili iamp, 1/p3, iwindow
aline line iGStart, p3, iGStart + (p3 * sr * ireverse)
aL tablei aline, giMOOD_BufL, 0, 0, 1
aR tablei aline, giMOOD_BufR, 0, 0, 1
if iFiltSpr > 0 then
iRndCfOct random 14 - (iFiltSpr * 10), 14
iRndCf = cpsoct(iRndCfOct)
aL butlp aL, iRndCf
aR butlp aR, iRndCf
endif
ipan random 0.5 - (ispread * 0.5), 0.5 + (ispread * 0.5)
gaMOOD_GranL += aL * aenv * ipan * ilevel
gaMOOD_GranR += aR * aenv * (1 - ipan) * ilevel
endin

;==============================================================
; MOOD FINAL BITCRUSHER
;==============================================================
instr 405
aInL = gaMOOD_GranL
aInR = gaMOOD_GranR
$MOOD_BITCRUSH(aInL'aInR'gaMOOD_FinalL'gaMOOD_FinalR)
endin
