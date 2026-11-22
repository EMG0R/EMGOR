#define SM_HAVE_DRONE ##
; SM_DroneMaker - source: SoundMagic Spectral "Spectral DroneMaker" (Michael Norris, AU SDmk).
; His description: drone made by gradually interpolating between frequency components
;   sampled at a distance; optional comb filterbank before the interpolation, gate as in
;   Spectral Tracing, and a flanger on the output.
; Params used: Interpolation length (s), Interp length variance (s), Use peak amplitudes, Comb
;   filterbank level/fundamental/Scale type/Number of octaves/Filter resonance, Flanger
;   amount/depth/rate, Gate level. (Brightness, cutoffs, Randomize, Gain are applied by SM_Common.)
; Process: every bin runs its own segment A -> B: output = (1-p)*A + p*B (magnitude), p advances
;   by hop/length per frame. At p>=1, A=B and B is re-sampled from the live (comb-filtered) frame,
;   with a fresh length T + rand(+-variance). Segment phases start staggered so bins never move
;   in lock-step (smooth, no frame-wide pulse). Peak mode: B = the largest amplitude seen in the bin
;   during the segment (that peak's frequency kept with it). Bin frequency = amplitude-weighted
;   mix of A/B frequencies.
; Comb: per note (scale degrees x octaves above the fundamental) a comb-filter magnitude response
;   (peaks at every harmonic, half-width f*(1-res)/2pi, floor (1-res)/(1+res), min width 0.8 bin
;   so peaks cannot fall between bins); bank = max over notes; mix = (1-level)+level*G. Built
;   incrementally (a work budget of notes per frame) whenever its params change.
;   Scale 0 (None) = comb bypassed.
; Flanger: spectral flanger, mag *= (1-a)+a*|cos(pi f d)| with delay d swept 0..depth by an LFO.
;   Deviation: no pitch-warble of a true delay-line flanger (frame domain), only the moving notches.
; Gate: output bins below ampdb(level) zeroed; -60 dB (the minimum) = off.
opcode SM_DroneMaker, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  iDt = (iN/4) / sr
  iNyq = sr / 2
  kA[] init iNb
  kB[] init iNb
  kFA[] init iNb
  kFB[] init iNb
  kPk[] init iNb
  kPkF[] init iNb
  kPh[] init iNb
  kLen[] init iNb
  kG[] init iNb
  kGb[] init iNb
  kInit init 0
  kGvalid init 0
  kPend init 0
  kNote init 0
  kTot init 0
  kS0 init -1
  kS1 init -1
  kS2 init -1
  kS3 init -1
  kFlP init random:i(0, 1)
  ; scale degrees in semitones, 7 slots per row, -1 pad; row = Scale type
  iScl[] fillarray 0,-1,-1,-1,-1,-1,-1, 0,-1,-1,-1,-1,-1,-1, 0,7,-1,-1,-1,-1,-1, 0,4,7,-1,-1,-1,-1, 0,3,7,-1,-1,-1,-1, 0,2,4,5,7,9,11, 0,2,3,5,7,9,10, 0,2,4,6,7,9,11, 0,2,4,5,7,9,10, 0,2,3,5,7,8,10
  iCnt[] fillarray 1, 1, 2, 3, 3, 7, 7, 7, 7, 7
  if kNew == 1 then
    kT = tab:k($SMP_DRONE_INTERPOLATION_LENGTH, giSMP)
    kV = tab:k($SMP_DRONE_INTERP_LENGTH_VARIANCE, giSMP)
    kPeakOn = tab:k($SMP_DRONE_USE_PEAK_AMPLITUDES, giSMP)
    kCL = limit(tab:k($SMP_DRONE_COMB_FILTERBANK_LEVEL, giSMP), 0, 100) / 100
    kFund = tab:k($SMP_DRONE_COMB_FILTER_FUNDAMENTAL, giSMP)
    kScl = limit(int(tab:k($SMP_DRONE_SCALE_TYPE, giSMP) + 0.5), 0, 9)
    kOct = limit(int(tab:k($SMP_DRONE_NUMBER_OF_OCTAVES, giSMP) + 0.5), 1, 10)
    kRes = limit(tab:k($SMP_DRONE_FILTER_RESONANCE, giSMP), 1, 99) / 100
    kFlA = limit(tab:k($SMP_DRONE_FLANGER_AMOUNT, giSMP), 0, 100) / 100
    kFlD = max(tab:k($SMP_DRONE_FLANGER_DEPTH, giSMP), 0.01) * 0.001
    kFlR = tab:k($SMP_DRONE_FLANGER_RATE, giSMP)
    kGate = tab:k($SMP_DRONE_GATE_LEVEL, giSMP)
    kGateA = (kGate > -59.9) ? ampdb(kGate) : 0
    kLo = max(kT - kV, 0.05)
    kHi = max(kT + kV, kLo)
    ; ---- comb bank (incremental rebuild)
    kUseComb = (kCL > 0 && kScl > 0) ? 1 : 0
    if kUseComb == 1 then
      if kFund != kS0 || kScl != kS1 || kOct != kS2 || kRes != kS3 then
        kS0 = kFund
        kS1 = kScl
        kS2 = kOct
        kS3 = kRes
        kNote = 0
        kPend = 1
        kTot = kOct * iCnt[kScl]
        kq = 0
        kFl = (1 - kRes) / (1 + kRes)
        while kq < iNb do
          kGb[kq] = kFl
          kq += 1
        od
      endif
      kOps = 0
      kFl = (1 - kRes) / (1 + kRes)
      while kNote < kTot && kOps < 40000 do
        kNO = int(kNote / iCnt[kScl])
        kNS = kNote - kNO * iCnt[kScl]
        kF = kFund * pow(2, kNO + iScl[kScl * 7 + kNS] / 12)
        if kF < iNyq && kF > 1 then
          kHw = max(kF * (1 - kRes) / 6.2832, 0.8 * iBinHz)
          kSpan = min(min(8 * kHw, kF * 0.5), 64 * iBinHz)
          kSpB = int(kSpan / iBinHz) + 1
          kHm = kF
          while kHm < iNyq do
            kC = int(kHm / iBinHz + 0.5)
            kj = max(kC - kSpB, 0)
            kjE = min(kC + kSpB, iNb - 1)
            while kj <= kjE do
              kd = (kj * iBinHz - kHm) / kHw
              kg = 1 / sqrt(1 + kd * kd)
              if kg > kGb[kj] then
                kGb[kj] = kg
              endif
              kj += 1
              kOps += 1
            od
            kHm += kF
          od
        endif
        kNote += 1
      od
      if kNote >= kTot && kPend == 1 then
        kq = 0
        while kq < iNb do
          kG[kq] = kGb[kq]
          kq += 1
        od
        kPend = 0
        kGvalid = 2
      endif
    endif
    ; ---- flanger phase
    kFlP += iDt * kFlR
    kFlP = kFlP - int(kFlP)
    kFd = kFlD * (0.5 + 0.5 * sin(6.2832 * kFlP)) + 0.00001
    ; ---- per-bin interpolation
    kk = 0
    while kk < iNb do
      kM = kMag[kk]
      if kUseComb == 1 && kGvalid == 2 then
        kM = kM * ((1 - kCL) + kCL * kG[kk])
      endif
      kF0 = kFrq[kk]
      if kInit == 0 then
        kA[kk] = kM
        kB[kk] = kM
        kFA[kk] = kF0
        kFB[kk] = kF0
        kPk[kk] = kM
        kPkF[kk] = kF0
        kLen[kk] = random:k(kLo, kHi)
        kPh[kk] = random:k(0, 1)
      else
        if kM >= kPk[kk] then
          kPk[kk] = kM
          kPkF[kk] = kF0
        endif
        kPh[kk] = kPh[kk] + iDt / kLen[kk]
        if kPh[kk] >= 1 then
          kA[kk] = kB[kk]
          kFA[kk] = kFB[kk]
          if kPeakOn >= 0.5 then
            kB[kk] = kPk[kk]
            kFB[kk] = kPkF[kk]
          else
            kB[kk] = kM
            kFB[kk] = kF0
          endif
          kPk[kk] = kM
          kPkF[kk] = kF0
          kPh[kk] = 0
          kLen[kk] = random:k(kLo, kHi)
        endif
      endif
      kp = kPh[kk]
      kwA = (1 - kp) * kA[kk]
      kwB = kp * kB[kk]
      kO = kwA + kwB
      if kO > 0 then
        kFo = (kwA * kFA[kk] + kwB * kFB[kk]) / kO
      else
        kFo = kk * iBinHz
      endif
      if kO < kGateA then
        kO = 0
      endif
      if kFlA > 0 then
        kO = kO * ((1 - kFlA) + kFlA * abs(cos(3.14159265 * kk * iBinHz * kFd)))
      endif
      kMag[kk] = kO
      kFrq[kk] = kFo
      kk += 1
    od
    kInit = 1
  endif
  xout kMag, kFrq
endop
