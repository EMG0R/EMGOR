; SHIMMER — pitch-shifted feedback reverb (Valhalla Shimmer-style bloom)
; ---------------------------------------------------------------------------
; Menu: DIGIVERB (9) · code shm · toggle 71 · instr 194
; Reads:  gaShmInL  / gaShmInR
; Writes: gaShmWetL / gaShmWetR
; Bypass: gkShmBypass (1 = bypassed). Alwayson, internally gated (kGate,
;         portk 50 ms) — input muted on bypass so the loop dies out and wet
;         write fades click-free.
;
; DSP: reverbsc (internal fb fixed 0.85) whose stereo output is summed to
; mono and run through ONE mono pitch shifter (pvsanal/pvscale/pvsynth,
; fft 1024 / overlap 256 — the PV smear is desirable here), then
; atone 180 Hz + tone 6 kHz + a gentle tanh limiter, then fed back into the
; reverbsc's input alongside the fresh dry signal. Each pass through the
; loop is transposed again, so held/sustained material blooms upward into a
; shimmering stack of octaves/fifths.
;
; P1 gkShmMix      — dry/wet (chain-level)
; P2 gkShmInterval — STEPPED shift interval, 0..1 in 4 steps:
;                    0 = +12 st, 1 = +7 st, 2 = +5 st, 3 = -12 st (default 0)
; P3 gkShmFbAmt    — loop feedback amount, 0..1 -> 0.3..0.75 actual loop fb
;                    ("bloom amount")
; ---------------------------------------------------------------------------

gkShmBypass   init 1
gkShmMix      init 1
gkShmInterval init 0
gkShmFbAmt    init 0.5
gaShmInL      init 0
gaShmInR      init 0
gaShmWetL     init 0
gaShmWetR     init 0

instr 194
  kByp  = limit:k(gkShmBypass, 0, 1)
  kGate portk (1 - kByp), 0.05

  ; P2 — stepped interval select (4 steps: +12, +7, +5, -12 semitones)
  kIdx  = limit:k(int(limit:k(gkShmInterval, 0, 1) * 3.999), 0, 3)
  kSemi = (kIdx == 0 ? 12 : (kIdx == 1 ? 7 : (kIdx == 2 ? 5 : -12)))
  kRatio = 2 ^ (kSemi / 12)

  ; P3 — loop feedback / bloom amount
  kLoopFB portk (0.3 + 0.45 * limit:k(gkShmFbAmt, 0, 1)), 0.05

  aInL = gaShmInL * kGate
  aInR = gaShmInR * kGate

  aFb init 0                                ; mono shifted feedback (prev cycle)

  aRvInL = aInL + aFb
  aRvInR = aInR + aFb
  aRvL, aRvR reverbsc aRvInL, aRvInR, 0.85, 8000, sr, 1

  aMono = (aRvL + aRvR) * 0.5
  fShm    pvsanal aMono, 1024, 256, 1024, 1
  fShmSc  pvscale fShm, kRatio
  aShift  pvsynth fShmSc

  aShift atone aShift, 180              ; kill sub rumble building up in the loop
  aShift tone  aShift, 6000             ; gentle top-end damping
  aShift = tanh(aShift)                 ; gentle limiter — keeps the loop bounded

  aFb = aShift * kLoopFB

  aWetL = aRvL
  aWetR = aRvR
  aWetL dcblock aWetL
  aWetR dcblock aWetR
  aWetL limit aWetL, -0.99, 0.99
  aWetR limit aWetR, -0.99, 0.99

  ; output trim tuned via bench harness for unity RMS at defaults
  iTrim = 1.15
  gaShmWetL = aWetL * kGate * iTrim
  gaShmWetR = aWetR * kGate * iTrim
endin
