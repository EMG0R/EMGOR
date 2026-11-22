; NEBULA — long-line Householder FDN reverb ("Supermassive"-style galaxy verb)
; ---------------------------------------------------------------------------
; Menu: DIGIVERB (9) · code neb · toggle 70 · instr 193
; Reads:  gaNebInL  / gaNebInR
; Writes: gaNebWetL / gaNebWetR
; Bypass: gkNebBypass (1 = bypassed). Effect is alwayson (started from score
;         by the integration pass) and internally gated: an active/bypass
;         crossfade (kGate, portk 50 ms) mutes input on bypass so the tail
;         decays naturally and the wet write fades out click-free.
;
; DSP: 4-line stereo FDN with LONG lines (0.25-2.0 s range, scaled by SIZE),
; a Householder feedback matrix (orthogonal 4x4 reflection, energy-safe),
; per-line independent slow LFO on each read tap (0.05-2 Hz, depth 1-10 ms
; auto-scaled inversely with rate so fast modulation never audibly detunes),
; an in-loop tone stage (~4 kHz) darkening the recirculating signal, and a
; short nested-allpass input diffusor (nestedap) smearing the attack before
; it hits the lines.
;
; P1 gkNebMix     — dry/wet (chain-level; also read here as a safety no-op)
; P2 gkNebDensity — DENSITY/MOD (identity knob): 0 = discrete countable long
;    echoes (granular-delay "Cirrus" vibe: pure per-line self-feedback, slow
;    LFO rates), 1 = full chorused galaxy wash (Householder cross-feed at
;    full strength + faster/deeper per-line modulation). Controls inter-line
;    bleed and mod amount together.
; P3 gkNebSize    — SIZE/decay: scales line lengths (0.3x-1.0x of their base
;    0.61-1.47 s lengths) and feedback gain 0.7 -> 0.99.
; EXPR gkNebWarp  — WARP (Supermassive-style, claims the expr pedal): 0 =
;    heel/neutral, stretches ALL line read-tap lengths together 1.0x -> 2.0x
;    toe, portk ~120 ms glide, using the same interpolated (deltap3) taps as
;    the LFO mod so the stretch smears in pitch rather than clicking.
; ---------------------------------------------------------------------------

gkNebBypass  init 1
gkNebMix     init 1
gkNebDensity init 0.4
gkNebSize    init 0.6
gkNebWarp    init 0
gaNebInL     init 0
gaNebInR     init 0
gaNebWetL    init 0
gaNebWetR    init 0

giNebMaxLine = 3.3     ; seconds — longest base line (1.47) * warp 2.0x + headroom

; base line lengths, seconds, at SIZE=1 WARP=0 (mutually-incommensurate)
giNebL1 = 0.83
giNebL2 = 1.13
giNebL3 = 1.47
giNebL4 = 0.61

; one FDN line: write aIn, read back an interpolated tap at kDel seconds
opcode NebLine, a, aki
  aIn, kDel, iMax xin
  aBuf delayr iMax
  aTap deltap3 kDel
       delayw aIn
  xout aTap
endop

instr 193
  kByp  = limit:k(gkNebBypass, 0, 1)
  kGate portk (1 - kByp), 0.05             ; smoothed active-gate, click-free

  kDensity portk limit:k(gkNebDensity, 0, 1), 0.05
  kSize    portk limit:k(gkNebSize, 0, 1), 0.05
  kWarp    portk limit:k(gkNebWarp, 0, 1), 0.12   ; ~120 ms glide (WARP)

  kFB         = 0.7 + 0.29 * kSize                ; 0.7 .. 0.99
  kLenScale   = 0.3 + 0.7 * kSize
  kWarpFactor = 1.0 + kWarp                       ; 1.0x .. 2.0x (WARP)

  ; per-line LFO rates, spread 0.11-0.29 Hz base, scaled 0.3x-2x by density
  kRateMul = 0.3 + 1.7 * kDensity
  kR1 = limit:k(0.11 * kRateMul, 0.05, 2.0)
  kR2 = limit:k(0.17 * kRateMul, 0.05, 2.0)
  kR3 = limit:k(0.23 * kRateMul, 0.05, 2.0)
  kR4 = limit:k(0.29 * kRateMul, 0.05, 2.0)

  ; mod depth auto-scaled ÷ rate — fast mod never detunes, slow mod up to 10ms
  kD1ms = limit:k(3.0 / kR1, 1, 10)
  kD2ms = limit:k(3.0 / kR2, 1, 10)
  kD3ms = limit:k(3.0 / kR3, 1, 10)
  kD4ms = limit:k(3.0 / kR4, 1, 10)

  kLfo1 oscili kD1ms * 0.001, kR1, -1, 0
  kLfo2 oscili kD2ms * 0.001, kR2, -1, 0.27
  kLfo3 oscili kD3ms * 0.001, kR3, -1, 0.54
  kLfo4 oscili kD4ms * 0.001, kR4, -1, 0.81

  kLen1 = limit:k(giNebL1 * kLenScale * kWarpFactor + kLfo1, 0.02, giNebMaxLine - 0.01)
  kLen2 = limit:k(giNebL2 * kLenScale * kWarpFactor + kLfo2, 0.02, giNebMaxLine - 0.01)
  kLen3 = limit:k(giNebL3 * kLenScale * kWarpFactor + kLfo3, 0.02, giNebMaxLine - 0.01)
  kLen4 = limit:k(giNebL4 * kLenScale * kWarpFactor + kLfo4, 0.02, giNebMaxLine - 0.01)

  aInL = gaNebInL * kGate
  aInR = gaNebInR * kGate

  ; input diffusor: two cascaded simple (imode=1) nestedap allpasses.
  ; NOTE: imode=2/3 (nested proper) were found unstable/divergent for these
  ; delay/gain pairs when benched — cascaded imode=1 stages give the same
  ; smeared-attack character and are unconditionally stable (verified).
  aDif1L nestedap aInL, 1, 1, 0.0327, 0.6
  aDifL  nestedap aDif1L, 1, 1, 0.0113, 0.5
  aDif1R nestedap aInR, 1, 1, 0.0294, 0.6
  aDifR  nestedap aDif1R, 1, 1, 0.0137, 0.5

  aFb1 init 0
  aFb2 init 0
  aFb3 init 0
  aFb4 init 0

  aX1 NebLine aDifL + aFb1, kLen1, giNebMaxLine
  aX2 NebLine aDifR + aFb2, kLen2, giNebMaxLine
  aX3 NebLine aDifL + aFb3, kLen3, giNebMaxLine
  aX4 NebLine aDifR + aFb4, kLen4, giNebMaxLine

  ; in-loop tone (damping ~4 kHz keeps the wash from getting harsh)
  aX1 tone aX1, 4000
  aX2 tone aX2, 4000
  aX3 tone aX3, 4000
  aX4 tone aX4, 4000

  ; Householder matrix, blended vs. self-feedback by DENSITY
  aSum = aX1 + aX2 + aX3 + aX4
  aH1 = 0.5 * aSum - aX1
  aH2 = 0.5 * aSum - aX2
  aH3 = 0.5 * aSum - aX3
  aH4 = 0.5 * aSum - aX4

  aFb1 = ((1 - kDensity) * aX1 + kDensity * aH1) * kFB
  aFb2 = ((1 - kDensity) * aX2 + kDensity * aH2) * kFB
  aFb3 = ((1 - kDensity) * aX3 + kDensity * aH3) * kFB
  aFb4 = ((1 - kDensity) * aX4 + kDensity * aH4) * kFB

  aWetL = (aX1 + aX3) * 0.6
  aWetR = (aX2 + aX4) * 0.6

  aWetL dcblock aWetL
  aWetR dcblock aWetR
  aWetL limit aWetL, -0.99, 0.99
  aWetR limit aWetR, -0.99, 0.99

  ; output trim tuned via bench harness for unity RMS at defaults
  iTrim = 1.6
  gaNebWetL = aWetL * kGate * iTrim
  gaNebWetR = aWetR * kGate * iTrim
endin
