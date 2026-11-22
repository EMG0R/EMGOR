#define SM_HAVE_BINSHIFT ##
; SM_BinShift - source: SoundMagic Spectral "Spectral Bin Shift" (Michael Norris, AU SSft).
; His description: shifts the entire spectrum up/down a set number of bins (linear, so the
;   result is inharmonic).
; Params used: Bins to Shift, Rotate, Instantaneous feedback (%), Num instant feedback,
;   Feedback (%), Original multiplier (%), Shifted multiplier (%). Brightness etc: SM_Common.
; Process: shifted = S(in) + g*S(S(in)) + g^2*S^3(in) ... for n extra same-frame passes
;   (g = instant feedback %); out = orig*in + shifted_mult*shifted. Feedback: previous OUTPUT frame
;   magnitudes are added into the input (and the louder source's frequency wins). Rotate wraps.
;   Frequencies move with their bins (+ shift*sr/N).
; Deviations: magnitudes are clamped at 8 (a full-scale sine is ~1) to stop runaway when
;   orig+shifted multipliers and feedback together exceed unity loop gain.
opcode SM_BinShiftArr, k[]k[], k[]k[]kki
  kMag[], kFrq[], kS, kRot, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  kOM[] init iNb
  kOF[] init iNb
  kk = 0
  while kk < iNb do
    kOM[kk] = 0
    kOF[kk] = kk * iBinHz
    kk += 1
  od
  kk = 0
  while kk < iNb do
    kt = kk + kS
    if kRot >= 0.5 then
      kt = kt % iNb
      if kt < 0 then
        kt += iNb
      endif
    endif
    if kt >= 0 && kt < iNb then
      kOM[kt] = kMag[kk]
      kOF[kt] = max(0, kFrq[kk] + (kt - kk) * iBinHz)
    endif
    kk += 1
  od
  xout kOM, kOF
endop

opcode SM_BinShift, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  kPM[] init iNb
  kPF[] init iNb
  kAM[] init iNb
  kAF[] init iNb
  kCM[] init iNb
  kCF[] init iNb
  if kNew == 1 then
    kS = int(tab:k($SMP_BINSHIFT_BINS_TO_SHIFT, giSMP))
    kRot = tab:k($SMP_BINSHIFT_ROTATE, giSMP)
    kIg = limit(tab:k($SMP_BINSHIFT_INSTANTANEOUS_FEEDBACK, giSMP), 0, 99) / 100
    kIn = int(tab:k($SMP_BINSHIFT_NUM_INSTANT_FEEDBACK, giSMP))
    kFb = limit(tab:k($SMP_BINSHIFT_FEEDBACK, giSMP), 0, 99) / 100
    kOm = tab:k($SMP_BINSHIFT_ORIGINAL_MULTIPLIER, giSMP) / 100
    kSm = tab:k($SMP_BINSHIFT_SHIFTED_MULTIPLIER, giSMP) / 100
    ; feedback of previous output into the input frame
    if kFb > 0 then
      kk = 0
      while kk < iNb do
        kf = kFb * kPM[kk]
        if kf > kMag[kk] then
          kFrq[kk] = kPF[kk]
        endif
        kMag[kk] = min(kMag[kk] + kf, 8)
        kk += 1
      od
    endif
    kAM, kAF SM_BinShiftArr kMag, kFrq, kS, kRot, iN
    if kIn > 0 && kIg > 0 then
      kCM = kAM
      kCF = kAF
      kGn = 1
      ki = 0
      while ki < kIn do
        kCM, kCF SM_BinShiftArr kCM, kCF, kS, kRot, iN
        kGn *= kIg
        kk = 0
        while kk < iNb do
          kv = kCM[kk] * kGn
          if kv > kAM[kk] then
            kAF[kk] = kCF[kk]
          endif
          kAM[kk] = min(kAM[kk] + kv, 8)
          kk += 1
        od
        ki += 1
      od
    endif
    kk = 0
    while kk < iNb do
      kvo = kMag[kk] * kOm
      kvs = kAM[kk] * kSm
      if kvo > kvs then
        kFrq[kk] = kFrq[kk]
      else
        kFrq[kk] = kAF[kk]
      endif
      kMag[kk] = min(kvo + kvs, 8)
      kPM[kk] = kMag[kk]
      kPF[kk] = kFrq[kk]
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
