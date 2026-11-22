; TAPESTOP — buffer-brake gesture (Chroma Console "Interrupt" / vinyl brake)
; ---------------------------------------------------------------------------
; Signal chain position : 12.5 (INLINE, after TAPE 12, before RING MOD 13 —
;                         post-drives / pre-spectral; drive-pedal pattern
;                         like tape.orc: zero bus latency, no instr number)
; Reads/Writes          : none (inline args/returns). Buffers: giTSTP_BufL/R
;                         (self-declared below, may move to globals.orc)
; Controls              : gkTSTP_bypass (toggle 59; 0 = brake ENGAGED),
;                         gkTSTP_mix, gkTSTP_brake, gkTSTP_curve, gkTSTP_mode,
;                         gkTSTP_expr + gkTSTP_exprOn (CC29 manual scrub),
;                         gkTSTP_run (chain-call gate, cleared by the opcode)
; Proposed IDs          : toggle 59, instr 182 RESERVED but UNUSED (inline
;                         effect — no instr; keep 182 off-limits anyway so a
;                         later refactor to bus form doesn't collide).
;                         NOTE: toggles 1-56 verified taken, 57/58 may be
;                         claimed by parallel builders — integration resolves
;                         collisions.
;
; INTEGRATION NOTES (integration agent owns all shared files)
;   * neptrPhase4.csd: #include "effects/tapestop.orc" (any order; fully
;     self-declared).
;   * globals.orc: the gi/gk declarations below may MOVE verbatim to
;     globals.orc (then delete them here). Safe where they are.
;   * signal_chain.orc @ stage 12.5 (immediately after the TAPE inline block):
;       ; 12.5 TAPESTOP (inline) — buffer brake, zero latency, transparent idle
;       if (gkTSTP_run > 0.5) then
;         aPrevL, aPrevR TapeStop aPrevL, aPrevR
;       endif
;     (gkTSTP_run stays 1 through the release/re-accel tail and is cleared by
;     the opcode itself once it has crossfaded back to bit-exact dry, so the
;     guard never chops the spin-up. When run=0 the effect costs nothing and
;     the chain is bit-identical to today.)
;   * midi_handler.orc: toggle 59 slot (menu/slot assignment = integration
;     agent's choice; suggest THEATER menu if one exists, else a free slot):
;       $FX_TOGGLE(gkTSTP_bypass'59)
;       if (gkTSTP_bypass < 0.5) then
;         gkTSTP_run = 1
;         gkLastToggle29 = 59   ; sticky pedal claim — see CC29 CONFLICT below
;       endif
;     Toggle-on defaults (set alongside, house style): mix 1.0, brake 0.35,
;     curve 0.5, mode unchanged (mode is a persistent perform choice).
;   * Encoder map (lastToggle == 59 cases):
;       P1 / CC24-home  -> $ENC_ROT(gkTSTP_mix'0.02'0'1)        ; dry/wet
;       CC20 (P2)       -> $ENC_ROT(gkTSTP_brake'0.02'0'1)      ; brake time
;                          (0..1 -> 50 ms..4 s exponential, mapped in-opcode)
;       CC23 (P3)       -> $ENC_ROT(gkTSTP_curve'0.02'0'1)      ; lin<->exp
;       P4 (first fill) -> stepped mode cycle, NAM-variant convention:
;                          if (kVal > 64) then
;                            gkTSTP_mode = (gkTSTP_mode >= 2 ? 0 : gkTSTP_mode + 1)
;                          elseif (kVal < 64) then
;                            gkTSTP_mode = (gkTSTP_mode <= 0 ? 2 : gkTSTP_mode - 1)
;                          endif
;                          ; 0=BRAKE 1=HALF LATCH 2=HARD CUT return
;   * CC29 expression (manual scrub, CONFLICT — integration arbitrates):
;     TAPESTOP claims the sticky gkLastToggle29 = 59 on toggle-ON, but ONWARD
;     (toggle 45) already claims CC29 the same way; whichever toggled last
;     wins under the existing sticky-claim rule — integration agent decides
;     whether that standing rule suffices or TAPESTOP should yield.
;     CC29 case (gkLastToggle29 == 59):
;       gkTSTP_expr   = kVal / 127        ; 0 = stopped .. 1 = 1x speed
;       gkTSTP_exprOn = 1                 ; first pedal move takes over rate
;     Clear gkTSTP_exprOn = 0 in the toggle-59 ON branch (fresh gesture).
;   * OSC feedback (state block, house macros):
;       $OSC_I(gkTSTP_bypass'/tstp_state'tstpByp)
;       $OSC_F(gkTSTP_mix'/tstp_mix'tstpMix)
;       $OSC_F(gkTSTP_brake'/tstp_brake'tstpBrk)
;       $OSC_F(gkTSTP_curve'/tstp_curve'tstpCrv)
;       $OSC_F(gkTSTP_mode'/tstp_mode'tstpMod)
;   * neptrPhase4_UI.py: MENUS entry ("tstp", "TAPESTOP") in the chosen slot;
;       EFFECT_PARAMS["tstp"] = [("mix", "Dry/Wet", 0, 1),
;                                ("brake", "Brake", 0, 1),
;                                ("curve", "Curve", 0, 1),
;                                ("mode", "Mode", 0, 2)]
;
; WHAT IT DOES
;   Circular buffer (~5.4 s) with a write head at 1x and a variable-rate read
;   head. Toggle ON = engage brake: read rate decelerates 1.0 -> floor along
;   a curved ramp (BRAKE = 50 ms..4 s, CURVE = linear..exponential), pitch
;   diving with it; output level follows the rate near the bottom (head-EMF
;   fade to silence, like tape.orc). MODE 1 parks the rate at 0.5 = latched
;   octave-down lo-fi playback (splice-guarded against buffer wrap). Toggle
;   OFF: MODE 0/1 re-accelerate (~0.35 s spin-up) then crossfade back to
;   bit-exact dry; MODE 2 hard-cuts to dry through an 8 ms fade. CC29 pedal
;   (when claimed) scrubs the rate by hand: heel 0 = stopped, toe 1 = 1x.
;   Latency: ZERO when idle (direct passthrough), zero added when engaged
;   (the read head only ever lags into just-written material).
;   CPU: one tabw + 4-point Catmull read per channel per sample while
;   running; nothing at all when gkTSTP_run == 0.
; ---------------------------------------------------------------------------

; ---- self-declared globals (may move to globals.orc verbatim) ----
giTSTP_BufL ftgen 0, 0, 262144, -2, 0   ; ~5.46 s @ 48k
giTSTP_BufR ftgen 0, 0, 262144, -2, 0

gkTSTP_bypass init 1     ; 0 = brake engaged (FX_TOGGLE convention)
gkTSTP_run    init 0     ; chain-call gate; set by toggle-ON, cleared here
gkTSTP_mix    init 1     ; P1  dry/wet (1 = full effect)
gkTSTP_brake  init 0.35  ; CC20 0..1 -> 50 ms..4 s exp
gkTSTP_curve  init 0.5   ; CC23 0 = linear .. 1 = exponential
gkTSTP_mode   init 0     ; P4  0 brake / 1 half-latch / 2 hard-cut return
gkTSTP_expr   init 1     ; CC29 scrub target rate (0 stopped .. 1 unity)
gkTSTP_exprOn init 0     ; 1 once the pedal has moved during this gesture

;------------------------------------------------------------------
; TapeStop: the whole effect. Call site: signal_chain instr 1,
; stage 12.5, guarded by gkTSTP_run.   aL, aR TapeStop aL, aR
;
; State machine (k-rate):
;   0 IDLE    rate 1, gap 0, output = input EXACTLY (bit-transparent)
;   1 BRAKE   rate = floor + (1-floor)*(1-x)^p, x ramps 0->1 over brake time
;             (floor 0.5 in half-latch mode, else 0); parks at x = 1
;   3 RELEASE rate ramps back to 1 (~0.35 s), then ->4
;   4 XFADE   40 ms equal-power crossfade buffer-read -> live dry, then ->0
;             (mode 2 skips RELEASE: toggle-off jumps straight to an 8 ms
;             XFADE = the hard cut)
; The catch-up trick from tape.orc is NOT used: the leftover read/write gap
; after re-accel is retired by the crossfade, not chased — a 2 s stop would
; otherwise take >10 s of audible pitch-up to close.
;------------------------------------------------------------------
opcode TapeStop, aa, aa
  aInL, aInR xin
  iN  = ftlen(giTSTP_BufL)
  iMinG = 4
  iMaxG = iN - 9600            ; 200 ms guard before the write head's tail
  kDT = ksmps / sr

  ; ---- controls ----
  kMix   portk limit:k(gkTSTP_mix, 0, 1), 0.02, i(gkTSTP_mix)
  kBrkN  limit gkTSTP_brake, 0, 1
  kCrvN  limit gkTSTP_curve, 0, 1
  kEng   = (gkTSTP_bypass < 0.5 ? 1 : 0)
  ; pedal smoothing runs unconditionally so takeover never glides from stale 0
  kExprSm portk limit:k(gkTSTP_expr, 0, 1), 0.03, i(gkTSTP_expr)

  ; ---- gesture state ----
  kSt    init 0                ; 0 idle / 1 brake / 3 release / 4 xfade
  kX     init 0                ; brake progress 0..1
  kRate  init 1
  kXf    init 1                ; xfade position: 1 = wet path, 0 = dry
  kXfT   init 0.04             ; current xfade duration (s)
  kFloor init 0                ; latched rate floor (0 or 0.5)
  kModeL init 0                ; mode latched at engage
  kRst   = 0                   ; 1 = re-zero the read gap this block

  if (kEng == 1 && (kSt == 0 || kSt == 3 || kSt == 4)) then
    ; (re-)engage: latch mode, restart the dive from the current rate
    kModeL = int(limit:k(gkTSTP_mode, 0, 2) + 0.5)
    kFloor = (kModeL == 1 ? 0.5 : 0)
    if (kSt == 0) then
      kRate = 1
      kRst  = 1                ; fresh gesture: read head snaps to write head
    endif
    ; resume the curve from wherever the rate currently is (re-engage during
    ; a release dives from mid-air instead of snapping)
    kXNorm = limit:k((kRate - kFloor) / max:k(1 - kFloor, 0.001), 0, 1)
    kPowE  = 1 + 3 * kCrvN * sqrt(kCrvN)
    kX     = 1 - pow(kXNorm, 1 / kPowE)
    kXf    = 1
    kSt    = 1
  elseif (kEng == 0 && kSt == 1) then
    ; disengage: hard-cut mode jumps to a fast fade, others spin back up
    if (kModeL == 2) then
      kSt  = 4
      kXfT = 0.008
    else
      kSt = 3
    endif
  endif

  if (kSt == 1) then
    kT    = 0.05 * pow(80, kBrkN)            ; 50 ms .. 4 s, exponential
    kX    = limit:k(kX + kDT / kT, 0, 1)
    kPow  = 1 + 3 * kCrvN * sqrt(kCrvN)      ; 1 = linear .. 4 = exponential
    kRate = kFloor + (1 - kFloor) * pow(1 - kX, kPow)
    if (gkTSTP_exprOn > 0.5) then
      ; CC29 manual scrub overrides the automatic trajectory entirely
      kRate = kExprSm
    endif
  elseif (kSt == 3) then
    kRate = kRate + kDT / 0.35               ; spin-up ~0.35 s full range
    if (kRate >= 1) then
      kRate = 1
      kSt   = 4
      kXfT  = 0.04
    endif
  elseif (kSt == 4) then
    kXf = kXf - kDT / kXfT
    if (kXf <= 0) then
      kXf = 0
      kSt = 0
      kRate = 1
      gkTSTP_run = 0                         ; chain stops calling us
    endif
  endif

  aResL init 0
  aResR init 0
  kWr   init 0
  kGap  init 0
  kSplG init 1

  if (kSt == 0) then
    ; ---- IDLE: bit-exact passthrough (chain normally gates us off too) ----
    aResL = aInL
    aResR = aInR
  else
    ; ---- transport: write at 1x, read lags at kRate ----
    if (kRst == 1) then
      kGap = iMinG      ; fresh gesture: read snaps to (just behind) the write
    endif               ; head — 4 smp back keeps the Catmull future taps off
                        ; last lap's stale audio; 0.08 ms, inaudible
    ; head-EMF output law: full level until the reel is nearly still
    kEmf  = sqrt(limit:k(kRate * 12, 0, 1))
    ; splice guard (half-latch can outrun the buffer): 15 ms dip masks the jump
    kSplG = 1 - (1 - kSplG) * exp(-kDT / 0.015)
    aRate interp kRate
    aOutL init 0
    aOutR init 0
    kS = 0
    while kS < ksmps do
      tabw(aInL[kS], kWr, giTSTP_BufL)
      tabw(aInR[kS], kWr, giTSTP_BufR)
      ; rate never exceeds 1, so the gap only ever grows (or holds at rate 1)
      kGap = kGap + 1 - aRate[kS]
      if (kGap > iMaxG) then
        kGap  = iMinG + 4800                 ; splice: jump near the write head
        kSplG = 0                            ; ...under a 15 ms gain dip
      endif
      kRd = kWr - kGap
      kRd = (kRd < 0 ? kRd + iN : kRd)
      kI  = int(kRd)
      kF  = kRd - kI
      kIm = (kI == 0 ? iN - 1 : kI - 1)
      kI1 = (kI + 1 >= iN ? kI + 1 - iN : kI + 1)
      kI2 = (kI + 2 >= iN ? kI + 2 - iN : kI + 2)
      ; 4-point Catmull-Rom, left
      k0 = tab:k(kIm, giTSTP_BufL)
      k1 = tab:k(kI,  giTSTP_BufL)
      k2 = tab:k(kI1, giTSTP_BufL)
      k3 = tab:k(kI2, giTSTP_BufL)
      kL = k1 + 0.5*kF*(k2 - k0 + kF*(2*k0 - 5*k1 + 4*k2 - k3 + kF*(3*(k1 - k2) + k3 - k0)))
      ; right
      k0 = tab:k(kIm, giTSTP_BufR)
      k1 = tab:k(kI,  giTSTP_BufR)
      k2 = tab:k(kI1, giTSTP_BufR)
      k3 = tab:k(kI2, giTSTP_BufR)
      kR = k1 + 0.5*kF*(k2 - k0 + kF*(2*k0 - 5*k1 + 4*k2 - k3 + kF*(3*(k1 - k2) + k3 - k0)))
      aOutL[kS] = kL
      aOutR[kS] = kR
      kWr = (kWr + 1 >= iN ? 0 : kWr + 1)
      kS += 1
    od

    ; ---- mix: (wet*mix + dry*(1-mix)) crossfaded back to pure dry ----
    kWetG = kEmf * kSplG
    aWetL = aOutL * kWetG
    aWetR = aOutR * kWetG
    aMixK interp kMix
    aXfW  interp sqrt(limit:k(kXf, 0, 1))        ; equal-power-ish
    aXfD  interp sqrt(limit:k(1 - kXf, 0, 1))
    aEffL = aWetL * aMixK + aInL * (1 - aMixK)
    aEffR = aWetR * aMixK + aInR * (1 - aMixK)
    aResL = aEffL * aXfW + aInL * aXfD
    aResR = aEffR * aXfW + aInR * aXfD
    aResL limit aResL, -0.99, 0.99
    aResR limit aResR, -0.99, 0.99
  endif
  xout aResL, aResR
endop
