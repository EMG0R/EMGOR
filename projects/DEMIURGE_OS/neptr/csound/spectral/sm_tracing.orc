#define SM_HAVE_TRACING ##
; SM_Tracing - source: SoundMagic Spectral "Spectral Tracing" (Michael Norris, AU STra).
; His description: retain only the n loudest bins and remove the others; or the n softest, a
;   range of loudness ranks, or a threshold gate (above / below).
; Params used: Tracing type (0..7), Number of bins 1, Number of bins 2, Threshold (dB; 0 dB = full-scale sine).
; Process: per frame, magnitudes are sorted (sortd) to find rank thresholds, then each bin is kept or zeroed.
;   0 keep loudest n1 | 1 remove loudest n1 | 2 keep softest n1 | 3 remove softest n1
;   4 keep ranks between n1 and n2 (loudest = rank 1) | 5 remove those | 6 keep >= threshold | 7 keep <= threshold.
;   Softest ranking ignores silent bins (mag < 1e-6), otherwise "softest" would just be empty bins.
;   Ranges 4/5 accept n1 > n2 or n1 < n2 (rank span is min..max). Bin frequencies pass through untouched.
; Deviations: ties at the cut value are all kept (so slightly more than n bins may survive on flat spectra);
;   rank-range reading of "n2 = lower end of the range" is my interpretation.
opcode SM_Tracing, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  kS[] init iNb
  if kNew == 1 then
    kTyp = int(limit(tab:k($SMP_TRACING_TRACING_TYPE, giSMP) + 0.5, 0, 7))
    kn1 = int(limit(tab:k($SMP_TRACING_NUMBER_OF_BINS_1, giSMP), 1, iNb))
    kn2 = int(limit(tab:k($SMP_TRACING_NUMBER_OF_BINS_2, giSMP), 1, iNb))
    kThr = ampdb(tab:k($SMP_TRACING_THRESHOLD, giSMP))
    kHi = 1.0e30
    kLo = 0
    kKeep = 1
    if kTyp >= 6 then
      if kTyp == 6 then
        kLo = kThr
      else
        kHi = kThr
      endif
    else
      kS sortd kMag
      kA = 0
      kk = 0
      while kk < iNb do
        if kS[kk] >= 0.000001 then
          kA += 1
        endif
        kk += 1
      od
      if kA > 0 then
        if kTyp < 2 then
          kLo = kS[min(kn1, kA) - 1]
          kKeep = (kTyp == 0) ? 1 : 0
        elseif kTyp < 4 then
          kHi = kS[max(kA - kn1, 0)]
          kKeep = (kTyp == 2) ? 1 : 0
        else
          kHi = kS[min(min(kn1, kn2), kA) - 1]
          kLo = kS[min(max(kn1, kn2), kA) - 1]
          kKeep = (kTyp == 4) ? 1 : 0
        endif
      else
        kLo = 1.0e30
      endif
    endif
    kk = 0
    while kk < iNb do
      kM = kMag[kk]
      kIn = (kM >= kLo && kM <= kHi) ? 1 : 0
      if kIn != kKeep then
        kMag[kk] = 0
      endif
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
