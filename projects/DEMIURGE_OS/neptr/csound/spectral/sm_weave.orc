#define SM_HAVE_WEAVE ##
; SM_Weave - source: SoundMagic Spectral "Spectral Weave" (Michael Norris, AU SWve).
; His description: wait until level exceeds Threshold, then until the sound is steady (level
;   varies < Max. stability dB for Min. stability length s); snapshot the whole spectrum (a
;   "thread") into one of up to Num. threads buffers; threads live Thread life s and are
;   faded in/out (weaved): Hold length +-var of sound, Silence length +-var of silence, repeat.
; Params: all 15 catalog params used (Lo/Hi/Randomize/Gain via SM_Common, FFT size by rack).
; Process: frame level = sqrt(sum mag^2) in dB (0 dB = full-scale sine). Stability = running
;   min/max of that level since the window restarted. Hold out: no new capture for Hold out s.
;   Capture stores mags + bin frequencies (optionally normalised to Normalize level dB).
;   Each thread: hold segment with trapezoid envelope (Fade % at each end; 50 % = triangle),
;   optional pulse (raised cosine, troughs at Pulse gain dB), then silence; lengths randomised.
;   Threads combine per bin by power sum; frequency from the strongest thread. Wet/dry mixes
;   with the input magnitudes (per bin, frequency from the louder side).
; Deviations: stability metric/hold-out details mine; new thread starts at the head of its hold;
;   if more threads than Num. threads, the oldest are dropped.
opcode SM_Weave, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iMaxT = 15
  kTM[] init iMaxT * iNb
  kTF[] init iMaxT * iNb
  kOn[] init iMaxT
  kAge[] init iMaxT
  kPos[] init iMaxT
  kSeg[] init iMaxT
  kSil[] init iMaxT
  kPph[] init iMaxT
  kLo init 0
  kHi init 0
  kCnt init 0
  kHold init 0
  kNext init 0
  idt = (iN/4) / sr
  if kNew == 1 then
    kThr = tab:k($SMP_WEAVE_THRESHOLD, giSMP)
    kNT = round(limit(tab:k($SMP_WEAVE_NUM_THREADS, giSMP), 1, iMaxT))
    kLife = tab:k($SMP_WEAVE_THREAD_LIFE, giSMP)
    kStab = tab:k($SMP_WEAVE_MAX_STABILITY, giSMP)
    kMinL = tab:k($SMP_WEAVE_MIN_STABILITY_LENGTH, giSMP)
    kHL = tab:k($SMP_WEAVE_HOLD_LENGTH, giSMP)
    kHV = tab:k($SMP_WEAVE_HOLD_LENGTH_VARIANCE, giSMP)
    kFd = limit(tab:k($SMP_WEAVE_FADE_IN_OUT, giSMP), 0, 50) / 100
    kSL = tab:k($SMP_WEAVE_SILENCE_LENGTH, giSMP)
    kSV = tab:k($SMP_WEAVE_SILENCE_LENGTH_VAR, giSMP)
    kHO = tab:k($SMP_WEAVE_HOLD_OUT_LENGTH, giSMP)
    kPF = tab:k($SMP_WEAVE_PULSE_FREQUENCY, giSMP)
    kPG = ampdb(tab:k($SMP_WEAVE_PULSE_GAIN, giSMP))
    kNorm = tab:k($SMP_WEAVE_NORMALIZE, giSMP)
    kNL = tab:k($SMP_WEAVE_NORMALIZE_LEVEL, giSMP)
    kWet = limit(tab:k($SMP_WEAVE_WET_DRY, giSMP), 0, 1)

    ; ---- level + steady-state detector
    kS = 0
    kk = 0
    while kk < iNb do
      kS += kMag[kk] * kMag[kk]
      kk += 1
    od
    kLev = sqrt(kS)
    kLdb = dbamp(max(kLev, 0.000001))
    kNext = max(kNext - idt, 0)
    if kLdb < kThr then
      kCnt = 0
      kLo = 0
      kHi = 0
    else
      if kCnt == 0 then
        kLo = kLdb
        kHi = kLdb
        kCnt = idt
      else
        kLo = min(kLo, kLdb)
        kHi = max(kHi, kLdb)
        if kHi - kLo > kStab then
          kLo = kLdb
          kHi = kLdb
          kCnt = idt
        else
          kCnt += idt
        endif
      endif
      if kCnt >= kMinL && kNext <= 0 then
        ; capture into a free slot, else the oldest
        kSlot = -1
        kt = 0
        while kt < iMaxT do
          if kOn[kt] == 0 && kSlot < 0 then
            kSlot = kt
          endif
          kt += 1
        od
        if kSlot < 0 then
          kOldA = -1
          kt = 0
          while kt < iMaxT do
            if kAge[kt] > kOldA then
              kOldA = kAge[kt]
              kSlot = kt
            endif
            kt += 1
          od
        endif
        kG = 1
        if kNorm >= 0.5 then
          kG = ampdb(kNL) / max(kLev, 0.000001)
        endif
        kb = kSlot * iNb
        kk = 0
        while kk < iNb do
          kTM[kb + kk] = kMag[kk] * kG
          kTF[kb + kk] = kFrq[kk]
          kk += 1
        od
        kOn[kSlot] = 1
        kAge[kSlot] = 0
        kPos[kSlot] = 0
        kSil[kSlot] = 0
        kPph[kSlot] = 0
        kSeg[kSlot] = max(kHL + kHV * random:k(-1, 1), idt)
        kNext = kHO
        kCnt = 0
        kLo = 0
        kHi = 0
      endif
    endif

    ; ---- age, expire, enforce Num. threads
    kAct = 0
    kt = 0
    while kt < iMaxT do
      if kOn[kt] == 1 then
        kAge[kt] = kAge[kt] + idt
        if kAge[kt] > kLife then
          kOn[kt] = 0
        else
          kAct += 1
        endif
      endif
      kt += 1
    od
    while kAct > kNT do
      kOldA = -1
      kOld = 0
      kt = 0
      while kt < iMaxT do
        if kOn[kt] == 1 && kAge[kt] > kOldA then
          kOldA = kAge[kt]
          kOld = kt
        endif
        kt += 1
      od
      kOn[kOld] = 0
      kAct -= 1
    od

    ; ---- mix threads
    kW[] init iNb
    kWF[] init iNb
    kBest[] init iNb
    kk = 0
    while kk < iNb do
      kW[kk] = 0
      kBest[kk] = 0
      kWF[kk] = kFrq[kk]
      kk += 1
    od
    kt = 0
    while kt < iMaxT do
      if kOn[kt] == 1 then
        kPos[kt] = kPos[kt] + idt
        kEnv = 0
        if kSil[kt] == 0 then
          if kPos[kt] >= kSeg[kt] then
            kSil[kt] = 1
            kPos[kt] = 0
            kSeg[kt] = max(kSL + kSV * random:k(-1, 1), 0)
          else
            ku = kPos[kt] / kSeg[kt]
            kEnv = 1
            if kFd > 0 then
              if ku < kFd then
                kEnv = ku / kFd
              elseif ku > 1 - kFd then
                kEnv = (1 - ku) / kFd
              endif
            endif
            if kPF > 0 then
              kPph[kt] = kPph[kt] + kPF * idt
              kPph[kt] = kPph[kt] - floor(kPph[kt])
              kEnv = kEnv * (kPG + (1 - kPG) * (0.5 + 0.5 * cos(6.283185307 * kPph[kt])))
            endif
          endif
        else
          if kPos[kt] >= kSeg[kt] then
            kSil[kt] = 0
            kPos[kt] = 0
            kPph[kt] = 0
            kSeg[kt] = max(kHL + kHV * random:k(-1, 1), idt)
          endif
        endif
        if kEnv > 0 then
          kb = kt * iNb
          kk = 0
          while kk < iNb do
            kM = kTM[kb + kk] * kEnv
            kW[kk] = kW[kk] + kM * kM
            if kM > kBest[kk] then
              kBest[kk] = kM
              kWF[kk] = kTF[kb + kk]
            endif
            kk += 1
          od
        endif
      endif
      kt += 1
    od

    ; ---- wet/dry
    kk = 0
    while kk < iNb do
      kWm = sqrt(kW[kk])
      kDm = kMag[kk]
      if kWm > kDm then
        kFrq[kk] = kWF[kk]
      endif
      kMag[kk] = kWet * kWm + (1 - kWet) * kDm
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
