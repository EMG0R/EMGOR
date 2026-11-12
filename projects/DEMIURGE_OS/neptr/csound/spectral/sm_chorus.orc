#define SM_HAVE_CHORUS ##
; SM_Chorus - source: SoundMagic Spectral "Chorus" (Michael Norris, AU Chor): "A time-varying chorus/flanger effect".
; TIME-DOMAIN effect (no FFT). Signature: aOutL, aOutR SM_Chorus aInL, aInR, ifx   (call every k-cycle, one call site).
; Params: Delay (ms), Delay Variance Type (0 None, 1 Random, 2 LFO, 3 Link amp, 4 Link inverted amp),
;   Delay Variance (ms, +-), LFO Rate (Hz), Feedback (%), Wet/dry mix (%), Gain (dB).
; Algorithm: per channel a feedback delay line (delayr/delayw, cubic-interpolated deltap3) whose delay time is
;   Delay + modulation. LFO = sine, R channel 90 deg out of phase with L (stereo spread). Random = interpolated
;   noise (randi) at LFO Rate, independent per channel. Link = input envelope (0..1) scales the variance.
;   Wet = delay output; Feedback = delay output fed (tanh-bounded) back into its input. Mix is linear.
; Approximations: min delay = ksmps/sr + 1 sample (feedback-loop block limit, ~1.4 ms at ksmps 64), so
;   Delay 0 behaves as the shortest flange; feedback capped at 98% and soft-saturated (tanh) so 100% is stable;
;   Delay/mix/gain/feedback/variance params are smoothed (portk) for click-free changes, the delay-time slew
;   (30 ms) also acts as his time smoothing; Random is band-limited noise rather than per-sample steps.
opcode SM_Chorus, aa, aai
  aInL, aInR, ifx xin
  iMinD = (ksmps + 1) / sr
  kDel   = limit(SM_P(ifx, $SMP_CHORUS_DELAY), 0, 100)
  kType  = int(limit(SM_P(ifx, $SMP_CHORUS_DELAY_VARIANCE_TYPE), 0, 4) + 0.5)
  kVar   = limit(SM_P(ifx, $SMP_CHORUS_DELAY_VARIANCE), 0, 100)
  kRate  = limit(SM_P(ifx, $SMP_CHORUS_LFO_RATE), 0.001, 200)
  kFb    = limit(SM_P(ifx, $SMP_CHORUS_FEEDBACK), 0, 100) * 0.0098
  kWet   = limit(SM_P(ifx, $SMP_CHORUS_WET_DRY_MIX), 0, 100) * 0.01
  kGain  = limit(SM_P(ifx, $SMP_CHORUS_GAIN), -30, 30)
  ; smoothing (init to first value to avoid a startup ramp)
  kDelS  portk kDel, 0.03, i(kDel)
  kVarS  portk kVar, 0.03, i(kVar)
  kFbS   portk kFb, 0.02, i(kFb)
  kWetS  portk kWet, 0.02, i(kWet)
  kGnT   = ampdb(kGain)
  kGnS   portk kGnT, 0.02, i(kGnT)
  ; input envelope 0..1 for Link modes
  kEnv   rms (aInL + aInR) * 0.5, 8
  kAmp   = limit(kEnv * 3, 0, 1)
  kAmpS  portk kAmp, 0.05
  ; modulation shapes, a-rate, +-1
  aPh    phasor kRate
  aLfoL  = sin(6.283185307 * aPh)
  aLfoR  = cos(6.283185307 * aPh)
  aRndL  randi 1, kRate, 0.31
  aRndR  randi 1, kRate, 0.77
  if kType == 2 then
    aModL = aLfoL * kVarS
    aModR = aLfoR * kVarS
  elseif kType == 1 then
    aModL = aRndL * kVarS
    aModR = aRndR * kVarS
  elseif kType == 3 then
    aModL = a(kAmpS * kVarS)
    aModR = aModL
  elseif kType == 4 then
    aModL = a((1 - kAmpS) * kVarS)
    aModR = aModL
  else
    aModL = a(0)
    aModR = a(0)
  endif
  aTL    = limit((kDelS + aModL) * 0.001, iMinD, 0.28)
  aTR    = limit((kDelS + aModR) * 0.001, iMinD, 0.28)
  aBufL  delayr 0.3
  aDL    deltap3 aTL
         delayw aInL + tanh(aDL * kFbS)
  aBufR  delayr 0.3
  aDR    deltap3 aTR
         delayw aInR + tanh(aDR * kFbS)
  aOutL  = (aInL * (1 - kWetS) + aDL * kWetS) * kGnS
  aOutR  = (aInR * (1 - kWetS) + aDR * kWetS) * kGnS
  xout aOutL, aOutR
endop
