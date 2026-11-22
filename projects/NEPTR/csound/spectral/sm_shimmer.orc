#define SM_HAVE_SHIMMER ##
; SM_Shimmer - source: SoundMagic Spectral "Spectral Shimmer" (Michael Norris, AU SShr).
; His description: shimmer, jitter, sparkle, pulsation or sizzle via (a) per-bin amplitude jitter,
;   (b) a spectrum-wide amplitude pulse wave (square / sin), (c) per-bin delay variance, + Feedback.
; Order per frame: feedback mix -> per-bin delay (frame history) -> jitter -> pulse -> store for feedback.
; Jitter: a bin is picked with probability Jitter amount %, only above "Jitter only above" Hz and when its
;   magnitude is over the gate level; it gets a random gain uniform in +-J dB, J interpolated linearly
;   from Low-frequency jitter (0 Hz) to High-frequency jitter (Nyquist); held for Hold windows
;   (+-Hold var % of it, each bin on its own clock, so surges are staggered).
; Pulse: square = blocks of Pulse width bins alternating max/min dB, sin = same period (2*width bins) between
;   min and max; the pattern slides along the spectrum at Pulse rate Hz (sign = direction). Type 0 = off.
; Delay (frames, history capped by memory ~1M bins): type 1 Descent (delay falls with frequency), 2 Ascent,
;   3 LFO (sine across the spectrum, travelling at LFO rate), 4 Link amp (all bins delayed by input level),
;   5 Random (per bin per frame). Delay variance = max delay in frames. Delayed bins carry their own frequency.
; Deviations: his jitter distribution/delay units are unpublished (mine: +-uniform dB, delay in windows).
;   Feedback re-feeds the pre-SM_Common output of the previous frame, magnitudes soft-capped at 2.
opcode SM_Shimmer, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iH = max(2, min(256, int(1000000 / iNb)))
  kHM[] init iH * iNb
  kHF[] init iH * iNb
  kPM[] init iNb
  kPF[] init iNb
  kJG[] init iNb
  kJC[] init iNb
  kW init 0
  kInit init 0
  kLph init 0
  kPph init 0
  kAmpH init 0
  iHop = iN / 4
  iFr = iHop / sr
  if kNew == 1 then
    kAmpH = SM_FrameAmp(kMag)
    ; ---- params
    kJAmt = limit(tab:k($SMP_SHIMMER_JITTER_AMOUNT, giSMP), 0, 100) / 100
    kJMin = tab:k($SMP_SHIMMER_JITTER_ONLY_ABOVE, giSMP) / (sr / iN)
    kJLo = tab:k($SMP_SHIMMER_LOW_FREQUENCY_JITTER, giSMP)
    kJHi = tab:k($SMP_SHIMMER_HIGH_FREQUENCY_JITTER, giSMP)
    kJGate = ampdb(tab:k($SMP_SHIMMER_JITTER_GATE_LEVEL, giSMP))
    kJHold = max(1, tab:k($SMP_SHIMMER_JITTER_HOLD_WINDOWS, giSMP))
    kJVar = limit(tab:k($SMP_SHIMMER_JITTER_HOLD_VAR, giSMP), 0, 100) / 100
    kPT = int(tab:k($SMP_SHIMMER_AMPLITUDE_PULSE_TYPE, giSMP) + 0.5)
    kPMax = tab:k($SMP_SHIMMER_PULSE_LEVEL_MAX, giSMP)
    kPMin = tab:k($SMP_SHIMMER_PULSE_LEVEL_MIN, giSMP)
    kPWid = max(1, tab:k($SMP_SHIMMER_PULSE_WIDTH, giSMP))
    kPRate = tab:k($SMP_SHIMMER_PULSE_RATE, giSMP)
    kDT = int(tab:k($SMP_SHIMMER_DELAY_VARIANCE_TYPE, giSMP) + 0.5)
    kLRate = tab:k($SMP_SHIMMER_LFO_RATE_DELAY_VAR, giSMP)
    kDMax = limit(tab:k($SMP_SHIMMER_DELAY_VARIANCE, giSMP), 0, iH - 1)
    kFb = limit(tab:k($SMP_SHIMMER_FEEDBACK, giSMP), 0, 99) / 100
    kJ0 = kJLo
    kJS = (kJHi - kJLo) / iNb
    kLph = (kLph + kLRate * iFr) % 1
    kPph = (kPph + kPRate * iFr) % 1
    kPOn = (kPT == 1 || kPT == 2) ? 1 : 0
    kPSpan = 2 * kPWid
    kGMax = ampdb(kPMax)
    kGMin = ampdb(kPMin)
    kk = 0
    while kk < iNb do
      kM = kMag[kk]
      kF = kFrq[kk]
      ; ---- feedback
      if kFb > 0 then
        kM = min(kM + kFb * kPM[kk], 2)
      endif
      ; ---- history write
      kIx = kW * iNb + kk
      kHM[kIx] = kM
      kHF[kIx] = kF
      ; ---- per-bin delay
      if kDT >= 1 && kDMax > 0 then
        kFrac = kk / iNb
        kD = 0
        if kDT == 1 then
          kD = kDMax * (1 - kFrac)
        elseif kDT == 2 then
          kD = kDMax * kFrac
        elseif kDT == 3 then
          kD = kDMax * (0.5 + 0.5 * sin(6.2831853 * (kFrac - kLph)))
        elseif kDT == 4 then
          kD = kDMax * kAmpH
        elseif kDT == 5 then
          kD = kDMax * random:k(0, 1)
        endif
        kRi = (kW - int(kD) + iH) % iH
        kRx = kRi * iNb + kk
        kM = kHM[kRx]
        kF = kHF[kRx]
      endif
      ; ---- jitter
      if kJAmt > 0 then
        kJC[kk] = kJC[kk] - 1
        if kJC[kk] <= 0 then
          kJC[kk] = max(1, int(kJHold * (1 + kJVar * random:k(-1, 1))))
          if kInit == 0 then
            kJC[kk] = max(1, int(random:k(1, kJC[kk] + 1)))
          endif
          if random:k(0, 1) < kJAmt then
            kJG[kk] = ampdb(abs(kJ0 + kJS * kk) * random:k(-1, 1))
          else
            kJG[kk] = 1
          endif
        endif
        if kk >= kJMin && kM > kJGate then
          kM = kM * kJG[kk]
        endif
      endif
      ; ---- pulse
      if kPOn == 1 then
        kX = kk / kPSpan - kPph
        kX = kX - floor(kX)
        if kPT == 1 then
          kM = kM * ((kX < 0.5) ? kGMax : kGMin)
        else
          kM = kM * (kGMin + (kGMax - kGMin) * (0.5 + 0.5 * sin(6.2831853 * kX)))
        endif
      endif
      kM = min(kM, 4)
      kPM[kk] = kM
      kPF[kk] = kF
      kMag[kk] = kM
      kFrq[kk] = kF
      kk += 1
    od
    kInit = 1
    kW = (kW + 1) % iH
  endif
  xout kMag, kFrq
endop
