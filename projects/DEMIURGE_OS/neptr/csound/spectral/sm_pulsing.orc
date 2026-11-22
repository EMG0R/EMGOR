#define SM_HAVE_PULSING ##
; SM_Pulsing - source: SoundMagic Spectral "Spectral Pulsing" (Michael Norris, AU SPsg).
; His description: switches bins on and off across the spectrum in a pulsating fashion.
;   On/Off duration for bin zero; "multiplier freq" scales each duration as you go UP the spectrum;
;   "multiplier time" scales it each time that bin completes a cycle; min/max clamp the durations;
;   Modulo type: 1 = ping-pong between min and max, 2 = wrap round max->min / min->max.
; Process: per-bin state machine (on/off countdown in seconds, one frame = (N/4)/sr). Off = magnitude 0
;   (its frequency is left alone). Effective duration = base * freqMul^(8*k/nbins) * timeFactor[bin],
;   then folded into [min,max] by the Modulo type.
; Deviations: the exact freq-multiplier law is unpublished; mine compounds the multiplier every 1/8 of the
;   spectrum (1.1 -> x2.1 at Nyquist, 1.2 -> x4.3). Time multiplier compounds per bin cycle (on+off pair
;   uses the on factor for on, off factor for off) and the factor is clamped to 1e-3..1e3.
;   If min > max they are swapped; max==min gives a fixed duration. No Wet/Dry or Feedback in this effect.
opcode SM_PulseFold, k, kkkk
  kx, klo, khi, kType xin
  kR = khi - klo
  if kR <= 0.000001 then
    kx = klo
  else
    if kType < 1.5 then
      kM = 2 * kR
      kp = (kx - klo) - floor((kx - klo) / kM) * kM
      kx = klo + ((kp > kR) ? kM - kp : kp)
    else
      kx = klo + ((kx - klo) - floor((kx - klo) / kR) * kR)
    endif
  endif
  xout kx
endop

opcode SM_Pulsing, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  idt = (iN/4) / sr
  kRem[] init iNb
  kOn[] init iNb
  kFOn[] init iNb
  kFOff[] init iNb
  kInit init 0
  if kNew == 1 then
    kOnD = tab:k($SMP_PULSING_ON_DURATION, giSMP)
    kOffD = tab:k($SMP_PULSING_OFF_DURATION, giSMP)
    kOnMF = tab:k($SMP_PULSING_ON_MULTIPLIER_FREQ, giSMP)
    kOffMF = tab:k($SMP_PULSING_OFF_MULTIPLIER_FREQ, giSMP)
    kOnMT = tab:k($SMP_PULSING_ON_MULTIPLIER_TIME, giSMP)
    kOffMT = tab:k($SMP_PULSING_OFF_MULTIPLIER_TIME, giSMP)
    kOnLo = tab:k($SMP_PULSING_MIN_ON_DURATION, giSMP)
    kOnHi = tab:k($SMP_PULSING_MAX_ON_DURATION, giSMP)
    kOffLo = tab:k($SMP_PULSING_MIN_OFF_DURATION, giSMP)
    kOffHi = tab:k($SMP_PULSING_MAX_OFF_DURATION, giSMP)
    kMod = tab:k($SMP_PULSING_MODULO_TYPE, giSMP)
    if kOnLo > kOnHi then
      kt = kOnLo
      kOnLo = kOnHi
      kOnHi = kt
    endif
    if kOffLo > kOffHi then
      kt = kOffLo
      kOffLo = kOffHi
      kOffHi = kt
    endif
    kFirst = (kInit == 0) ? 1 : 0
    kInit = 1
    kinv = 8 / iNb
    kk = 0
    while kk < iNb do
      if kFirst == 1 then
        kFOn[kk] = 1
        kFOff[kk] = 1
        kOn[kk] = 1
        kRem[kk] = SM_PulseFold(kOnD * pow(kOnMF, kk * kinv), kOnLo, kOnHi, kMod)
      endif
      kRem[kk] = kRem[kk] - idt
      if kRem[kk] <= 0 then
        if kOn[kk] == 1 then
          kOn[kk] = 0
          kFOff[kk] = limit(kFOff[kk] * kOffMT, 0.001, 1000)
          kd = SM_PulseFold(kOffD * pow(kOffMF, kk * kinv) * kFOff[kk], kOffLo, kOffHi, kMod)
        else
          kOn[kk] = 1
          kFOn[kk] = limit(kFOn[kk] * kOnMT, 0.001, 1000)
          kd = SM_PulseFold(kOnD * pow(kOnMF, kk * kinv) * kFOn[kk], kOnLo, kOnHi, kMod)
        endif
        kRem[kk] = kRem[kk] + max(kd, idt)
        if kRem[kk] <= 0 then
          kRem[kk] = idt
        endif
      endif
      if kOn[kk] == 0 then
        kMag[kk] = 0
      endif
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
