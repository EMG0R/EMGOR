;==============================================================
; COMPRESSOR — optical/FET-style compressor + downward-expander gate (instr 180)
; DYNAMICS pack, 2026-07-13 batch. LA-2A/1176 class, program-friendly:
; follow2 envelope -> dB-domain soft-knee (6 dB) gain computer -> makeup ->
; single gain lane applied to both channels (stereo-linked, mono detector).
; One AMOUNT macro drives threshold (0..-40 dB) AND ratio (1:1..10:1) together,
; one SPEED macro morphs attack/release from FET-fast (5 ms / 50 ms) to
; opto-slow (40 ms / 400 ms). Parallel (NY) blend lives in the chain ntrpol.
; Built-in gate is a downward expander (2 dB per dB below its own threshold,
; floor -60 dB) with fast-open / 80 ms-close asymmetric smoothing — kills
; single-coil hiss into the drive section without chattering.
; Zero latency (no lookahead, gain-only), cheap inline: 1 follow2 + k-rate
; math + 1 interp per block. No dcblock on purpose (pure gain stage — keeps
; the AMOUNT=0 path bit-transparent for the unity null).
;
; INTEGRATION NOTES (integration agent: apply these; numbers are proposed —
; resolve cross-batch collisions):
;   instr 180 (verified unused in existing csd/midi_handler/effects; proposed,
;     integration agent resolves cross-batch collisions), toggle 57 (proposed).
;   Menu: suggest PREAMP menu empty slot 4 (currently "—"). OSC prefix "cmp".
;   csd: #include "effects/compressor.orc" with the other effects (include
;     order globals -> effects -> chain makes the self-declared globals safe).
;   globals used (self-declared below): gkCMP_bypass, gkCMP_amt, gkCMP_speed,
;     gkCMP_makeup, gkCMP_blend, gkCMP_gate, gaCMPSendL/R, gaCMPOutL/R.
;   signal_chain.orc: stage 0.8 — after BASS DISTORTION (0b/162), before
;     OCTAVER (0.9/170): post-preamp, pre-drives, so it evens the signal
;     feeding the dirt section:
;     gaCMPSendL = aPrevL
;     gaCMPSendR = aPrevR
;     $FX_BYPASS(gkCMP_bypass'180'gaCMPSendL'gaCMPSendR'gaCMPOutL'gaCMPOutR'cmp)
;     aPrevL ntrpol gaCMPSendL, gaCMPOutL, gkCMP_blend
;     aPrevR ntrpol gaCMPSendR, gaCMPOutR, gkCMP_blend
;   midi_handler.orc dispatch (encoder takeover v2): toggle via $FX_TOGGLE
;     (gkCMP_bypass'57). P1 (home-slot rotation) -> gkCMP_amt [0..1] lin .02;
;     CC20 (P2) -> gkCMP_speed [0..1] lin .02; CC23 (P3) -> gkCMP_makeup
;     [0..1] lin .02; P4/P5 (remaining slot rotations, ascending) ->
;     gkCMP_blend [0..1] lin .02, gkCMP_gate [0..1] lin .02.
;     NO CC29 claim (pedal-into-threshold is a future option; gkLastToggle29
;     is contested — do not wire now).
;     OSC sends: /cmp_state, /cmp_amt, /cmp_speed, /cmp_makeup, /cmp_blend,
;     /cmp_gate.
;   neptrPhase4_UI.py EFFECT_PARAMS:
;     "cmp": [("amt", "AMT", 0, 1), ("speed", "SPEED", 0, 1),
;             ("makeup", "MKUP", 0, 1), ("blend", "BLEND", 0, 1),
;             ("gate", "GATE", 0, 1)]   label "COMPRESS".
;==============================================================

gkCMP_bypass init 1
gkCMP_amt    init 0.4    ; macro: threshold 0..-40 dB + ratio 1:1..10:1
gkCMP_speed  init 0.3    ; 0 = FET fast (5/50 ms), 1 = opto slow (40/400 ms)
gkCMP_makeup init 0      ; 0..1 -> 0..+12 dB
gkCMP_blend  init 1      ; parallel blend, consumed by the chain ntrpol
gkCMP_gate   init 0      ; 0 = gate off; else expander threshold -75..-25 dB
gaCMPSendL   init 0
gaCMPSendR   init 0
gaCMPOutL    init 0
gaCMPOutR    init 0

instr 180  ; COMPRESSOR (+gate)
  aInL = gaCMPSendL
  aInR = gaCMPSendR

  kAmt   portk gkCMP_amt,    0.02, i(gkCMP_amt)
  kSpeed portk gkCMP_speed,  0.02, i(gkCMP_speed)
  kMkup  portk gkCMP_makeup, 0.02, i(gkCMP_makeup)
  kGate  portk gkCMP_gate,   0.02, i(gkCMP_gate)

  ; ---- AMOUNT macro -> threshold + ratio together ----
  kThr   = -40 * kAmt                 ; dBFS
  kRatio = 1 + 9 * kAmt               ; 1:1 .. 10:1

  ; ---- SPEED macro -> attack/release morph (fast FET -> slow opto) ----
  ; follow2's att/rel are T60-style lag times: a typical ~12 dB gain
  ; recovery completes in roughly kRel/5 (bench-measured 2026-07-13), which
  ; lands these nominal values right in classic FET/opto territory.
  kAtt = 0.005 + 0.035 * kSpeed       ; 5 .. 40 ms
  kRel = 0.050 + 0.350 * kSpeed       ; 50 .. 400 ms

  ; ---- detector: mono sum, follow2 envelope, dB domain ----
  aDet = (aInL + aInR) * 0.5
  aEnv follow2 aDet, kAtt, kRel
  kEnv downsamp aEnv
  kLev = 20 * log10(max:k(kEnv, 0.00001))   ; floor -100 dB

  ; ---- soft-knee (6 dB) gain computer ----
  kSlope = 1 / kRatio - 1             ; <= 0; exactly 0 at AMOUNT=0 (unity)
  kOver  = kLev - kThr
  if (kOver <= -3) then
    kGR = 0
  elseif (kOver < 3) then
    kGR = kSlope * (kOver + 3) * (kOver + 3) / 12
  else
    kGR = kSlope * kOver
  endif

  ; ---- downward-expander gate: 2 dB cut per dB below threshold ----
  if (kGate > 0.005) then
    kGThr = -75 + 50 * kGate          ; -75 .. -25 dBFS
    kGTgt = limit:k((kLev - kGThr) * 2, -60, 0)
  else
    kGTgt = 0
  endif
  ; asymmetric smoothing: open fast (3 ms) so pick attacks pass, close at a
  ; fixed 80 ms gate-release so the tail decays instead of chopping
  kGSm  init 0
  kGLag = (kGTgt > kGSm ? 0.003 : 0.080)
  kGSm  portk kGTgt, kGLag

  ; ---- combine + makeup, back to linear, a-rate ramp ----
  kGainDb = kGR + kGSm + 12 * kMkup
  kGain   = ampdb(kGainDb)
  aGain   interp kGain

  gaCMPOutL = limit(aInL * aGain, -0.99, 0.99)
  gaCMPOutR = limit(aInR * aGain, -0.99, 0.99)
endin
