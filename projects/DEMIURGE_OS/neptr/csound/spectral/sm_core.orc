; sm_core.orc - SoundMagic spectral rack: shared UDOs (SM_Common, SM_Variance, SM_Peaks, SM_P)
;
; IMPORTANT (measured on Csound 6.18): pvsanal ignores its iformat argument. The frame it
; produces is ALWAYS amp + TRUE FREQUENCY (Hz), so pvs2tab's second array is per-bin
; frequency, not phase. Everything here therefore treats the second array ("kFrq") as the
; bin's frequency; pvsynth accumulates phase from it. Consequences vs Norris's AU:
;   - Randomize phases  = per-frame random frequency offset of +-2 bins around the bin centre
;                         (phase increment uniform over a full circle per hop = random phase).
;   - kernels that move a bin by d bins also add d*sr/N to its frequency (carry it coherently).
; Magnitudes are amplitude-like: a full-scale sine peaks at about 1.0 in its bin.
;
; Only call per k-cycle; every heavy loop runs only when kNew == 1.

; per-effect "has" table: 5 slots per fx = absolute giSMP index of
; [Lo bin cutoff, Hi bin cutoff, Brightness, Randomize phases, Gain] or -1 (effect lacks it).
; Built from the generated #defines.
giSMhas ftgen 0, 0, -95, -2, \
  $SMP_AVERAGE_LO_BIN_CUTOFF, $SMP_AVERAGE_HI_BIN_CUTOFF, $SMP_AVERAGE_BRIGHTNESS, $SMP_AVERAGE_RANDOMIZE_PHASES, $SMP_AVERAGE_GAIN, \
  $SMP_BLUR_LO_BIN_CUTOFF, $SMP_BLUR_HI_BIN_CUTOFF, -1, $SMP_BLUR_RANDOMIZE_PHASES, $SMP_BLUR_GAIN, \
  $SMP_DRONE_LO_BIN_CUTOFF, $SMP_DRONE_HI_BIN_CUTOFF, $SMP_DRONE_BRIGHTNESS, $SMP_DRONE_RANDOMIZE_PHASES, $SMP_DRONE_GAIN, \
  $SMP_FREEZE_LO_BIN_CUTOFF, $SMP_FREEZE_HI_BIN_CUTOFF, -1, $SMP_FREEZE_RANDOMIZE_PHASES, $SMP_FREEZE_GAIN, \
  $SMP_GATEHOLD_LO_BIN_CUTOFF, $SMP_GATEHOLD_HI_BIN_CUTOFF, -1, $SMP_GATEHOLD_RANDOMIZE_PHASES, $SMP_GATEHOLD_GAIN, \
  $SMP_WEAVE_LO_BIN_CUTOFF, $SMP_WEAVE_HI_BIN_CUTOFF, -1, $SMP_WEAVE_RANDOMIZE_PHASES, $SMP_WEAVE_GAIN, \
  $SMP_SHIMMER_LO_BIN_CUTOFF, $SMP_SHIMMER_HI_BIN_CUTOFF, $SMP_SHIMMER_BRIGHTNESS, $SMP_SHIMMER_RANDOMIZE_PHASES, $SMP_SHIMMER_GAIN, \
  $SMP_SHUFFLE_LO_BIN_CUTOFF, $SMP_SHUFFLE_HI_BIN_CUTOFF, -1, $SMP_SHUFFLE_RANDOMIZE_PHASES, $SMP_SHUFFLE_GAIN, \
  -1, -1, $SMP_PULSING_BRIGHTNESS, -1, $SMP_PULSING_GAIN, \
  $SMP_HARMONIZ_LO_BIN_CUTOFF, $SMP_HARMONIZ_HI_BIN_CUTOFF, -1, $SMP_HARMONIZ_RANDOMIZE_PHASES, $SMP_HARMONIZ_GAIN, \
  $SMP_BINSHIFT_LO_BIN_CUTOFF, $SMP_BINSHIFT_HI_BIN_CUTOFF, $SMP_BINSHIFT_BRIGHTNESS, $SMP_BINSHIFT_RANDOMIZE_PHASES, $SMP_BINSHIFT_GAIN, \
  -1, -1, -1, -1, $SMP_PITCH_GAIN, \
  $SMP_STRETCH_LO_BIN_CUTOFF, $SMP_STRETCH_HI_BIN_CUTOFF, -1, $SMP_STRETCH_RANDOMIZE_PHASES, $SMP_STRETCH_GAIN, \
  $SMP_TRACING_LO_BIN_CUTOFF, $SMP_TRACING_HI_BIN_CUTOFF, -1, $SMP_TRACING_RANDOMIZE_PHASES, $SMP_TRACING_GAIN, \
  $SMP_FILTBANK_LO_BIN_CUTOFF, $SMP_FILTBANK_HI_BIN_CUTOFF, $SMP_FILTBANK_BRIGHTNESS, $SMP_FILTBANK_RANDOMIZE_PHASES, $SMP_FILTBANK_GAIN, \
  $SMP_GLIDEFLT_LO_BIN_CUTOFF, $SMP_GLIDEFLT_HI_BIN_CUTOFF, -1, $SMP_GLIDEFLT_RANDOMIZE_PHASES, $SMP_GLIDEFLT_GAIN, \
  $SMP_GRANUL_LO_BIN_CUTOFF, $SMP_GRANUL_HI_BIN_CUTOFF, -1, $SMP_GRANUL_RANDOMIZE_PHASES, $SMP_GRANUL_GAIN, \
  $SMP_EMERGE_LO_BIN_CUTOFF, $SMP_EMERGE_HI_BIN_CUTOFF, -1, $SMP_EMERGE_RANDOMIZE_PHASES, $SMP_EMERGE_GAIN, \
  $SMP_PARTGLD_LO_BIN_CUTOFF, $SMP_PARTGLD_HI_BIN_CUTOFF, -1, $SMP_PARTGLD_RANDOMIZE_PHASES, $SMP_PARTGLD_GAIN

#define SM_HAVE_CORE ##

; SM_P: k-rate param read. ifx = effect id, ip = param 0..31 (a generated absolute
; SMP_ define is also accepted: only its low 5 bits are used).
opcode SM_P, k, ii
  ifx, ip xin
  kv = tab:k(ifx*32 + (ip % 32), giSMP)
  xout kv
endop

; SM_Variance: kMod = kBase + modulation. VarType 0 None, 1 Random (uniform +-amt per call),
; 2 LFO (sine, kLfoRate Hz, +-amt), 3 Link amplitude (+amt*kInputAmp), 4 Link inverted amp
; (+amt*(1-kInputAmp)). Call EVERY k-cycle (the LFO needs continuous time).
opcode SM_Variance, k, kkkkk
  kBase, kType, kAmt, kRate, kAmp xin
  kLfo = lfo:k(1, max(kRate, 0.0001))
  kRnd = random:k(-1, 1)
  kMod = 0
  if kType == 1 then
    kMod = kAmt * kRnd
  elseif kType == 2 then
    kMod = kAmt * kLfo
  elseif kType == 3 then
    kMod = kAmt * kAmp
  elseif kType == 4 then
    kMod = kAmt * (1 - kAmp)
  endif
  xout kBase + kMod
endop

; SM_FrameAmp: normalised input frame level 0..1 (sqrt of summed squared magnitudes, clipped).
opcode SM_FrameAmp, k, k[]
  kMag[] xin
  kS = 0
  kk = 0
  while kk < lenarray(kMag) do
    kS += kMag[kk] * kMag[kk]
    kk += 1
  od
  xout min(sqrt(kS), 1)
endop

; SM_Peaks: Dolson/Laroche peak picking. Peaks = local maxima above kFuzz * frame max
; (kFuzz 0..1), at most kMaxBins of them. Regions of influence split at the midpoint
; between neighbouring peaks. Returns kPk[] = region start bins with kPk[kN] = nbins (sentinel),
; kN = number of regions. A region's peak is the argmax of its bins.
opcode SM_Peaks, k[]k, k[]kk
  kMag[], kFuzz, kMaxBins xin
  iNb = lenarray(kMag)
  kPk[] init iNb + 1
  kMx = 0
  kk = 0
  while kk < iNb do
    kMx = max(kMx, kMag[kk])
    kk += 1
  od
  kThr = kFuzz * kMx
  kN = 0
  kPrev = -1
  kk = 1
  while kk < iNb - 1 do
    if kMag[kk] > kThr && kMag[kk] > kMag[kk-1] && kMag[kk] >= kMag[kk+1] && kN < kMaxBins then
      if kN == 0 then
        kPk[0] = 0
      else
        kPk[kN] = int((kPrev + kk) / 2) + 1
      endif
      kN += 1
      kPrev = kk
    endif
    kk += 1
  od
  if kN > 0 then
    kPk[kN] = iNb
  endif
  xout kPk, kN
endop

; SM_Common: lo/hi cutoff, brightness tilt, randomize phases, gain - in that order,
; skipping params the effect lacks. Does nothing when kNew == 0. Neutral settings cost nothing.
opcode SM_Common, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  iLoI = table:i(ifx*5 + 0, giSMhas)
  iHiI = table:i(ifx*5 + 1, giSMhas)
  iBrI = table:i(ifx*5 + 2, giSMhas)
  iRnI = table:i(ifx*5 + 3, giSMhas)
  iGnI = table:i(ifx*5 + 4, giSMhas)
  if kNew == 1 then
    kLo = 0
    kHi = 100
    kBr = 0
    kRn = 0
    kGn = 0
    if iLoI >= 0 then
      kLo = tab:k(iLoI, giSMP)
    endif
    if iHiI >= 0 then
      kHi = tab:k(iHiI, giSMP)
    endif
    if iBrI >= 0 then
      kBr = tab:k(iBrI, giSMP)
    endif
    if iRnI >= 0 then
      kRn = tab:k(iRnI, giSMP)
    endif
    if iGnI >= 0 then
      kGn = tab:k(iGnI, giSMP)
    endif
    kLoB = int(kLo / 100 * iNb)
    kHiB = int(kHi / 100 * iNb)
    kGain = ampdb(kGn)
    kStep = 1
    if kBr != 0 then
      kStep = pow(10, kBr * 0.012 / iNb)
    endif
    if kLoB > 0 || kHiB < iNb || kBr != 0 || kRn >= 0.5 || kGn != 0 then
      kAcc = 1
      kk = 0
      while kk < iNb do
        if kk < kLoB || kk >= kHiB then
          kMag[kk] = 0
        else
          kMag[kk] = kMag[kk] * kAcc * kGain
        endif
        kAcc *= kStep
        if kRn >= 0.5 then
          kFrq[kk] = max(0, (kk + random:k(-2, 2))) * iBinHz
        endif
        kk += 1
      od
    endif
  endif
  xout kMag, kFrq
endop
