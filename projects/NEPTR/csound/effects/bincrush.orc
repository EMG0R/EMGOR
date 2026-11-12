; BINCRUSH — Spectral Contrast Quantizer
; ---------------------------------------------------------------------------
; Signal chain position : 121 (FILTERING cluster, right after FORMANT/120)
; Menu / slot           : FILTERING menu, slot 2 (toggle 38, CC76)
; Reads                 : gaBinSendL / gaBinSendR
; Writes                : gaBinWetL  / gaBinWetR   (100% wet — hard-assigned)
; Controls              : expression pedal CC29 → gkBinPedal
;                         heel (0) = extreme contrast (sinusoidal / crystallized)
;                         toe  (1) = near-transparent, then γ<1 boosts soft harmonics
;
; WHAT IT DOES
;   Applies spectral contrast exponentiation each FFT frame. Every magnitude
;   bin is raised to a power γ (gamma) controlled by the pedal. At γ=1
;   (heel) the spectrum is unchanged. As γ grows toward 16 (toe), louder
;   bins grow dominant while quieter bins collapse — the guitar progressively
;   reduces to a sparse constellation of its strongest harmonics, eventually
;   to a pure sine at its fundamental. Reads as "harmonic crystallization"
;   or lo-fi sinusoidal resynthesis. The sound is soft and ghostly, not harsh.
;
; WHY SPECTRAL CONTRAST (vs. "keep loudest N bins")
;   A loudest-N approach fails on guitar: all signal energy sits in a handful
;   of strong harmonic bins already, so keeping N=100 or N=2 preserves the
;   same bins — energy_kept ≈ 0.975 at every pedal position. Transparency.
;
;   Spectral contrast reshapes the SPECTRAL ENVELOPE, not the bin count.
;   Even with just 5 harmonics in 5 bins, γ=8 makes the 2nd harmonic
;   0.65^8 ≈ 3% of the fundamental — audibly gone. At γ=16 only the single
;   strongest partial survives: pure sinusoidal resynthesis.
;
; FFT PARAMETERS (giBinFFT / giBinHop / giBinWin in globals.orc)
;   FFT=1024, hop=256, win=1024 → 4× overlap, ~21 ms latency @48k.
;   This is intentionally larger than the original FFT=256 (~5.3 ms):
;   at 256 the bin width is 187.5 Hz — E2's first 5 harmonics (82–412 Hz)
;   all land in bins 0–2, making spectral manipulation invisible. At 1024
;   (46.9 Hz/bin) those harmonics fall in bins 2, 4, 5, 7, 9 — separately
;   resolvable and independently crushable. Same FFT/hop as Spectral Delay.
;   hop=256 = 2× ksmps (128) so pvsanal is happy.
;
; LO-FI / SOFT CHARACTER
;   * Peak-relative normalization: magnitude is divided by the frame peak
;     before exponentiation and scaled back after. The loudest bin stays at
;     its original amplitude; everything else collapses relative to it.
;     This preserves musical dynamics and prevents over-amplification.
;   * pvsmooth 0.35/0.35 after resynthesis — 35% frame-bleed smooths
;     abrupt spectral changes, preventing "birdie" warble as harmonics
;     drift between bins. This is the primary source of the smeared/dreamy
;     lo-fi texture.
;   * balance vs. the dry send keeps perceived loudness constant as the
;     spectrum collapses (fewer active bins = less total energy without it).
;   * dcblock (1-pole, not dcblock2) avoids the 254-sample FIR latency
;     that dcblock2 would add.
; ---------------------------------------------------------------------------

; --- SpectralContrast UDO -------------------------------------------------
; Peak-relative magnitude contrast exponentiation.
; The loudest bin stays at its original amplitude; each other bin becomes
; (mag/peak)^gamma * peak. With gamma > 1, louder bins dominate more and
; more; quieter bins decay toward zero. At gamma=16, all but the single
; loudest bin are effectively silent → pure sinusoidal resynthesis.
opcode SpectralContrast, k[], k[]k
  kIn[], kGamma xin
  iN      = lenarray:i(kIn)
  kOut[]  init iN

  ; find the spectral peak for this frame
  kPeak = 0
  ki = 0
  while ki < iN do
    kPeak = (kIn[ki] > kPeak ? kIn[ki] : kPeak)
    ki += 1
  od
  kPeak = (kPeak < 1e-9 ? 1e-9 : kPeak)   ; silence guard — avoids div/0

  ; normalize → exponentiate → restore peak level, per bin
  ki = 0
  while ki < iN do
    kNorm      = kIn[ki] / kPeak
    kOut[ki]   = pow(kNorm, kGamma) * kPeak
    ki += 1
  od
  xout kOut
endop

instr 121
  ; Pedal → gamma exponent (polarity inverted: heel=crush, toe=emphasis).
  ; kGamma = pow(32, 1-kPedal) * 0.5
  ;   heel (kPedal=0) → γ=16   → extreme — single sinusoidal trace
  ;   kPedal=0.25     → γ≈5.7  → strong — essentially fundamental only
  ;   kPedal=0.5      → γ≈2    → mild contrast, harmonics thinning
  ;   kPedal=0.75     → γ≈0.7  → subtle lifting of softer harmonics
  ;   toe  (kPedal=1) → γ=0.5  → emphasis (sqrt) — boosts soft harmonics
  kPedal   portk gkBinPedal, gkPortTime
  kGamma   = pow(32, 1 - kPedal) * 0.5

  aInL = gaBinSendL
  aInR = gaBinSendR

  ; ---- LEFT ----
  fInL     pvsanal   aInL, giBinFFT, giBinHop, giBinWin, 1
  kMagL[]  init      giBinBins
  kFrqL[]  init      giBinBins
  kflL     pvs2tab   kMagL, kFrqL, fInL
  kMagL    SpectralContrast kMagL, kGamma
  fOutL    tab2pvs   kMagL, kFrqL, giBinHop, giBinWin, 1
  fSmL     pvsmooth  fOutL, 0.35, 0.35
  aWetL    pvsynth   fSmL

  ; ---- RIGHT ----
  fInR     pvsanal   aInR, giBinFFT, giBinHop, giBinWin, 1
  kMagR[]  init      giBinBins
  kFrqR[]  init      giBinBins
  kflR     pvs2tab   kMagR, kFrqR, fInR
  kMagR    SpectralContrast kMagR, kGamma
  fOutR    tab2pvs   kMagR, kFrqR, giBinHop, giBinWin, 1
  fSmR     pvsmooth  fOutR, 0.35, 0.35
  aWetR    pvsynth   fSmR

  ; keep perceived loudness constant as the spectral envelope collapses
  aWetL    balance   aWetL, aInL
  aWetR    balance   aWetR, aInR

  ; 1-pole DC block + safety limiter (dcblock NOT dcblock2 — see header)
  aWetL    dcblock   aWetL
  aWetR    dcblock   aWetR
  aWetL    limit     aWetL, -0.99, 0.99
  aWetR    limit     aWetR, -0.99, 0.99

  gaBinWetL = aWetL
  gaBinWetR = aWetR
endin
