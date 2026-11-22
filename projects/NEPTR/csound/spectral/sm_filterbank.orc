#define SM_HAVE_FILTBANK ##
; SM_Filterbank - source: SoundMagic Spectral "Spectral Filterbank" (Michael Norris, AU SFbk).
; His description: a bank of extremely narrow, pure bandpass filters (GRM Reson-like); N filters
;   built from a base frequency and an interval by Stacked / Chord / Harmonics transposition;
;   Harmonic gain attenuates each higher filter; Side bins widen each filter with decaying gain.
; Params used: Number of filters, Base frequency, Interval (cents), Transposition type
;   (0 Stacked, 1 Chord, 2 Harmonics), Harmonic gain, Side bins, Side bin gain, Feedback, Wet/Dry Mix.
; Process: a per-bin gain mask is built (only when a parameter changed): filter i at frequency f_i
;   keeps the nearest bin round(f_i/binHz) at gain G_i = 10^(hg*i/20); side bins at +-j keep
;   G_i*10^(sg*j/20); overlaps take the max; everything else is 0. Stacked f_i = base*2^(i*int/1200);
;   Chord: notes base and base*2^(int/1200) repeated per octave; Harmonics f_i = base*(i+1)*int/1200
;   (1200 cents = ordinary harmonic series). Magnitudes are multiplied by the mask; bin
;   frequencies are untouched. Feedback adds the previous wet output (magnitudes) before filtering.
; Deviations: "lin. gain" units of Harmonic gain / Side bin gain are read as dB per step (per
;   filter / per side bin); mask gain capped at 4. Frequencies snap to the nearest bin (his
;   FFT-limited accuracy). Wet/Dry mixes magnitudes of filtered vs input frame.
opcode SM_Filterbank, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  kMask[] init iNb
  kPrev[] init iNb
  kSig init -1
  if kNew == 1 then
    kNum = int(limit(tab:k($SMP_FILTBANK_NUMBER_OF_FILTERS, giSMP), 1, 400))
    kBase = max(tab:k($SMP_FILTBANK_BASE_FREQUENCY, giSMP), 1)
    kInt = tab:k($SMP_FILTBANK_INTERVAL, giSMP)
    kTyp = int(tab:k($SMP_FILTBANK_TRANSPOSITION_TYPE, giSMP) + 0.5)
    kHg = tab:k($SMP_FILTBANK_HARMONIC_GAIN, giSMP)
    kSb = int(limit(tab:k($SMP_FILTBANK_SIDE_BINS, giSMP), 0, 40))
    kSg = tab:k($SMP_FILTBANK_SIDE_BIN_GAIN, giSMP)
    kFb = limit(tab:k($SMP_FILTBANK_FEEDBACK, giSMP), 0, 99) / 100
    kWet = limit(tab:k($SMP_FILTBANK_WET_DRY_MIX, giSMP), 0, 100) / 100
    kS = kNum + kBase * 3.1 + kInt * 1.7 + kTyp * 977 + kHg * 131 + kSb * 7919 + kSg * 53
    if kS != kSig then
      kSig = kS
      kk = 0
      while kk < iNb do
        kMask[kk] = 0
        kk += 1
      od
      kRi = pow(2, kInt / 1200)
      ki = 0
      while ki < kNum do
        if kTyp == 0 then
          kf = kBase * pow(kRi, ki)
        elseif kTyp == 1 then
          kf = kBase * pow(2, int(ki / 2)) * ((ki % 2 == 1) ? kRi : 1)
        else
          kf = kBase * (ki + 1) * kInt / 1200
        endif
        kb = int(kf / iBinHz + 0.5)
        if kb >= 0 && kb < iNb && kf > 0 then
          kG = min(pow(10, limit(kHg * ki, -120, 40) / 20), 4)
          if kG > kMask[kb] then
            kMask[kb] = kG
          endif
          kj = 1
          while kj <= kSb do
            kGs = kG * pow(10, kSg * kj / 20)
            if kb - kj >= 0 && kGs > kMask[max(kb - kj,0)] then
              kMask[kb - kj] = kGs
            endif
            if kb + kj < iNb && kGs > kMask[min(kb + kj,iNb-1)] then
              kMask[kb + kj] = kGs
            endif
            kj += 1
          od
        endif
        ki += 1
      od
    endif
    kk = 0
    while kk < iNb do
      kIn = kMag[kk]
      kx = min(kIn + kFb * kPrev[kk], 8)
      kw = kx * kMask[kk]
      kPrev[kk] = min(kw, 8)
      kMag[kk] = kw * kWet + kIn * (1 - kWet)
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
