#define SM_HAVE_GLIDEFLT ##
; SM_GlidingFilters - source: SoundMagic Spectral "Spectral Gliding Filters" (Michael Norris, AU SGlf).
; His description: a series of spectral filters that glide in pitch. Each filter is born at
;   Start frequency +- variation, drifts Pitch glide (cents/s) +- var, lives Filter duration +- var,
;   then there is Silence duration +- var before a new filter is created in that slot.
; Params used: Number of filters, Filter width (bins), Start frequency (+var), Pitch glide (+var),
;   Filter duration (+var), Silence duration (+var), Wet dry mix.
; Process: up to 100 filter slots, each a Hann-shaped band-pass (total width = Filter width bins) on the
;   magnitudes centred at its current frequency; the frame's pass mask is the max over live filters
;   (no gain pile-up), out = mag * mask. Each filter fades in/out over min(40 % of life, 0.15 s) so
;   births/deaths do not click. Slots start in a random-length silence so they do not all fire at once.
;   A filter gliding past 20 Hz / Nyquist dies early. Frequencies are in Hz so FFT size does not move them.
;   Wet/Dry blends masked and original magnitudes. Frequencies of bins are left live.
; Deviations: fade length, Hann shape and uniform +-var randomisation are mine (unpublished).
opcode SM_GlidingFilters, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  iDt = (iN/4) / sr
  iMaxF = 100
  kCF[] init iMaxF
  kGl[] init iMaxF
  kRem[] init iMaxF
  kTot[] init iMaxF
  kSt[] init iMaxF
  kInit init 0
  kMask[] init iNb
  if kNew == 1 then
    kNum = int(limit(tab:k($SMP_GLIDEFLT_NUMBER_OF_FILTERS, giSMP), 1, iMaxF) + 0.5)
    kW = max(tab:k($SMP_GLIDEFLT_FILTER_WIDTH, giSMP), 1)
    kHW = max(kW * 0.5, 1)
    kSF = tab:k($SMP_GLIDEFLT_START_FREQUENCY, giSMP)
    kSFv = tab:k($SMP_GLIDEFLT_START_FREQUENCY_VARIATION, giSMP)
    kGc = tab:k($SMP_GLIDEFLT_PITCH_GLIDE_PER_SECOND, giSMP)
    kGv = tab:k($SMP_GLIDEFLT_PITCH_GLIDE_VAR, giSMP)
    kDu = tab:k($SMP_GLIDEFLT_FILTER_DURATION, giSMP)
    kDuv = tab:k($SMP_GLIDEFLT_FILTER_DURATION_VAR, giSMP)
    kSi = tab:k($SMP_GLIDEFLT_SILENCE_DURATION, giSMP)
    kSiv = tab:k($SMP_GLIDEFLT_SILENCE_DURATION_VAR, giSMP)
    kWet = limit(tab:k($SMP_GLIDEFLT_WET_DRY_MIX, giSMP), 0, 100) / 100
    kNyq = sr * 0.5
    if kInit == 0 then
      kInit = 1
      kj = 0
      while kj < iMaxF do
        kSt[kj] = 0
        kRem[kj] = random:k(0, max(kSi + kSiv, iDt))
        kj += 1
      od
    endif
    kk = 0
    while kk < iNb do
      kMask[kk] = 0
      kk += 1
    od
    kj = 0
    while kj < kNum do
      if kSt[kj] == 0 then
        kRem[kj] = kRem[kj] - iDt
        if kRem[kj] <= 0 then
          kf = kSF + kSFv * random:k(-1, 1)
          kCF[kj] = limit(kf, 20, kNyq * 0.999)
          kGl[kj] = kGc + kGv * random:k(-1, 1)
          kT = max(kDu + kDuv * random:k(-1, 1), 2 * iDt)
          kTot[kj] = kT
          kRem[kj] = kT
          kSt[kj] = 1
        endif
      endif
      if kSt[kj] == 1 then
        kRem[kj] = kRem[kj] - iDt
        kCF[kj] = kCF[kj] * pow(2, kGl[kj] / 1200 * iDt)
        if kRem[kj] <= 0 || kCF[kj] < 20 || kCF[kj] >= kNyq then
          kSt[kj] = 0
          kRem[kj] = max(kSi + kSiv * random:k(-1, 1), 0)
        else
          kFade = max(min(0.4 * kTot[kj], 0.15), 2 * iDt)
          kEnv = min(1, min((kTot[kj] - kRem[kj]) / kFade, kRem[kj] / kFade))
          kC = kCF[kj] / iBinHz
          kLo = max(int(kC - kHW), 0)
          kHi = min(int(kC + kHW) + 1, iNb - 1)
          kk = kLo
          while kk <= kHi do
            kd = abs(kk - kC)
            if kd < kHW then
              kv = kEnv * 0.5 * (1 + cos(3.14159265 * kd / kHW))
              if kv > kMask[kk] then
                kMask[kk] = kv
              endif
            endif
            kk += 1
          od
        endif
      endif
      kj += 1
    od
    kk = 0
    while kk < iNb do
      kMag[kk] = kMag[kk] * (1 - kWet + kWet * kMask[kk])
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
