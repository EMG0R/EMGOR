#define SM_HAVE_PITCH ##
; SM_PitchShift - source: SoundMagic Spectral "Spectral Pitch Shift" (Michael Norris, AU SPSh).
; His description: pitch shift by Dolson/Laroche - spectral peaks are tracked and shifted
;   independently; optionally by a fixed number of bins; Rotate wraps the top around.
; Params used: Pitch shift (cents), Use 'Bins to Shift', Bins to Shift, Rotate, Feedback (%,
;   default 10). Only Gain is common (SM_Common).
; Process: SM_Peaks (fuzz 2 % of frame max, max 512 peaks) -> per region: peak frequency f,
;   shifted by df = f*(2^(cents/1200)-1) (or n*sr/N in bins mode), target bin round((f+df)/binHz);
;   the whole region moves by that integer bin offset, every bin's frequency += df (carried
;   coherently - this is where the Laroche phase-locking lives, since the analysis frame IS
;   amp+frequency here). Overlapping targets: the louder bin wins. Rotate wraps instead of
;   dropping. Exactly 0 cents / 0 bins is a bit-exact bypass. Feedback adds the previous output
;   magnitudes into the input (louder source's frequency wins), clamped at 8.
; Deviations: regions split at peak midpoints (not at spectral minima).
opcode SM_PitchShift, k[]k[], k[]k[]iki
  kMag[], kFrq[], ifx, kNew, iN xin
  iNb = iN/2 + 1
  iBinHz = sr / iN
  kPM[] init iNb
  kPF[] init iNb
  kOM[] init iNb
  kOF[] init iNb
  kPk[] init iNb + 1
  if kNew == 1 then
    kC = tab:k($SMP_PITCH_PITCH_SHIFT, giSMP)
    kUb = tab:k($SMP_PITCH_USE_BINS_TO_SHIFT_PARAMETER, giSMP)
    kBn = int(tab:k($SMP_PITCH_BINS_TO_SHIFT, giSMP))
    kRot = tab:k($SMP_PITCH_ROTATE, giSMP)
    kFb = limit(tab:k($SMP_PITCH_FEEDBACK, giSMP), 0, 99) / 100
    kRatio = pow(2, kC / 1200)
    if kFb > 0 then
      kk = 0
      while kk < iNb do
        kf = kFb * kPM[kk]
        if kf > kMag[kk] then
          kFrq[kk] = kPF[kk]
        endif
        kMag[kk] = min(kMag[kk] + kf, 8)
        kk += 1
      od
    endif
    kIdent = (kUb >= 0.5) ? ((kBn == 0) ? 1 : 0) : ((kC == 0) ? 1 : 0)
    if kIdent == 0 then
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
        kDf = (kUb >= 0.5) ? kBn * iBinHz : kf0 * (kRatio - 1)
        kd = round((kf0 + kDf) / iBinHz) - kp
        kk = kst
        while kk < ken do
          kt = kk + kd
          kw = 0
          if kt < 0 || kt >= iNb then
            kw = 1
            if kRot >= 0.5 then
              kt = kt % iNb
              if kt < 0 then
                kt += iNb
              endif
            else
              kt = -1
            endif
          endif
          if kt >= 0 && kMag[kk] > kOM[kt] then
            kOM[kt] = kMag[kk]
            if kw == 1 then
              kOF[kt] = max(0, kFrq[kk] + (kt - kk) * iBinHz)
            else
              kOF[kt] = max(0, kFrq[kk] + kDf)
            endif
          endif
          kk += 1
        od
        krg += 1
      od
      kMag = kOM
      kFrq = kOF
    endif
    kPM = kMag
    kPF = kFrq
  endif
  xout kMag, kFrq
endop
