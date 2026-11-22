#define SM_HAVE_STRETCH ##
; SM_Stretch - source: SoundMagic Spectral "Spectral Stretch" (Michael Norris, AU SSch).
; His description: spectral peaks are tracked and shifted by the quadratic beta*w + alpha*w^2,
;   w = frequency of each bin. Beta can vary (Random / LFO / Link amp / Link inv amp) and be
;   quantized so the pitch steps (often a harmonic series).
; Params used: Alpha stretch, Beta stretch, Variance type, Beta stretch variance,
;   Variance quantization, LFO rate. (Lo/Hi/Randomize/Gain via SM_Common.)
; Process: SM_Peaks (fuzz 2 % of frame max, max 512 peaks) -> per region peak frequency f
;   -> f' = beta*f + alpha*f*(f/Nyquist); the region moves by round((f'-f)/binHz) bins and every
;   bin's frequency gains (f'-f) (coherent partial carry). Louder bin wins on overlaps; targets
;   past Nyquist drop. Beta is varied per frame via SM_Variance (amount in beta units, floor 0.05),
;   then quantized to steps of q/100 when q > 0 and a variance type is active.
; Deviations: his w units are unpublished; I normalise w to f/Nyquist so alpha 2 is a clear but
;   musical inharmonic stretch (1 kHz -> ~1.09 kHz, 5 kHz -> ~7.3 kHz at beta 1).
opcode SM_Stretch, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  iNyq = sr / 2
  kOM[] init iNb
  kOF[] init iNb
  kPk[] init iNb + 1
  kAmpH init 0
  kVt = tab:k($SMP_STRETCH_VARIANCE_TYPE, giSMP)
  kBeta = SM_Variance(tab:k($SMP_STRETCH_BETA_STRETCH, giSMP), kVt, tab:k($SMP_STRETCH_BETA_STRETCH_VARIANCE, giSMP), tab:k($SMP_STRETCH_LFO_RATE, giSMP), kAmpH)
  if kNew == 1 then
    kAmpH = SM_FrameAmp(kMag)
    kAlpha = tab:k($SMP_STRETCH_ALPHA_STRETCH, giSMP)
    kQ = tab:k($SMP_STRETCH_VARIANCE_QUANTIZATION, giSMP) / 100
    if kVt >= 0.5 && kQ > 0 then
      kBeta = round(kBeta / kQ) * kQ
    endif
    kBeta = max(kBeta, 0.05)
    kk = 0
    while kk < iNb do
      kOM[kk] = 0
      kOF[kk] = kk * iBinHz
      kk += 1
    od
    kPk, kN SM_Peaks kMag, 0.02, 512
    krg = 0
    while krg < kN do
      kst = kPk[krg]
      ken = kPk[krg + 1]
      kp = kst
      kb = 0
      kk = kst
      while kk < ken do
        if kMag[kk] > kb then
          kb = kMag[kk]
          kp = kk
        endif
        kk += 1
      od
      kf0 = (kFrq[kp] > 0) ? kFrq[kp] : kp * iBinHz
      kDf = kf0 * (kBeta - 1) + kAlpha * kf0 * kf0 / iNyq
      kd = round((kf0 + kDf) / iBinHz) - kp
      kk = kst
      while kk < ken do
        kt = kk + kd
        if kt >= 0 then
          if kt < iNb then
            if kMag[kk] > kOM[kt] then
              kOM[kt] = kMag[kk]
              kOF[kt] = max(0, kFrq[kk] + kDf)
            endif
          endif
        endif
        kk += 1
      od
      krg += 1
    od
    kMag = kOM
    kFrq = kOF
  endif
  xout kMag, kFrq
endop
