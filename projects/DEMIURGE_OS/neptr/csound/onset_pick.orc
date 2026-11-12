; ONSET PICK — shared pick-onset detector UDO (Chase Bliss batch, 2026-07-13)
; ---------------------------------------------------------------------------
; Consumers   : ONWARD (155), IMPULSE SYNTH (156)
; Provenance  : reimplements ATTACK v2's pick channels A+B (attack.orc) as a
;               standalone UDO. attack.orc is deliberately UNTOUCHED — see
;               INTEGRATION NOTES below.
;
; INTEGRATION NOTES (for the integration agent — this file creates no globals)
;   * neptrPhase4.csd: `#include "onset_pick.orc"` BEFORE any effect that uses
;     it (onward.orc, impulse_synth.orc). Safe to include before globals'
;     consumers; the UDO references no gk/ga globals.
;   * OPTIONAL LATER REFACTOR: attack.orc channels A+B duplicate this logic
;     (its channel C legato detector is attack-only). Converting attack.orc to
;     call OnsetPick is a separate, benched change (scripts/bench/
;     analyze_attack.py must stay ALL PASS) — do not fold it in casually.
;
; WHAT IT DETECTS (per-instance state; one call = one detector)
;   A. Pick detector (US 6,091,013): full-band envelope vs a ~5ms-delayed
;      copy of itself; a new attack beats the delayed reference by a
;      sens-scaled ratio even while the previous note rings. ~3ms lag.
;   B. SuperFlux spectral onset (Bock/Widmer DAFx'13), FFT 512 hop 128,
;      LOG-domain positive flux vs ±1-bin MAX of the previous frame —
;      catches re-picks during ring that amplitude can't see.
;   Full-band noise gate under both; shared refractory; 120ms startup mute
;   (pvsanal warmup fires one giant flux frame otherwise).
;   No channel C (2048 legato) — freeze/synth retrigger on PICKS only.
;
; USAGE
;   kTrig, kEnv OnsetPick aMono, kSens, kRetrigSec
;     kTrig 1 for exactly one k-cycle per detected onset, else 0
;     kEnv  linear full-band envelope (velocity sampling at kTrig time)
;     kSens 0..1 (0 dull .. 1 hair-trigger), kRetrigSec refractory (>=0.02)
; ---------------------------------------------------------------------------

opcode OnsetPick, kk, akk
  aMono, kSensIn, kRetrigIn xin

  kDT = ksmps/sr
  kSens   limit kSensIn, 0, 1
  kRetrig = max(0.02, kRetrigIn)

  ; --- shared level + noise gate
  aFullE follow2 aMono, 0.0015, 0.08
  kEnv downsamp aFullE
  kEnvDb = dbamp(max(kEnv, 0.00001))
  kGateDb = -65 + 15 * (1 - kSens)
  kAwake = (kEnvDb > kGateDb ? 1 : 0)

  ; --- A. envelope vs its own delayed copy, armed/disarmed
  kEnvD1 init 0
  kEnvD2 init 0
  kRefA = max(kEnvD2, 0.00001)
  kEnvD2 = kEnvD1
  kEnvD1 = kEnv
  kARatio = kEnv / kRefA
  kRatio = 1.5 + 1.5 * (1 - kSens)
  kAArmed init 1
  if (kARatio < 1.15) then
    kAArmed = 1
  endif
  kAHit = (kAArmed == 1 && kARatio > kRatio && kEnvDb > kGateDb + 3 ? 1 : 0)

  ; --- B. SuperFlux log-domain flux, FFT 512 hop 128 (hop >= ksmps gotcha)
  fDet pvsanal aMono, 512, 128, 512, 1
  kMag[]  init 257
  kFrq[]  init 257
  kPrev[] init 257
  kfl pvs2tab kMag, kFrq, fDet
  kPrevFl  init -1
  kFluxRaw init 0
  if (kfl != kPrevFl) then
    kPrevFl = kfl
    kFlux = 0
    kLeft = kPrev[0]
    ki = 1
    while ki < 256 do
      kC = kPrev[ki]
      kM = kMag[ki]
      if (kM > 0.0003) then
        kD = log(kM + 0.00001) - log(max(kLeft, kC, kPrev[ki+1]) + 0.00001)
        if (kD > 0) then
          kFlux = kFlux + kD
        endif
      endif
      kPrev[ki] = kM
      kLeft = kC
      ki += 1
    od
    kPrev[256] = kMag[256]
    kPrev[0]   = kMag[0]
    kFluxRaw = kFlux
  endif
  kFluxSm portk kFluxRaw, 0.004
  ; adaptive LOWER-envelope reference (instant fall, slow rise)
  kMult = 1.3 + 2.2 * (1 - kSens)
  kFluxBase init 0
  kFluxBase = min(kFluxSm, kFluxBase + 40 * kDT)
  kFluxTh = kFluxBase * kMult + (3.0 + 8.0 * (1 - kSens))
  kFluxArmed init 1
  if (kFluxSm < kFluxTh * 0.5) then
    kFluxArmed = 1
  endif
  kBHit = (kFluxArmed == 1 && kFluxSm > kFluxTh ? 1 : 0)

  ; --- fusion + refractory + startup mute (edge semantics: 0->1 only)
  kUp init 0
  kUp = kUp + kDT
  kLock init 0
  if (kLock > 0) then
    kLock = max(kLock - kDT, 0)
  endif
  kAPrev init 0
  kBPrev init 0
  kAEdge = (kAHit == 1 && kAPrev == 0 ? 1 : 0)
  kBEdge = (kBHit == 1 && kBPrev == 0 ? 1 : 0)
  kAPrev = kAHit
  kBPrev = kBHit

  kTrig = 0
  if (kUp > 0.12 && kLock == 0 && kAwake == 1 && (kAEdge == 1 || kBEdge == 1)) then
    kTrig = 1
    kLock = kRetrig
    kFluxArmed = 0
    kAArmed = 0
  endif

  xout kTrig, kEnv
endop
