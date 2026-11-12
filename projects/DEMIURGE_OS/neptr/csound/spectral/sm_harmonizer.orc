#define SM_HAVE_HARMONIZ ##
; SM_Harmonizer - source: SoundMagic Spectral "Spectral Harmonizer" (Michael Norris, AU SHrm).
; His description: the spectrum is transposed up/down by up to three intervals (cents) and mixed
;   back into the original; each interval can be expanded into several transpositions with the
;   Spectral Filterbank rules (Stacked / Chord / Harmonics), 'Transpositions per interval' of them.
; Params used: Use Interval 1-3, Interval 1-3 (cents), Gain 1-3 (dB), Transposition Type,
;   Transpositions per interval, Feedback, Wet/Dry Mix. Lo/Hi/RandPhase/Gain are SM_Common's.
; Process: peaks found ONCE per frame (SM_Peaks, Dolson/Laroche regions); each voice moves every
;   region by an integer bin offset and adds df = f*(ratio-1) to the bin frequencies (phase-locked
;   by construction). Voice list per interval I, n transpositions (n=0 treated as 1), k=1..n:
;     Stacked   k*I cents (3 x 4 of 300 = diminished seventh)
;     Chord     I, 1200, I+1200, 2400, ... (interval note and root repeated at octaves)
;     Harmonics ratio 1 + k*I/1200 (I=1200 gives 2n,3n,4n..; 600 gives 1.5n,2n,2.5n..)
;   Voices sum in magnitude (louder contribution keeps the bin's frequency), clamped at 8.
;   Out = (1-wet)*dry + wet*(sum of voices). wet=0 or no voice = bit-exact dry.
; Deviations: n=0 is treated as 1 (an interval with no voice would just be 'Use' off); max 256
;   peaks and 32 voices; regions split at peak midpoints; Harmonics ratio<=0 voices are skipped.
opcode SM_Harmonizer, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  kPM[] init iNb
  kPF[] init iNb
  kOM[] init iNb
  kOF[] init iNb
  kBM[] init iNb
  kPk[] init iNb + 1
  kRP[] init 257
  kRF[] init 257
  kVR[] init 32
  kVG[] init 32
  if kNew == 1 then
    kWet = limit(tab:k($SMP_HARMONIZ_WET_DRY_MIX, giSMP), 0, 100) / 100
    kFb = limit(tab:k($SMP_HARMONIZ_FEEDBACK, giSMP), 0, 99) / 100
    kTyp = int(tab:k($SMP_HARMONIZ_TRANSPOSITION_TYPE, giSMP))
    kNt = limit(int(tab:k($SMP_HARMONIZ_TRANSPOSITIONS_PER_INTERVAL, giSMP)), 1, 10)
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
    ; ---- voice list
    kNv = 0
    ki = 0
    while ki < 3 do
      if tab:k($SMP_HARMONIZ_USE_INTERVAL_1 + ki*3, giSMP) >= 0.5 then
        kI = tab:k($SMP_HARMONIZ_INTERVAL_1 + ki*3, giSMP)
        kG = ampdb(tab:k($SMP_HARMONIZ_GAIN_1 + ki*3, giSMP))
        kv = 1
        while kv <= kNt do
          if kTyp == 0 then
            kR = pow(2, kv * kI / 1200)
          elseif kTyp == 1 then
            kj = kv - 1
            if kj % 2 == 0 then
              kR = pow(2, (kI + 1200 * int(kj / 2)) / 1200)
            else
              kR = pow(2, (1200 * int((kj + 1) / 2)) / 1200)
            endif
          else
            kR = 1 + kv * kI / 1200
          endif
          if kR > 0.01 && kNv < 32 then
            kVR[kNv] = kR
            kVG[kNv] = kG
            kNv += 1
          endif
          kv += 1
        od
      endif
      ki += 1
    od
    if kNv > 0 && kWet > 0 then
      kk = 0
      while kk < iNb do
        kOM[kk] = (1 - kWet) * kMag[kk]
        kBM[kk] = kOM[kk]
        kOF[kk] = kFrq[kk]
        kk += 1
      od
      kPk, kN SM_Peaks kMag, 0.02, 256
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
        kRP[krg] = kp
        kRF[krg] = (kFrq[kp] > 0) ? kFrq[kp] : kp * iBinHz
        krg += 1
      od
      kvi = 0
      while kvi < kNv do
        kRat = kVR[kvi]
        kGn = kVG[kvi] * kWet
        krg = 0
        while krg < kN do
          kf0 = kRF[krg]
          kDf = kf0 * (kRat - 1)
          kd = round((kf0 + kDf) / iBinHz) - kRP[krg]
          kk = kPk[krg]
          ken = kPk[krg + 1]
          while kk < ken do
            kt = kk + kd
            if kt >= 0 && kt < iNb then
              kmv = kMag[kk] * kGn
              if kmv > 0.00001 then
                kOM[kt] = kOM[kt] + kmv
                if kmv > kBM[kt] then
                  kBM[kt] = kmv
                  kOF[kt] = max(0, kFrq[kk] + kDf)
                endif
              endif
            endif
            kk += 1
          od
          krg += 1
        od
        kvi += 1
      od
      kk = 0
      while kk < iNb do
        kMag[kk] = min(kOM[kk], 8)
        kFrq[kk] = kOF[kk]
        kk += 1
      od
    endif
    kPM = kMag
    kPF = kFrq
  endif
  xout kMag, kFrq
endop
