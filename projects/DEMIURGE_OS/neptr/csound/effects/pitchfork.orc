; Pitch Fork clone — CHORD KING phase vocoder, FFT=2048 hop=512 (4x overlap)
; Signal chain position: 49
; Reads: gaPFSendL/R
; Writes: gaPFOutL/R
; Bypass: gkBypassPF
;
; v5 (2026-07-03) — CHORD KING rebuild. Research: Laroche-Dolson phase
; locking + Signalsmith Stretch design notes. Why v3/v4 garbled chords:
;   1. FFT 512 @48k = 93.75 Hz bins. Low E is 82 Hz — whole chord voicings
;      shared analysis bins and intermodulated. FFT 2048 = 23.4 Hz bins.
;   2. hop 256 at FFT 512 was only 2x overlap; phase vocoder resynthesis
;      wants 4-8x. Same hop at FFT 2048 = 8x overlap. (hop stays 256 =
;      2*ksmps, the alignment that fixed the old engage crash.)
;   3. pvslock restored. v4 removed it citing "monophonic-only" — wrong:
;      Laroche-Dolson peak locking is REGION-based (each spectral peak
;      locks its own neighborhood) and was designed for polyphonic
;      material. Without it: classic phasey underwater smear.
;   4. tanh soft-clip dropped — on hot chords it added intermodulation
;      distortion. Gentle -2 dB shelf above 6 kHz kept for warmth.
;
; Latency: window 2048 + hop 512 = 2560 samples ≈ 53 ms on the wet path
; (dry path unaffected). The harmony trails like a lush doubler — accepted
; tradeoff per user (chords > speed).
;
; QUANTIZED INTERVALS (unchanged): gkSemiPF snapped to nearest of
; {-12, -9, -7, -5, +5, +7, +9, +12}. Harmony-only, no detune mode.
;
; Defaults: gkSemiPF=5 (perfect 5th up), gkPFmix=0.5 (50% wet).

instr 49
    aIn = (gaPFSendL + gaPFSendR) * 0.5
    aIn dcblock2 aIn

    ; Clamp then quantize semitones to allowed set
    kSemiPFC limit gkSemiPF, gkPF_min, gkPF_max
    if (kSemiPFC < -10.5) then
      kSemi = -12
    elseif (kSemiPFC < -8.0) then
      kSemi = -9
    elseif (kSemiPFC < -6.0) then
      kSemi = -7
    elseif (kSemiPFC < 0.0) then
      kSemi = -5
    elseif (kSemiPFC < 6.0) then
      kSemi = 5
    elseif (kSemiPFC < 8.0) then
      kSemi = 7
    elseif (kSemiPFC < 10.5) then
      kSemi = 9
    else
      kSemi = 12
    endif
    kRatio = pow(2, kSemi / 12)
    kRatio portk kRatio, 0.01

    ; CHORD KING engine: 2048-pt Hann analysis (resolves low-string chord
    ; voicings), 4x overlap (v6: was 8x — halved the FFT rate to stop
    ; dropouts; 4x is the canonical PV setting), peak phase locking.
    fAnal  pvsanal aIn, 2048, 512, 2048, 1
    fLock  pvslock fAnal, 1
    fShift pvscale fLock, kRatio, 0, 1
    aOut   pvsynth fShift

    ; SPARKLE (v6): the real Pitch Fork reads bright and glassy — wet path
    ; gets a presence shelf lift instead of the old -2 dB cut. The long
    ; analysis window rounds the top; this restores the shimmer.
    aOut pareq aOut, 7500, ampdb(2.5), 0.7, 2

    gaPFOutL = aOut
    gaPFOutR = aOut
endin
