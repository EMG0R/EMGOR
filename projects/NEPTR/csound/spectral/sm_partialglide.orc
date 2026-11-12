#define SM_HAVE_PARTGLD ##
; SM_PartialGlide - source: SoundMagic Spectral "Spectral Partial Glide" (Michael Norris, AU SPGl).
; His description: tracks partials, shifts their pitches by a set amount per second; the amount can
;   vary per partial (partials glide in opposite directions). Mirror bin flips glide direction below it.
; Params used: Pitch shift per second (cents/s), Pitch shift variance (cents/s, per partial, drawn once
;   when a partial is born), Mirror Bin (% of spectrum), Fade in (windows), Fuzziness (peak threshold,
;   x0.4 % of frame max), Max Bins per Partial (half-width cap of a region of influence),
;   Zero Transients. Lo/Hi/Randomize/Gain are SM_Common.
; Process: SM_Peaks (<=512) -> each peak is matched to a previous-frame track at the same/adjacent bin
;   (else born: age 0, rate = central + var*rand(-1,1)). Every frame offset += rate*hop seconds
;   (rate negated for peaks below the mirror bin). Region moves by the integer bin offset for
;   f*(2^(cents/1200)-1) and every bin's frequency += df (coherent partials). Louder bin wins on overlap.
;   Fade: gain = min(1, age/fade). Zero Transients: partials seen in only one window are muted.
;   Only tracked partials are output (non-peak bins are dropped, as a partial tracker would).
; Deviations: a partial gliding past 0 Hz / Nyquist (or beyond 14400 cents) restarts its glide at 0
;   (sawtooth glissando) rather than vanishing for good. Regions split at peak midpoints.
opcode SM_PartialGlide, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  iDt = (iN/4) / sr
  kOM[] init iNb
  kOF[] init iNb
  kAge[] init iNb
  kOff[] init iNb
  kRte[] init iNb
  kNAge[] init iNb
  kNOff[] init iNb
  kNRte[] init iNb
  kPk[] init iNb + 1
  if kNew == 1 then
    kCen = tab:k($SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, giSMP)
    kVar = tab:k($SMP_PARTGLD_PITCH_SHIFT_VARIANCE, giSMP)
    kMir = tab:k($SMP_PARTGLD_MIRROR_BIN, giSMP) / 100 * iNb
    kFade = tab:k($SMP_PARTGLD_FADE_IN_WINDOWS, giSMP)
    kFz = limit(tab:k($SMP_PARTGLD_FUZZINESS, giSMP), 0, 10) * 0.004
    kMxB = max(1, int(tab:k($SMP_PARTGLD_MAX_BINS_PER_PARTIAL, giSMP)))
    kZT = tab:k($SMP_PARTGLD_ZERO_TRANSIENTS, giSMP)
    kk = 0
    while kk < iNb do
      kOM[kk] = 0
      kOF[kk] = kk * iBinHz
      kNAge[kk] = 0
      kk += 1
    od
    kPk, kN SM_Peaks kMag, kFz, 512
    krg = 0
    while krg < kN do
      kst = kPk[krg]
      ken = kPk[krg + 1]
      kp = kst
      kb = 0
      kk = kst
      while kk < ken do
        if kMag[kk] > kb then
          kb = kMag[kk]
          kp = kk
        endif
        kk += 1
      od
      ; match to previous track
      kAg = 0
      kOf = 0
      kRt = 0
      kHit = -1
      if kAge[kp] > 0 then
        kHit = kp
      elseif kp > 0 && kAge[max(kp-1,0)] > 0 then
        kHit = kp - 1
      elseif kp < iNb - 1 && kAge[min(kp+1,iNb-1)] > 0 then
        kHit = kp + 1
      endif
      if kHit >= 0 then
        kAg = kAge[kHit]
        kOf = kOff[kHit]
        kRt = kRte[kHit]
      else
        kRt = kCen + kVar * random:k(-1, 1)
      endif
      kAg += 1
      kSg = (kp < kMir) ? -1 : 1
      kOf += kSg * kRt * iDt
      kf0 = (kFrq[kp] > 0) ? kFrq[kp] : kp * iBinHz
      kDf = kf0 * (pow(2, kOf / 1200) - 1)
      if abs(kOf) > 14400 || kf0 + kDf < 0 || kf0 + kDf >= sr * 0.5 then
        kOf = 0
        kDf = 0
      endif
      kNAge[kp] = kAg
      kNOff[kp] = kOf
      kNRte[kp] = kRt
      kGn = 1
      if kFade > 0 then
        kGn = min(1, kAg / kFade)
      endif
      if kZT >= 0.5 && kAg < 2 then
        kGn = 0
      endif
      if kGn > 0 then
        kd = round((kf0 + kDf) / iBinHz) - kp
        kst2 = max(kst, kp - kMxB)
        ken2 = min(ken, kp + kMxB + 1)
        kk = kst2
        while kk < ken2 do
          kt = kk + kd
          if kt >= 0 && kt < iNb && kMag[kk] * kGn > kOM[limit(kt,0,iNb-1)] then
            kOM[kt] = kMag[kk] * kGn
            kOF[kt] = max(0, kFrq[kk] + kDf)
          endif
          kk += 1
        od
      endif
      krg += 1
    od
    kAge = kNAge
    kOff = kNOff
    kRte = kNRte
    kMag = kOM
    kFrq = kOF
  endif
  xout kMag, kFrq
endop
