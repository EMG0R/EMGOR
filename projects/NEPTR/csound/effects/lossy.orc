; LOSSY — Codec Degradation (Chase Bliss Lossy / Goodhertz family)
; ---------------------------------------------------------------------------
; Signal chain position : stage 11.7 (after BINCRUSHER/122, before TAPE/12)
; Instr / toggle        : instr 123, toggle ID 46
; Menu / slot           : FILTERING menu (4), slot 0 ("lsy", "LOSSY")
; Reads                 : gaLsySendL / gaLsySendR
; Writes                : gaLsyWetL  / gaLsyWetR
; Controls              : slot rotation -> gkLSY_mix   (default 1.0, wet)
;                         CC20 secondary -> gkLSY_loss (default 0.35)
;                         CC23 tertiary  -> gkLSY_bw   (default 0.8)
;                         P4 takeover    -> gkLSY_pkt  (default 0)
;                         CC29: none in v1 (sticky pedal stays elsewhere)
;
; WHAT IT DOES (three codec-failure stages, in order)
;   1. LOSS — energy-thresholded SPECTRAL GATE with hysteresis + per-bin
;      30 ms release. Bins below a threshold derived from the running frame
;      mean (so it breathes with playing) are ZEROED — not scaled (BINCRUSH
;      121 = contrast exponentiation), not selected/coarsened (BINCRUSHER
;      122 = group peak-picking). Quiet content vanishes and swirls back in
;      as it crosses the threshold: the "underwater MP3" artifact.
;   2. PACKETS — k-rate event scheduler; gkLSY_pkt sweeps mute-probability
;      (0..0.5: 10-80 ms dropouts, 3 ms declick ramps) into repeat-
;      probability (0.5..1: 20-120 ms buffer-repeat transfer errors).
;   3. BANDWIDTH — gkLSY_bw 1..0 narrows full-range down to telephone band
;      (300 Hz - 3.4 kHz butterworth pair) with a gentle sample-repeat fold
;      (aliasing) blended in over the narrowest third. Transparent-bypassed
;      above bw=0.995 so loss-only settings null cleanly.
;
;   No balance/makeup: losing energy IS the effect (the pedal gets thinner
;   as loss rises; resurrecting loudness would fight the character).
;
; FFT: reuses the giBin* family (1024/256/1024, 4x overlap, ~21 ms wet
;   latency, hop 256 = 2x ksmps). Default mix = 1.0 (character effect, 100%
;   wet like the bincrushers; partial mix combs against the 21 ms latency —
;   allowed, occasionally useful, but 1.0 is the intended sound).
;
; INTEGRATION NOTES (for the integration agent — shared files NOT touched here)
;   globals.orc     : NOTHING to add — all LOSSY globals are self-declared
;                     below (include order: globals -> effects -> chain).
;                     Do NOT redeclare them.
;   signal_chain.orc: insert at stage 11.7, directly after the BINCRUSHER
;                     block (11.6) and before TAPE (12):
;                       ; 11.7. LOSSY (123) — codec degradation
;                       gaLsySendL = aPrevL
;                       gaLsySendR = aPrevR
;                       $FX_BYPASS(gkLsyBypass'123'gaLsySendL'gaLsySendR'gaLsyWetL'gaLsyWetR'lsy)
;                       aPrevL = gaLsyWetL
;                       aPrevR = gaLsyWetR
;   midi_handler.orc: toggle ID 46 <-> gkLsyBypass (menu 4 slot 0, physical
;                     CC74); toggle-on defaults mix=1.0 loss=0.35 bw=0.8
;                     pkt=0; slot rotation -> gkLSY_mix; CC20 -> gkLSY_loss;
;                     CC23 -> gkLSY_bw; encoder-takeover P4 -> gkLSY_pkt;
;                     OSC sends: /lsy_state, /lsy_mix, /lsy_loss, /lsy_bw,
;                     /lsy_pkt (change-triggered, house pattern).
;   neptrPhase4.csd : #include "effects/lossy.orc" after bincrusher.orc.
;   neptrPhase4_UI.py: MENUS menu 4 slot 0 = ("lsy", "LOSSY");
;                     EFFECT_PARAMS["lsy"] = [("mix","Mix",0,1),
;                       ("loss","Loss",0,1),("bw","Bandwidth",0,1),
;                       ("pkt","Packets",0,1)]; route_osc handles /lsy_*.
;   docs            : registry row — 46 LOSSY instr 123 menu 4 s0; next
;                     free toggle after batch: see batch spec.
; ---------------------------------------------------------------------------

; ---- self-declared globals (see INTEGRATION NOTES) ----
gkLsyBypass init 1
gkLSY_mix   init 1.0
gkLSY_loss  init 0.35
gkLSY_bw    init 0.8
gkLSY_pkt   init 0
gaLsySendL  init 0
gaLsySendR  init 0
gaLsyWetL   init 0
gaLsyWetR   init 0

; --- SpectralGate UDO -------------------------------------------------------
; Energy-thresholded gate with hysteresis and per-bin release.
; Threshold breathes with the frame: ThrHi = frameMean * 8 * loss^2
;   loss=0    -> ThrHi=0    -> every bin passes untouched (bit-transparent)
;   loss=0.35 -> ~1x mean   -> below-average bins start swirling out
;   loss=0.7  -> ~3.9x mean -> only the strongest partials survive
; Hysteresis: open at ThrHi, start releasing below 0.6*ThrHi. Release is a
; per-bin one-pole (~30 ms), then a hard zero below 1% — bins truly vanish
; (the defining difference from BINCRUSH's smooth exponent scaling).
; kGate[] is caller-owned state: pass the same array back in every frame.
opcode SpectralGate, k[]k[], k[]k[]kk
  kIn[], kGate[], kLoss, kRel xin
  iN = lenarray:i(kIn)
  kOut[] init iN

  ; running frame mean
  kSum = 0
  ki = 0
  while ki < iN do
    kSum += kIn[ki]
    ki += 1
  od
  kMean  = kSum / iN
  kThrHi = kMean * 8 * kLoss * kLoss
  kThrLo = kThrHi * 0.6

  ki = 0
  while ki < iN do
    if (kIn[ki] >= kThrHi) then
      kGate[ki] = 1
    elseif (kIn[ki] < kThrLo) then
      kGate[ki] = kGate[ki] * kRel
      kGate[ki] = (kGate[ki] < 0.01 ? 0 : kGate[ki])
    endif
    kOut[ki] = kIn[ki] * kGate[ki]
    ki += 1
  od
  xout kOut, kGate
endop

instr 123
  kMix  portk gkLSY_mix,  gkPortTime
  kLoss portk gkLSY_loss, gkPortTime
  kBw   portk gkLSY_bw,   gkPortTime
  kPkt  portk gkLSY_pkt,  gkPortTime
  iRel  = exp(-1 / (0.030 * kr))          ; 30 ms per-bin release

  aInL = gaLsySendL
  aInR = gaLsySendR

  ; ---- stage 1: LOSS (spectral gate) ----
  fInL    pvsanal aInL, giBinFFT, giBinHop, giBinWin, 1
  kMagL[] init giBinBins
  kFrqL[] init giBinBins
  kGateL[] init giBinBins
  kflL    pvs2tab kMagL, kFrqL, fInL
  kMagL, kGateL SpectralGate kMagL, kGateL, kLoss, iRel
  fOutL   tab2pvs kMagL, kFrqL, giBinHop, giBinWin, 1
  aGateL  pvsynth fOutL

  fInR    pvsanal aInR, giBinFFT, giBinHop, giBinWin, 1
  kMagR[] init giBinBins
  kFrqR[] init giBinBins
  kGateR[] init giBinBins
  kflR    pvs2tab kMagR, kFrqR, fInR
  kMagR, kGateR SpectralGate kMagR, kGateR, kLoss, iRel
  fOutR   tab2pvs kMagR, kFrqR, giBinHop, giBinWin, 1
  aGateR  pvsynth fOutR

  ; ---- stage 2: PACKETS (shared scheduler, both channels drop together) ----
  kAtt  metro 8                            ; candidate events, 8/s
  kP    trandom kAtt, 0, 1
  kDurR trandom kAtt, 0, 1
  kTypR trandom kAtt, 0, 1
  kActive init 0
  kCnt    init 0
  kRepMix init 0
  kDlyMs  init 30
  kProb = (kPkt < 0.5 ? kPkt * 2 : 1) * 0.9
  if (kAtt == 1 && kActive == 0 && kPkt > 0.005 && kP < kProb) then
    kRepMix = (kTypR < limit((kPkt - 0.5) * 2, 0, 1) ? 1 : 0)
    kDurS   = (kRepMix == 1 ? 0.020 + 0.100 * kDurR : 0.010 + 0.070 * kDurR)
    kDlyMs  = kDurS * 1000
    kCnt    = kDurS * kr
    kActive = 1
  endif
  kEnvT = 0
  if (kActive == 1) then
    kEnvT = 1
    kCnt -= 1
    kActive = (kCnt <= 0 ? 0 : 1)
  endif
  kEnv  portk kEnvT, 0.003                 ; 3 ms declick ramps
  aEnv  interp kEnv
  aTapL vdelay3 aGateL, kDlyMs, 200
  aTapR vdelay3 aGateR, kDlyMs, 200
  aPktL = aGateL * (1 - aEnv) + aTapL * aEnv * kRepMix
  aPktR = aGateR * (1 - aEnv) + aTapR * aEnv * kRepMix

  ; ---- stage 3: BANDWIDTH (telephone band + fold over narrowest third) ----
  if (kBw > 0.995) then
    aBwL = aPktL                           ; clean bypass: loss-only nulls
    aBwR = aPktR
  else
    kFoldAmt = 1 + limit((0.33 - kBw) / 0.33, 0, 1) * 3
    kLP = 3400 * pow(21000 / 3400, kBw)
    kHP = 300 * pow(20 / 300, kBw)
    aFdL fold aPktL, kFoldAmt
    aFdR fold aPktR, kFoldAmt
    aBwL buthp aFdL, kHP
    aBwR buthp aFdR, kHP
    aBwL butlp aBwL, kLP
    aBwR butlp aBwR, kLP
  endif

  ; ---- mix + safety ----
  aOutL ntrpol aInL, aBwL, kMix
  aOutR ntrpol aInR, aBwR, kMix
  aOutL dcblock aOutL
  aOutR dcblock aOutR
  aOutL limit aOutL, -0.99, 0.99
  aOutR limit aOutR, -0.99, 0.99

  gaLsyWetL = aOutL
  gaLsyWetR = aOutR
endin
