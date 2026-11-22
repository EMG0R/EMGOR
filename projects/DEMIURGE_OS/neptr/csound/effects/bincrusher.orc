; BINCRUSHER — Spectral Bin Selector
; ---------------------------------------------------------------------------
; Signal chain position : 122 (after BIN REDUCTION/121)
; Menu / slot           : SPECTRAL menu (11), slot 0 (toggle 44, CC72)
; Reads                 : gaBcSendL / gaBcSendR
; Writes                : gaBcWetL  / gaBcWetR   (100% wet when active)
; Controls              : expression pedal CC29 → gkBcPedal
;                         heel (0) = all 513 bins survive (transparent)
;                         toe  (1) = 1 bin survives (single dominant frequency)
;
; WHAT IT DOES
;   Divides the FFT spectrum into N equal-sized groups and keeps only the
;   single loudest bin in each group — all others are zeroed. The result is
;   resynthesis from a sparse set of isolated sinusoids.
;
;   At heel: 513 groups of 1 → all bins survive, transparent.
;   At mid:  ~23 survivors spread across the full spectrum.
;   At toe:  1 survivor (the global loudest bin), a single pure tone.
;
;   This is the "fewer bins" compression character — not averaging, not
;   filtering, just fewer distinct frequency components contributing to the
;   output. No envelope following, no balance opcode.
;
; GROUP SIZE CURVE
;   kGroup = pow(513, kPedal) → exponential, so the sweep feels gradual.
;   pedal=0.0 → group=1 (all bins)
;   pedal=0.5 → group≈23 (~22 survivors)
;   pedal=0.75→ group≈135 (~4 survivors)
;   pedal=1.0 → group=513 (1 survivor)
;
; ARCHIVED VERSION
;   The BinCoarsen averaging algorithm is kept in _retired/bincrusher_v1_coarsen.orc.
; ---------------------------------------------------------------------------

opcode BinSelect, k[], k[]k
  kIn[], kGroup xin
  iN = lenarray:i(kIn)
  kOut[] init iN
  ki = 0
  while ki < iN do
    kEnd = ki + kGroup
    if (kEnd > iN) then
      kEnd = iN
    endif
    ; find peak magnitude bin in this group
    kPeak    = 0
    kPeakIdx = ki
    kj = ki
    while kj < kEnd do
      if (kIn[kj] > kPeak) then
        kPeak    = kIn[kj]
        kPeakIdx = kj
      endif
      kj += 1
    od
    ; zero the entire group, then restore only the peak bin
    kj = ki
    while kj < kEnd do
      kOut[kj] = 0
      kj += 1
    od
    kOut[kPeakIdx] = kPeak
    ki = kEnd
  od
  xout kOut
endop

instr 122
  kPedal portk gkBcPedal, gkPortTime
  ; exponential group size: 1 at heel → giBinBins at toe
  kGroup = max(1, round(pow(giBinBins, kPedal)))

  aInL = gaBcSendL
  aInR = gaBcSendR

  ; ---- LEFT ----
  fInL    pvsanal   aInL, giBinFFT, giBinHop, giBinWin, 1
  kMagL[] init      giBinBins
  kFrqL[] init      giBinBins
  kflL    pvs2tab   kMagL, kFrqL, fInL
  kMagL   BinSelect kMagL, kGroup
  fOutL   tab2pvs   kMagL, kFrqL, giBinHop, giBinWin, 1
  aWetL   pvsynth   fOutL

  ; ---- RIGHT ----
  fInR    pvsanal   aInR, giBinFFT, giBinHop, giBinWin, 1
  kMagR[] init      giBinBins
  kFrqR[] init      giBinBins
  kflR    pvs2tab   kMagR, kFrqR, fInR
  kMagR   BinSelect kMagR, kGroup
  fOutR   tab2pvs   kMagR, kFrqR, giBinHop, giBinWin, 1
  aWetR   pvsynth   fOutR

  aWetL   dcblock   aWetL
  aWetR   dcblock   aWetR
  aWetL   limit     aWetL, -0.99, 0.99
  aWetR   limit     aWetR, -0.99, 0.99

  gaBcWetL = aWetL
  gaBcWetR = aWetR
endin
