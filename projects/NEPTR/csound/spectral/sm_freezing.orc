#define SM_HAVE_FREEZE ##
; SM_Freezing - source: SoundMagic Spectral "Spectral Freezing" (Michael Norris, AU SFrz).
; His description: each bin is watched until its amplitude peaks, then held at that peak until
;   exceeded by another peak; Freeze Factor puts a decay on the hold (100 % = no decay).
; Params used: Freeze factor, Use threshold param, Threshold (dB, 0 dB = full-scale sine),
;   Zero bins under threshold.
; Process: held = max(live, held*d) per bin, d = a^0.05 (a = factor/100; d=1 at 100 %, d=0 at 0 %
;   so 0 % is transparent). The frequency of the captured peak is held with it, so a frozen
;   partial keeps sounding at its own pitch. Threshold on: only bins above it may capture;
;   others show live unless Zero is on (then 0). 
; Deviations: decay-per-frame curve is mine (per frame, so time scale follows FFT size, as his).
opcode SM_Freezing, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  kH[] init iNb
  kHF[] init iNb
  ; Off -> on with the group still running (another effect at this FFT size kept
  ; it alive): this UDO was skipped meanwhile and kH still holds the OLD frozen
  ; spectrum, which at 100 % never decays = "stuck on". A gap longer than two
  ; frames between calls means we were off: drop the hold, start clean.
  kLastK init -1
  kNowK timeinstk
  if kNew == 1 then
    kStale = (kLastK >= 0 && kNowK - kLastK > 2 * (iN / 4) / ksmps) ? 1 : 0
    kLastK = kNowK
    ka = limit(tab:k($SMP_FREEZE_FREEZE_FACTOR, giSMP), 0, 100) / 100
    kd = (ka >= 1) ? 1 : pow(ka, 0.05)
    kUse = tab:k($SMP_FREEZE_USE_THRESHOLD_PARAM, giSMP)
    kThr = (kUse >= 0.5) ? ampdb(tab:k($SMP_FREEZE_THRESHOLD, giSMP)) : 0
    kZero = tab:k($SMP_FREEZE_ZERO_BINS_UNDER_THESHOLD, giSMP)
    kk = 0
    while kk < iNb do
      kM = kMag[kk]
      if kStale == 1 then
        kH[kk] = 0
      endif
      kDec = kH[kk] * kd
      if kDec < 0.000001 then
        kDec = 0
      endif
      if kM > 0 && kM >= kDec && kM >= kThr then
        kH[kk] = kM
        kHF[kk] = kFrq[kk]
      else
        kH[kk] = kDec
        if kDec > 0 then
          if kM <= kDec then
            kMag[kk] = kDec
            kFrq[kk] = kHF[kk]
          endif
        elseif kUse >= 0.5 && kZero >= 0.5 && kM < kThr then
          kMag[kk] = 0
        endif
      endif
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
