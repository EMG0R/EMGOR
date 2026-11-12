#define SM_HAVE_BLUR ##
; SM_Blurring - source: SoundMagic Spectral "Spectral Blurring" (Michael Norris, AU SBlr).
; His description: low-pass filter on the spectral changes from one window to the next,
;   progressively smearing the sound.
; Params used: Blur amount (0..100 %), Variance type (0..4), Blur variance, LFO rate.
; Process: per-bin one-pole across frames, y = c*y + (1-c)*x, c = 1 - 10^(-3*amount/100)
;   (0 % -> c=0 transparent; 50 % -> 0.968; 100 % -> 0.999 = nearly static). Variance modulates the
;   amount (in % units) through SM_Variance, evaluated every k-cycle, sampled at the frame.
; Deviations: the exact mapping of his % to coefficient is unpublished; this curve is mine.
;   Bin frequencies stay live (only magnitudes are smoothed).
opcode SM_Blurring, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  kY[] init iNb
  kCnt init 0
  kAmpH init 0
  kAmt = SM_Variance(tab:k($SMP_BLUR_BLUR_AMOUNT, giSMP), tab:k($SMP_BLUR_VARIANCE_TYPE, giSMP), tab:k($SMP_BLUR_BLUR_VARIANCE, giSMP), tab:k($SMP_BLUR_LFO_RATE, giSMP), kAmpH)
  if kNew == 1 then
    kAmpH = SM_FrameAmp(kMag)
    kc = 1 - pow(10, -3 * limit(kAmt, 0, 100) / 100)
    if kAmt <= 0 then
      kc = 0
    endif
    kFirst = (kCnt == 0) ? 1 : 0
    kCnt = 1
    kk = 0
    while kk < iNb do
      if kFirst == 1 then
        kY[kk] = kMag[kk]
      else
        kY[kk] = kc * kY[kk] + (1 - kc) * kMag[kk]
      endif
      kMag[kk] = kY[kk]
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
