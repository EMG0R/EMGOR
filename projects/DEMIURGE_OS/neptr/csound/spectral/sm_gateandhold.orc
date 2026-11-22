#define SM_HAVE_GATEHOLD ##
; SM_GateAndHold - source: SoundMagic Spectral "Spectral Gate And Hold" (Michael Norris, AU SGth).
; His description: like Freezing, but a bin that crosses the threshold is frozen for a set time, with
;   an amplitude envelope (fade in, hold +- variance, fade out) and an optional decay over its life.
; Process: per-bin state machine, 0 idle/watching, 1 waiting (silence delay), 2 fade in, 3 hold, 4 fade out.
;   A trigger captures the bin's magnitude and frequency; the frozen partial then sounds at that pitch.
;   Gate Type: 0 all bins over threshold; 1 n loudest peaks; 2 n loudest peaks that also pass the
;   threshold; 3/4 = 1/2 but a new set is only chosen once every bin has finished (wait until done).
;   Peaks are local maxima; the peak and its two neighbours are selected together (Hann main lobe).
;   Threshold brightness tilts the threshold by -24 dB * b * (k/nbins) (b>0 lowers it up the spectrum).
;   Delay vs freq scales the onset delay by f^(2*pos-1) (f<1: high bins first, f>1: low bins first).
;   Onset delay = Silence length +- var (per bin) when Hold silence first is on; if off, only the
;   delay-vs-freq part (Silence length * (mult-1)) remains.
;   Decay factor a: amplitude *= max(a,1e-6)^(t/lifetime), 100 % = none, 0 % = -120 dB over life.
;   Pulsing: amp *= 0.5*(1+sin) at a per-bin rate (rate +- var) fixed over the bin's life.
;   Bin shift: held bin is written to k+shift (+-var, per bin, integer) with frequency moved by shift*sr/N.
;   Zero partials under threshold: non-held bins are silenced, else live signal passes.
;   Normalize held: captured magnitude is set to Normalize level (dB).
; Deviations: per-bin random uses an LCG (a k-rate random in a loop would repeat). Time is in frame hops.
opcode SM_GateAndHold, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iDt = (iN/4) / sr
  kSt[] init iNb
  kTm[] init iNb
  kDly[] init iNb
  kHld[] init iNb
  kHA[] init iNb
  kHF[] init iNb
  kLife[] init iNb
  kPR[] init iNb
  kPP[] init iNb
  kShf[] init iNb
  kSel[] init iNb
  kOM[] init iNb
  kOF[] init iNb
  kTV[] init 52
  kTI[] init 52
  kSd init 12345
  if kNew == 1 then
    kType = int(tab:k($SMP_GATEHOLD_GATE_TYPE, giSMP) + 0.5)
    kThrDb = tab:k($SMP_GATEHOLD_THRESHOLD, giSMP)
    kBr = tab:k($SMP_GATEHOLD_THRESHOLD_BRIGHTNESS, giSMP) / 100
    kNP = limit(int(tab:k($SMP_GATEHOLD_NUM_PARTIALS, giSMP) + 0.5), 0, 50)
    kFI = max(tab:k($SMP_GATEHOLD_FADE_IN_LENGTH, giSMP), 0)
    kHL = max(tab:k($SMP_GATEHOLD_HOLD_LENGTH, giSMP), 0)
    kHV = max(tab:k($SMP_GATEHOLD_HOLD_VARIANCE, giSMP), 0)
    kFO = max(tab:k($SMP_GATEHOLD_FADE_OUT_LENGTH, giSMP), 0)
    kSL = max(tab:k($SMP_GATEHOLD_SILENCE_LENGTH, giSMP), 0)
    kSV = max(tab:k($SMP_GATEHOLD_SILENCE_LENGTH_VAR, giSMP), 0)
    kDec = limit(tab:k($SMP_GATEHOLD_DECAY_FACTOR, giSMP), 0, 100) / 100
    kBS = tab:k($SMP_GATEHOLD_BIN_SHIFT, giSMP)
    kBSV = max(tab:k($SMP_GATEHOLD_BIN_SHIFT_VAR, giSMP), 0)
    kDvf = limit(tab:k($SMP_GATEHOLD_DELAY_VS_FREQ, giSMP), 1, 200) / 100
    kPRt = max(tab:k($SMP_GATEHOLD_PULSING_RATE, giSMP), 0)
    kPRv = max(tab:k($SMP_GATEHOLD_PULSING_RATE_VAR, giSMP), 0)
    kZero = tab:k($SMP_GATEHOLD_ZERO_PARTIALS_UNDER_THRESHOLD, giSMP)
    kNrm = tab:k($SMP_GATEHOLD_NORMALIZE_HELD_PARTIALS, giSMP)
    kNrmA = ampdb(tab:k($SMP_GATEHOLD_NORMALIZE_LEVEL, giSMP))
    kSil1 = tab:k($SMP_GATEHOLD_HOLD_SILENCE_FIRST, giSMP)
    ; ---- busy count
    kBusy = 0
    kk = 0
    while kk < iNb do
      if kSt[kk] > 0 then
        kBusy += 1
      endif
      kk += 1
    od
    ; ---- n-loudest peak selection (types 1..4)
    kUseN = (kType >= 1 && kNP >= 1 && (kType < 3 || kBusy == 0)) ? 1 : 0
    kk = 0
    while kk < iNb do
      kSel[kk] = 0
      kk += 1
    od
    if kUseN == 1 then
      kj = 0
      while kj < kNP do
        kTV[kj] = 0
        kTI[kj] = -1
        kj += 1
      od
      kk = 1
      while kk < iNb - 1 do
        kM = kMag[kk]
        if kM > kTV[max(kNP - 1,0)] && kM >= kMag[kk-1] && kM >= kMag[kk+1] then
          kj = kNP - 1
          while kj > 0 && kM > kTV[max(kj - 1,0)] do
            kTV[kj] = kTV[kj - 1]
            kTI[kj] = kTI[kj - 1]
            kj -= 1
          od
          kTV[kj] = kM
          kTI[kj] = kk
        endif
        kk += 1
      od
      kj = 0
      while kj < kNP do
        kb = kTI[kj]
        if kb >= 0 then
          kSel[kb - 1] = 1
          kSel[kb] = 1
          kSel[kb + 1] = 1
        endif
        kj += 1
      od
    endif
    ; ---- per-bin state machine
    kk = 0
    while kk < iNb do
      kOM[kk] = 0
      kOF[kk] = 0
      kk += 1
    od
    kEnable = (kType < 3 || kBusy == 0) ? 1 : 0
    kInvN = 1 / (iNb - 1)
    kk = 0
    while kk < iNb do
      kM = kMag[kk]
      kPos = kk * kInvN
      kS = kSt[kk]
      if kS == 0 then
        ; --- watching: trigger?
        kThr = ampdb(kThrDb - 24 * kBr * kPos)
        kGo = 0
        if kType == 0 then
          kGo = (kM >= kThr && kM > 0.000001) ? 1 : 0
        elseif kType == 1 || kType == 3 then
          kGo = (kSel[kk] == 1 && kM > 0.000001 && kEnable == 1) ? 1 : 0
        else
          kGo = (kSel[kk] == 1 && kM >= kThr && kM > 0.000001 && kEnable == 1) ? 1 : 0
        endif
        if kGo == 1 then
          kSd = (kSd * 1664525 + 1013904223) % 4294967296
          kR1 = kSd / 4294967296
          kSd = (kSd * 1664525 + 1013904223) % 4294967296
          kR2 = kSd / 4294967296
          kSd = (kSd * 1664525 + 1013904223) % 4294967296
          kR3 = kSd / 4294967296
          kSd = (kSd * 1664525 + 1013904223) % 4294967296
          kR4 = kSd / 4294967296
          kSd = (kSd * 1664525 + 1013904223) % 4294967296
          kR5 = kSd / 4294967296
          kMul = pow(kDvf, 2 * kPos - 1)
          if kSil1 >= 0.5 then
            kDly[kk] = max(kSL + kSV * (2 * kR1 - 1), 0) * kMul
          else
            kDly[kk] = max(kSL * (kMul - 1), 0)
          endif
          kHld[kk] = max(kHL + kHV * (2 * kR2 - 1), 0)
          kLife[kk] = max(kFI + kHld[kk] + kFO, 0.0001)
          kPR[kk] = max(kPRt + kPRv * (2 * kR3 - 1), 0)
          kPP[kk] = kR4
          kShf[kk] = kBS + kBSV * (2 * kR5 - 1)
          kHA[kk] = (kNrm >= 0.5) ? kNrmA : kM
          kHF[kk] = kFrq[kk]
          kTm[kk] = 0
          kSt[kk] = 1
          kS = 1
        endif
      endif
      if kS >= 1 then
        kTm[kk] = kTm[kk] + iDt
        kT = kTm[kk]
        kAmp = 0
        kOn = 0
        if kS == 1 then
          if kT >= kDly[kk] then
            kSt[kk] = 2
            kS = 2
            kTm[kk] = 0
            kT = 0
          endif
        endif
        if kS == 2 then
          if kT >= kFI then
            kSt[kk] = 3
            kS = 3
            kTm[kk] = kT - kFI
            kT = kTm[kk]
          endif
        endif
        if kS == 3 then
          if kT >= kHld[kk] then
            kSt[kk] = 4
            kS = 4
            kTm[kk] = kT - kHld[kk]
            kT = kTm[kk]
          endif
        endif
        if kS == 4 then
          if kT >= kFO then
            kSt[kk] = 0
            kS = 0
          endif
        endif
        if kS >= 2 then
          ; envelope
          if kS == 2 then
            kEnv = kT / max(kFI, 0.0001)
            kAge = kT
          elseif kS == 3 then
            kEnv = 1
            kAge = kFI + kT
          else
            kEnv = 1 - kT / max(kFO, 0.0001)
            kAge = kFI + kHld[kk] + kT
          endif
          kDg = (kDec >= 1) ? 1 : pow(max(kDec, 0.000001), min(kAge / kLife[kk], 1))
          kPul = 1
          if kPR[kk] > 0 then
            kPul = 0.5 * (1 + sin(6.283185307 * (kPP[kk] + kPR[kk] * kAge)))
          endif
          kAmp = kHA[kk] * limit(kEnv, 0, 1) * kDg * kPul
          kd = kk + int(kShf[kk] + (kShf[kk] >= 0 ? 0.5 : -0.5))
          if kd >= 0 && kd < iNb && kAmp > kOM[kd] then
            kOM[kd] = kAmp
            kOF[kd] = kHF[kk] + (kd - kk) * sr / iN
          endif
        endif
      endif
      kk += 1
    od
    ; ---- combine with live signal
    kk = 0
    while kk < iNb do
      if kSt[kk] < 2 && kZero < 0.5 && kOM[kk] <= 0 then
        kOM[kk] = kMag[kk]
        kOF[kk] = kFrq[kk]
      elseif kOM[kk] <= 0 then
        kOF[kk] = (kk + 0.0) * sr / iN
      endif
      kk += 1
    od
    kMag = kOM
    kFrq = kOF
  endif
  xout kMag, kFrq
endop
