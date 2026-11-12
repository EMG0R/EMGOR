#define SM_HAVE_GRANUL ##
; SM_Granulation - source: SoundMagic Spectral "Spectral Granulation" (Michael Norris, AU SGrn).
; His description: grains = chunks of the spectrum (a band of bins x a duration) are taken and
;   delayed by a length; optional bin shift and fade in/out per grain. Output is ONLY the grains
;   (Density 0 % = nothing gets out), no dry path.
; Params used: Grain length (s), Grain length variance, Grain delay (s), Grain delay variance,
;   Bin shift, Bin shift variance, Bins per grain, Fade amount (%), Density (%), Feedback (%).
; Process: every analysis frame (mag+freq) is written into a ring (7.5 s of frames). The spectrum is
;   cut into groups of "Bins per grain" bins; each group runs its own grain clock. A grain draws,
;   once, a length (frames, floor, min 1), a delay (frames), a bin shift and a density coin flip;
;   while it lasts the group's output bins are read from the ring `delay` frames ago, moved by the
;   shift (frequency += shift*sr/N so moved partials stay coherent) and multiplied by a triangular
;   fade envelope (fade 100 % = full triangle). Overlaps from shifting merge by max.
;   Group clocks start at random phase so groups do not all re-roll on the same frame.
; Deviations: grain length is rounded to frames (his: to FFT length); feedback mixes the previous
;   output frame into the input by sum with fb capped 0.99 and mags clamped to 1 so it cannot run away.
opcode SM_Granulation, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iHop = iN/4
  ifps = sr / iHop
  iRing = int(7.5 * ifps) + 3
  kRM[] init iRing * iNb
  kRF[] init iRing * iNb
  kOM[] init iNb
  kOF[] init iNb
  kPM[] init iNb
  kPF[] init iNb
  kLeft[] init iNb
  kLen[] init iNb
  kDel[] init iNb
  kShf[] init iNb
  kOn[] init iNb
  kW init 0
  kInit init 0
  if kNew == 1 then
    kLenS = tab:k($SMP_GRANUL_GRAIN_LENGTH, giSMP)
    kLenV = tab:k($SMP_GRANUL_GRAIN_LENGTH_VARIANCE, giSMP)
    kDelS = tab:k($SMP_GRANUL_GRAIN_DELAY, giSMP)
    kDelV = tab:k($SMP_GRANUL_GRAIN_DELAY_VARIANCE, giSMP)
    kShS = tab:k($SMP_GRANUL_BIN_SHIFT, giSMP)
    kShV = tab:k($SMP_GRANUL_BIN_SHIFT_VARIANCE, giSMP)
    kB = max(1, int(tab:k($SMP_GRANUL_BINS_PER_GRAIN, giSMP)))
    kFade = limit(tab:k($SMP_GRANUL_FADE_AMOUNT, giSMP), 0, 100) / 100
    kDens = limit(tab:k($SMP_GRANUL_DENSITY, giSMP), 0, 100)
    kFb = limit(tab:k($SMP_GRANUL_FEEDBACK, giSMP), 0, 99) / 100
    kBinHz = sr / iN
    kFl = 0.5 * kFade
    ; ---- write input (+ feedback of last output) into the ring
    kBase = kW * iNb
    kk = 0
    while kk < iNb do
      kM = kMag[kk]
      kF = kFrq[kk]
      if kFb > 0 && kPM[kk] > 0 then
        if kPM[kk] * kFb > kM then
          kF = kPF[kk]
        endif
        kM = min(kM + kFb * kPM[kk], 1)
      endif
      kRM[kBase + kk] = kM
      kRF[kBase + kk] = kF
      kk += 1
    od
    ; ---- per group: advance clocks, read delayed bins
    kk = 0
    while kk < iNb do
      kOM[kk] = 0
      kOF[kk] = 0
      kk += 1
    od
    kg = 0
    while kg < iNb do
      kS = kg
      kE = min(kg + kB, iNb)
      if kInit == 0 then
        ; first call: random phase into the first grain, zero left forces a roll below
        kLeft[kS] = 0
      endif
      if kLeft[kS] <= 0 then
        kL = limit(kLenS + kLenV * random:k(-1, 1), 0.02, 4)
        kLen[kS] = max(1, int(kL * ifps))
        kLeft[kS] = kLen[kS]
        if kInit == 0 then
          kLeft[kS] = max(1, int(random:k(0, 1) * kLen[kS]) + 1)
        endif
        kD = max(0, kDelS + kDelV * random:k(-1, 1))
        kDel[kS] = limit(round(kD * ifps), 0, iRing - 2)
        kShf[kS] = round(kShS + kShV * random:k(-1, 1))
        kOn[kS] = (random:k(0, 100) < kDens) ? 1 : 0
      endif
      if kOn[kS] == 1 then
        kPos = kLen[kS] - kLeft[kS]
        kT = (kPos + 0.5) / kLen[kS]
        kEnv = 1
        if kFl > 0 then
          kEnv = min(1, min(kT / kFl, (1 - kT) / kFl))
        endif
        kRd = kW - kDel[kS]
        if kRd < 0 then
          kRd += iRing
        endif
        kRb = kRd * iNb
        kSh = kShf[kS]
        kj = kS
        while kj < kE do
          kd = kj + kSh
          if kd >= 0 && kd < iNb then
            kV = kRM[kRb + kj] * kEnv
            if kV > kOM[kd] then
              kOM[kd] = kV
              kOF[kd] = kRF[kRb + kj] + kSh * kBinHz
            endif
          endif
          kj += 1
        od
      endif
      kLeft[kS] = kLeft[kS] - 1
      kg += kB
    od
    kInit = 1
    kW += 1
    if kW >= iRing then
      kW = 0
    endif
    ; ---- output
    kk = 0
    while kk < iNb do
      kMag[kk] = kOM[kk]
      kFrq[kk] = (kOM[kk] > 0) ? kOF[kk] : kk * kBinHz
      kPM[kk] = kOM[kk]
      kPF[kk] = kFrq[kk]
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
