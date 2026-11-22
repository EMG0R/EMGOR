#define SM_HAVE_AVERAGE ##
; SM_Averaging - source: SoundMagic Spectral "Spectral Averaging" (Michael Norris, AU SAve).
; His description: averages the spectral information over n windows (similar to Blurring).
; Params used: Number of frames (SMP_AVERAGE_NUMBER_OF_FRAMES, 2..1000). Brightness/cutoffs/
;   randomize/FFT/gain are SM_Common's.
; Deviations: PURE EXPONENTIAL mean, not a boxcar ring. Why: a ring of 200 (default) .. 1000
;   frames x N/2+1 bins is 8 MB per channel at N=2048 and impossible at 65536, and a ring
;   up to 64 + exponential tail would be two mechanisms for one job. Coefficient a = 2/(n+1)
;   gives the same mean lag as an n-frame boxcar ((n-1)/2 frames); the first n frames use a true
;   cumulative mean so startup is not pulled toward silence. Magnitudes are averaged; each bin's
;   frequency stays live.
opcode SM_Averaging, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  kAcc[] init iNb
  kCnt init 0
  if kNew == 1 then
    kn = tab:k($SMP_AVERAGE_NUMBER_OF_FRAMES, giSMP)
    kCnt = min(kCnt + 1, 100000)
    ka = max(2 / (kn + 1), 1 / kCnt)
    kk = 0
    while kk < iNb do
      kAcc[kk] = kAcc[kk] + ka * (kMag[kk] - kAcc[kk])
      kMag[kk] = kAcc[kk]
      kk += 1
    od
  endif
  xout kMag, kFrq
endop
