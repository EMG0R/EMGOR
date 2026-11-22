; ATTACK v2 — POG-style pick swell (rewrite 2026-07-03)
; ---------------------------------------------------------------------------
; Signal chain position : 150
; Menu / slot           : VOLUME menu (1), slot 3 (toggle 2, CC21 home)
; Reads                 : gaATTK_sendL / gaATTK_sendR
; Writes                : gaATTK_outL / gaATTK_outR (pure wet; chain ntrpols mix)
; Bypass                : gkATTK_bypass (FX_BYPASS), mix = gkATTK_mix (in chain)
;
; WHAT IT DOES (POG2 attack-slider model, mono-envelope approximation)
;   Every detected note onset restarts a volume swell. A note played from
;   silence swells from true zero; a re-pick while strings still ring ducks
;   to 20% of the current gain (not zero — the POG2 never chops ringing
;   notes, its envelopes are per-note) and swells back to 1. After the swell
;   the gain stays OPEN so the note rings naturally — no hold/auto-fade.
;   When the input goes quiet the gain closes silently, re-arming a
;   from-zero swell for the next note.
;
; DETECTION (hybrid — retriggers mid-ring at 15+ notes/sec)
;   A. Pick detector (US Patent 6,091,013): full-band envelope compared to a
;      ~5ms-delayed copy of itself; a new attack beats the delayed reference
;      by a sens-scaled ratio even while the previous note rings. ~3ms lag.
;   B. Legato detector (SuperFlux, Bock/Widmer DAFx'13): pvsanal 512/128,
;      per-frame sum of positive bin deltas vs a +/-1-bin MAX of the previous
;      frame (rejects vibrato/bend false fires). Catches hammer-ons and
;      same-level pitch changes amplitude detection can't see. Threshold is
;      adaptive (slow EMA x sens multiplier + floor, 50% re-arm hysteresis).
;   Full-band noise gate under both; shared refractory gkATTK_retrigMs
;   (20..150ms, default 45 — tracks 20 notes/sec).
;
;   RANGE voices the detector: 0 = low bass (heavier flux smoothing + slower
;   envelope release to ride out low-B beating), 1 = bright guitar (fastest).
;
;   5ms lookahead (audio delayed, detector isn't) so the duck lands before
;   the pick reaches the output. pvsanal hop 128 MUST stay >= ksmps
;   (crash gotcha, see globals.orc).
;
; CONTROLS (encoder takeover v2)
;   P1 CC21 gkATTK_mix     [0..1]      parallel blend (POG2 dry slider)
;   P2 CC20 gkATTK_atkMs   [10..2000]  swell time, exp feel
;   P3 CC23 gkATTK_sens    [0..1]      detection sensitivity
;   P4 CC24 gkATTK_shape   [0..1]      swell curve percussive..violin
;   P5 CC25 gkATTK_range   [0..1]      detector voicing bass..guitar
;   P6 CC26 gkATTK_retrigMs[20..150]   refractory / flam rejection
;   No expression pedal.
; ---------------------------------------------------------------------------

instr 150
  aL = gaATTK_sendL
  aR = gaATTK_sendR
  aMono = (aL + aR) * 0.5

  ; --- lookahead: audio waits 5ms, detector doesn't
  aLd delay aL, 0.005
  aRd delay aR, 0.005

  kDT = ksmps/sr
  kSens   portk gkATTK_sens,  0.05
  kRange  portk gkATTK_range, 0.05
  kAtkSec = max(0.01, gkATTK_atkMs * 0.001)
  kRetrig = max(0.02, gkATTK_retrigMs * 0.001)

  ; ===================== DETECTOR =====================
  ; --- shared level + noise gate
  kEnvRel = 0.06 + 0.06 * (1 - kRange)      ; 60ms (guitar) .. 120ms (bass)
  aFullE follow2 aMono, 0.0015, kEnvRel
  kEnv downsamp aFullE
  kEnvDb = dbamp(max(kEnv, 0.00001))
  kGateDb = -65 + 15 * (1 - kSens)          ; -50dB (dull) .. -65dB (sensitive)
  kAwake = (kEnvDb > kGateDb ? 1 : 0)

  ; --- A. pick detector: envelope vs its own ~5ms-delayed copy (US6091013)
  ;     armed/disarmed so one rise = one hit (no repeat fire on slow attacks)
  kEnvD1 init 0
  kEnvD2 init 0
  kRefA = max(kEnvD2, 0.00001)
  kEnvD2 = kEnvD1
  kEnvD1 = kEnv
  kARatio = kEnv / kRefA
  kRatio = 1.5 + 1.5 * (1 - kSens)          ; 3.0x (dull) .. 1.5x (sensitive)
  kAArmed init 1
  if (kARatio < 1.15) then
    kAArmed = 1
  endif
  kAHit = (kAArmed == 1 && kARatio > kRatio && kEnvDb > kGateDb + 3 ? 1 : 0)

  ; --- B. SuperFlux spectral onset (legato / re-pick during ring)
  fDet pvsanal aMono, 512, 128, 512, 1
  kMag[]  init 257
  kFrq[]  init 257
  kPrev[] init 257
  kfl pvs2tab kMag, kFrq, fDet
  kPrevFl  init -1
  kFluxRaw init 0
  ; LOG-domain flux (LogFiltSpecFlux): per-bin positive delta of log magnitude
  ; vs a +/-1-bin MAX of the previous frame (SuperFlux vibrato/bend rejection).
  ; Log domain is what makes bass work: a ringing low-B wobbles its loud bins
  ; +/-10% every frame (~0.1 each in log terms) while a fresh attack lights up
  ; dozens of quiet bins by 30-50dB (~7-11 each) — onset >> ring, regardless
  ; of absolute level. Bins below -70dB are ignored so noise can't sum up.
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

  ; low notes beat between frames — bass end smooths harder
  kFluxLag = 0.002 + 0.006 * (1 - kRange)
  kFluxSm  portk kFluxRaw, kFluxLag
  ; adaptive reference = LOWER envelope of flux (instant fall, slow rise):
  ; tracks the between-notes baseline while ignoring onset spikes, so loud
  ; notes can't wind the threshold up and deafen the detector (EMA failure)
  kMult = 1.3 + 2.2 * (1 - kSens)
  kFluxBase init 0
  kFluxBase = min(kFluxSm, kFluxBase + 40 * kDT)
  kFluxTh = kFluxBase * kMult + (3.0 + 8.0 * (1 - kSens))
  kFluxArmed init 1
  if (kFluxSm < kFluxTh * 0.5) then
    kFluxArmed = 1                          ; re-arm hysteresis
  endif
  kBHit = (kFluxArmed == 1 && kFluxSm > kFluxTh ? 1 : 0)

  ; --- C. legato channel: same log-flux at FFT 2048 (23Hz bins), where a
  ;     hammer-on/pull-off actually moves harmonics across bins. The 512
  ;     channel physically cannot see small pitch steps (94Hz bins); this
  ;     one can, at the cost of ~20ms lag — fine, legato has no transient
  ;     to leak. hop 256 >= ksmps (pvsanal crash gotcha).
  fDet2 pvsanal aMono, 2048, 256, 2048, 1
  kMag2[]  init 1025
  kFrq2[]  init 1025
  kPrev2[] init 1025
  kfl2 pvs2tab kMag2, kFrq2, fDet2
  kPrevFl2  init -1
  kFluxRaw2 init 0
  if (kfl2 != kPrevFl2) then
    kPrevFl2 = kfl2
    kFlux2 = 0
    kLeft2 = kPrev2[0]
    ki = 1
    while ki < 1024 do
      kC2 = kPrev2[ki]
      kM2 = kMag2[ki]
      if (kM2 > 0.0003) then
        kD2 = log(kM2 + 0.00001) - log(max(kLeft2, kC2, kPrev2[ki+1]) + 0.00001)
        if (kD2 > 0) then
          kFlux2 = kFlux2 + kD2
        endif
      endif
      kPrev2[ki] = kM2
      kLeft2 = kC2
      ki += 1
    od
    kPrev2[1024] = kMag2[1024]
    kPrev2[0]    = kMag2[0]
    kFluxRaw2 = kFlux2
  endif
  kFluxSm2 portk kFluxRaw2, 0.004
  kFluxBase2 init 0
  kFluxBase2 = min(kFluxSm2, kFluxBase2 + 40 * kDT)
  kFluxTh2 = kFluxBase2 * kMult + (3.0 + 8.0 * (1 - kSens))
  ; this channel's onset spike arrives 40-50ms late (2048 window smear), so
  ; it may not re-arm until 75ms after ANY trigger — its own echo of a
  ; pick the fast channels already caught must decay away while disarmed
  kSinceTrig init 10
  kSinceTrig = kSinceTrig + kDT
  kArmed2 init 1
  if (kSinceTrig > 0.075 && kFluxSm2 < kFluxTh2 * 0.5) then
    kArmed2 = 1
  endif
  kCHit = (kArmed2 == 1 && kFluxSm2 > kFluxTh2 ? 1 : 0)

  ; --- fusion + refractory (+ 120ms mute after instr start: pvsanal warmup
  ;     produces one giant flux frame that must not fire a phantom swell)
  kUp init 0
  kUp = kUp + kDT
  kLock init 0
  if (kLock > 0) then
    kLock = max(kLock - kDT, 0)
  endif
  ; edge semantics: a channel fires only on its 0->1 transition, so a hit
  ; that is still high when the refractory lock expires cannot fire late
  kAPrev init 0
  kBPrev init 0
  kCPrev init 0
  kAEdge = (kAHit == 1 && kAPrev == 0 ? 1 : 0)
  kBEdge = (kBHit == 1 && kBPrev == 0 ? 1 : 0)
  kCEdge = (kCHit == 1 && kCPrev == 0 ? 1 : 0)
  kAPrev = kAHit
  kBPrev = kBHit
  kCPrev = kCHit

  kTrig = 0
  if (kUp > 0.12 && kLock == 0 && kAwake == 1 && (kAEdge == 1 || kBEdge == 1 || kCEdge == 1)) then
    kTrig = 1
    kLock = kRetrig
    kSinceTrig = 0
    kFluxArmed = 0
    kArmed2 = 0
    kAArmed = 0
    gkATTK_trigCount = gkATTK_trigCount + 1
  endif

#ifdef ATKDBG
  printks "DBG t=%.3f env=%.5f refA=%.5f flux=%.5f th=%.5f fx2=%.5f th2=%.5f a=%.0f b=%.0f cc=%.0f awk=%.0f\n", 0.005, timeinsts:k(), kEnv, kRefA, kFluxSm, kFluxTh, kFluxSm2, kFluxTh2, kAHit, kBHit, kCHit, kAwake
#endif

  ; ===================== ENVELOPE =====================
  ; 0 KILL (duck to floor) -> 1 SWELL (floor -> 1) -> 2 OPEN (ring)
  ; 3 CLOSED (input quiet: gain fades to 0, next note swells from silence)
  kStage init 2
  kGain  init 1
  kPhase init 0
  kBase  init 0
  kQuietT init 0
  kAwakeT init 0

  if (kAwake == 0) then
    kQuietT = kQuietT + kDT
    kAwakeT = 0
  else
    kQuietT = 0
    kAwakeT = kAwakeT + kDT
  endif

  if (kTrig == 1) then
    kBase = (kStage == 3 ? 0 : kGain * 0.2) ; re-pick ducks to 20% of current
    kStage = 0
  elseif (kStage != 3 && kQuietT > 0.08) then
    kStage = 3                              ; string muted — close and re-arm
  elseif (kStage == 3 && kAwakeT > 0.15) then
    kBase = 0                               ; safety: sound faded in with no
    kStage = 1                              ; onset — swell anyway, don't
    kPhase = 0                              ; stay stuck silent
  endif

  if (kStage == 0) then
    kGain = max(kGain - kDT / 0.002, kBase)
    if (kGain <= kBase + 0.0001) then
      kStage = 1
      kPhase = 0
    endif
  elseif (kStage == 1) then
    kPhase = min(kPhase + kDT / kAtkSec, 1)
    kExp = pow(4, 2 * gkATTK_shape - 1)     ; 0.25 percussive .. 4 violin
    kGain = kBase + (1 - kBase) * pow(kPhase, kExp)
    if (kPhase >= 1) then
      kStage = 2
    endif
  elseif (kStage == 2) then
    kGain = 1
  else
    kGain = max(kGain - kDT / 0.06, 0)      ; silent close
  endif
  gkATTK_gain = kGain
  aGain interp kGain

  ; ===================== OUTPUT =====================
  aWetL = aLd * aGain
  aWetR = aRd * aGain
  aWetL dcblock aWetL
  aWetR dcblock aWetR
  aWetL limit aWetL, -0.99, 0.99
  aWetR limit aWetR, -0.99, 0.99
  gaATTK_outL = aWetL
  gaATTK_outR = aWetR
endin
