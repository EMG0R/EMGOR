#define SM_HAVE_EMERGE ##
; SM_Emergence - source: SoundMagic Spectral "Spectral Emergence" (Michael Norris, AU SEmg).
; His description: tracks partials and applies an amplitude envelope (fade in / hold / fade out)
;   to each tracked partial, giving a pulsating sound; "Recycle faded peaks" restarts the envelope
;   if the partial is still tracked after all three phases, else it stays silent until it dies.
; Params used: Fade in / Hold / Fade out (windows), Fuzziness, Max. bins per partial, Recycle faded peaks.
; Process: each frame, Dolson/Laroche peaks (SM_Peaks, -60 dB floor, up to 512 peaks). A peak continues a
;   track if a peak existed within +-round(Fuzziness) bins (min 1) in the previous frame; its age then
;   increments, else age=0 (birth). Envelope by age a: fade-in ramp (a+1)/in, then 1 for hold, then
;   linear down over fade-out, then 0 (or age wraps if Recycle). Each peak's gain scales only the bins
;   within Max. bins per partial (centred on the peak, clipped to its region); all other bins are zeroed
;   (only tracked partials are output). Bin frequencies pass through untouched.
; Deviations: his fuzziness/max-bins semantics are unpublished; mine are as above. L and R use
;   independent state but the same rule, so identical mono input gives identical envelopes (his rotating
;   pan comes from per-channel randomness in his tracker, not reproduced).
opcode SM_Emergence, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  kAgeP[] init iNb
  kAgeC[] init iNb
  kOut[] init iNb
  kInit init 0
  if kInit == 0 then
    kz = 0
    while kz < iNb do
      kAgeP[kz] = -1
      kz += 1
    od
    kInit = 1
  endif
  if kNew == 1 then
    kIn = max(floor(tab:k($SMP_EMERGE_FADE_IN_WINDOWS, giSMP) + 0.5), 0)
    kHold = max(floor(tab:k($SMP_EMERGE_HOLD_WINDOWS, giSMP) + 0.5), 0)
    kOutW = max(floor(tab:k($SMP_EMERGE_FADE_OUT_WINDOWS, giSMP) + 0.5), 0)
    kTol = max(floor(tab:k($SMP_EMERGE_FUZZINESS, giSMP) + 0.5), 1)
    kMb = max(floor(tab:k($SMP_EMERGE_MAX_BINS_PER_PARTIAL, giSMP) + 0.5), 1)
    kRec = tab:k($SMP_EMERGE_RECYCLE_FADED_PEAKS, giSMP)
    kTot = kIn + kHold + kOutW
    kPk[], kN SM_Peaks kMag, 0.001, 512
    kk = 0
    while kk < iNb do
      kAgeC[kk] = -1
      kOut[kk] = 0
      kk += 1
    od
    kRg = 0
    while kRg < kN do
      kS = kPk[kRg]
      kE = kPk[kRg+1]
      ; argmax of region
      kP = kS
      kBest = kMag[kS]
      kq = kS + 1
      while kq < kE do
        if kMag[kq] > kBest then
          kBest = kMag[kq]
          kP = kq
        endif
        kq += 1
      od
      ; track continuation: nearest previous peak within kTol
      kAge = 0
      kd = 0
      kFound = 0
      while kd <= kTol && kFound == 0 do
        if kP - kd >= 0 then
          if kAgeP[kP - kd] >= 0 then
            kAge = kAgeP[kP - kd] + 1
            kFound = 1
          endif
        endif
        if kFound == 0 && kd > 0 && kP + kd < iNb then
          if kAgeP[kP + kd] >= 0 then
            kAge = kAgeP[kP + kd] + 1
            kFound = 1
          endif
        endif
        kd += 1
      od
      if kTot > 0 && kRec >= 0.5 && kAge >= kTot then
        kAge = kAge % kTot
      endif
      kAgeC[kP] = kAge
      ; envelope
      kEnv = 0
      if kTot <= 0 then
        kEnv = 1
      elseif kAge < kIn then
        kEnv = (kAge + 1) / kIn
      elseif kAge < kIn + kHold then
        kEnv = 1
      elseif kAge < kTot then
        kEnv = 1 - (kAge - kIn - kHold + 1) / kOutW
      endif
      ; apply to bins around the peak
      kHw = int(kMb / 2)
      kLo = max(kS, kP - kHw)
      kHi = min(kE - 1, kP - kHw + kMb - 1)
      kq = kLo
      while kq <= kHi do
        kOut[kq] = kMag[kq] * kEnv
        kq += 1
      od
      kRg += 1
    od
    kk = 0
    while kk < iNb do
      kMag[kk] = kOut[kk]
      kAgeP[kk] = kAgeC[kk]
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
