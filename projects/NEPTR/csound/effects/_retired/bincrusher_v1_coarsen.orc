; BINCRUSHER — Spectral Bin Coarsener
; ---------------------------------------------------------------------------
; Signal chain position : 122 (after BIN REDUCTION/121)
; Menu / slot           : SPECTRAL menu (11), slot 4 (toggle 44, CC72)
; Reads                 : gaBcSendL / gaBcSendR
; Writes                : gaBcWetL  / gaBcWetR   (100% wet when active)
; Controls              : expression pedal CC29 → gkBcPedal
;                         heel (0) = 1-bin groups (no effect, transparent)
;                         toe  (1) = 64-bin groups (~8 spectral bands)
;
; WHAT IT DOES
;   Merges adjacent FFT bins by averaging their magnitudes. At heel the
;   group size is 1 (each bin is itself — no change). As the pedal moves
;   toward toe, groups grow exponentially: 1 → 2 → 4 → 8 → 16 → 32 → 64.
;   At 64 bins per group the 513-bin spectrum collapses to ~8 distinct
;   amplitude values — a staircase / blocky spectral shape that reads as
;   spectral decimation or "bit-crushed FFT". Pure, classic lo-fi spectral
;   effect with no pitch analysis or fundamental tracking.
;
; USES SAME FFT AS BIN REDUCTION
;   giBinFFT/giBinHop/giBinWin from globals.orc — reuses those tables
;   (FFT=1024, hop=256 → 46.9 Hz/bin resolves guitar harmonics).
;   Sharing the FFT params means both effects have the same spectral
;   resolution; they can run simultaneously without conflict.
; ---------------------------------------------------------------------------

opcode BinCoarsen, k[], k[]k
  kIn[], kStep xin
  iN     = lenarray:i(kIn)
  kOut[] init iN
  ki = 0
  while ki < iN do
    kEnd = ki + kStep
    if (kEnd > iN) then
      kEnd = iN
    endif
    ; average all bins in this group
    kSum = 0
    kj = ki
    while kj < kEnd do
      kSum += kIn[kj]
      kj += 1
    od
    kAvg = kSum / (kEnd - ki)
    ; write the average back to every bin in the group
    kj = ki
    while kj < kEnd do
      kOut[kj] = kAvg
      kj += 1
    od
    ki = kEnd
  od
  xout kOut
endop

instr 122
  kPedal  portk gkBcPedal, gkPortTime
  ; Group size: 1 at heel (transparent) → 64 at toe (8 spectral bands)
  kStep = max(1, round(pow(64, kPedal)))

  aInL = gaBcSendL
  aInR = gaBcSendR

  ; ---- LEFT ----
  fInL    pvsanal   aInL, giBinFFT, giBinHop, giBinWin, 1
  kMagL[] init      giBinBins
  kFrqL[] init      giBinBins
  kflL    pvs2tab   kMagL, kFrqL, fInL
  kMagL   BinCoarsen kMagL, kStep
  fOutL   tab2pvs   kMagL, kFrqL, giBinHop, giBinWin, 1
  aWetL   pvsynth   fOutL

  ; ---- RIGHT ----
  fInR    pvsanal   aInR, giBinFFT, giBinHop, giBinWin, 1
  kMagR[] init      giBinBins
  kFrqR[] init      giBinBins
  kflR    pvs2tab   kMagR, kFrqR, fInR
  kMagR   BinCoarsen kMagR, kStep
  fOutR   tab2pvs   kMagR, kFrqR, giBinHop, giBinWin, 1
  aWetR   pvsynth   fOutR

  aWetL   dcblock   aWetL
  aWetR   dcblock   aWetR
  aWetL   limit     aWetL, -0.99, 0.99
  aWetR   limit     aWetR, -0.99, 0.99

  gaBcWetL = aWetL
  gaBcWetR = aWetR
endin
