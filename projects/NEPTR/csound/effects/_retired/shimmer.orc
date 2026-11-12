; SHIMMER — Strymon BigSky-style shimmer reverb (pitch INSIDE the loop)
; ---------------------------------------------------------------------------
; Instr / toggle (proposed): instr 188, toggle ID 65   (both verified unused
;                            by grep 2026-07-13 over effects/*.orc +
;                            midi_handler.orc + neptrPhase4.csd; parallel
;                            builders hold 186/t63 FEEDBACKER, 187/t64
;                            DIFFUSION — 188/t65 is the next AIR-menu slot.
;                            Integration resolves collisions.)
; Menu / slot   (proposed) : AIR menu (with FEEDBACKER t63 / DIFFUSION t64),
;                            code "shm", UI label "SHIMMER"
; Chain stage              : 21b — between REVERB (21, instr 70) and
;                            IR REVERB (21c, instr 240)
; Reads                    : gaSHM_sendL/R
; Writes                   : gaSHM_outL/R (ALREADY-MIXED dry/wet — IR-REVERB
;                            style assignment in the chain)
;
; TOPOLOGY (the BigSky shimmer trick: the pitch shifter lives inside the
; regeneration loop, so EVERY recirculation climbs another interval):
;
;   in -> input diffuser (4 series allpasses) ---+
;                                                v
;         +--> loop delay ~120 ms (117 L / 123 R for width)
;         |      -> 2 in-loop allpasses (echo -> wash smearing)
;         |      -> dual-window delay-line pitch shifter (pitchshifter4,
;         |         ~50 ms window, crossfaded taps — the house shifter;
;         |         NOT pvs, moderate CPU)
;         |      -> damping LP (tone) + low cut
;         +---<- tanh + dcblock + feedback gain (HARD CAP 0.85)
;
;   wet tap = loop-delay output (original arrival + each climbing pass)
;
; SHIFT (gkSHM_shift, stepped 0/1/2):
;   0 = +12          (ratio 2.0)              — classic octave shimmer
;   1 = +7 & +12     (ratios 1.5 + 2.0 mixed) — fifth+octave "choir" climb
;   2 = -12 "grotto" (ratio 0.5)              — every pass descends
;
; PARAMS (all self-declared below; encoder-v2 rows for the integrator):
;   P1  gkSHM_mix    [0..1]  dry/wet (home-slot rotation)
;   P2  gkSHM_decay  [0..1]  CC20 — feedback amount (0.85 hard cap inside)
;   P3  gkSHM_shift  [0..2]  CC23 — shift interval, stepped
;   P4  gkSHM_damp   [0..1]  fill — damping/tone (high = darker tail)
;
; INTEGRATION NOTES (for the integration agent — this feature branch adds
; ONLY this file + its bench; everything below is a spec, not a diff.
; Integration resolves collisions.):
;   globals.orc     : NOTHING to add — all SHIMMER globals are self-declared
;                     below (include order: globals -> effects). Do NOT
;                     redeclare them.
;   neptrPhase4.csd : #include "effects/shimmer.orc" with the other effects.
;   signal_chain.orc: insert at 21b, right after the REVERB (21) ntrpol pair
;                     and before IR REVERB (21c):
;                       ; 21b. SHIMMER (188) — pitch-in-loop shimmer reverb;
;                       ; instr writes the ALREADY-MIXED blend (IR style)
;                       gaSHM_sendL = aPrevL
;                       gaSHM_sendR = aPrevR
;                       $FX_BYPASS(gkSHM_bypass'188'gaSHM_sendL'gaSHM_sendR'gaSHM_outL'gaSHM_outR'shm)
;                       aPrevL = gaSHM_outL
;                       aPrevR = gaSHM_outR
;   midi_handler.orc: toggle ID 65 in the proposed AIR menu slot:
;                       $FX_TOGGLE(gkSHM_bypass'65)
;                     Encoder takeover rows (dispatch on gkLastToggle == 65):
;                       home-slot rotation (P1): $ENC_ROT(gkSHM_mix'0.02'0'1)
;                       CC20 (P2): $ENC_ROT(gkSHM_decay'0.02'0'1)
;                       CC23 (P3): $ENC_ROT(gkSHM_shift'1'0'2)      ; stepped
;                       P4 fill  : $ENC_ROT(gkSHM_damp'0.02'0'1)
;                     OSC (change-triggered, with the other effect groups):
;                       $OSC_I(gkSHM_bypass'/shm_state'shmByp)
;                       $OSC_F(gkSHM_mix'/shm_mix'shmMix)
;                       $OSC_F(gkSHM_decay'/shm_decay'shmDec)
;                       $OSC_F(gkSHM_shift'/shm_shift'shmShf)
;                       $OSC_F(gkSHM_damp'/shm_damp'shmDmp)
;   neptrPhase4_UI.py: AIR menu row ("shm", "SHIMMER");
;                     EFFECT_PARAMS["shm"] = [("mix","Mix",0,1),
;                       ("decay","Decay",0,1),("shift","Interval",0,2),
;                       ("damp","Damp",0,1)]; route_osc handles /shm_*.
;   docs            : registry row — 65 SHIMMER instr 188 AIR menu, stage 21b.
; ---------------------------------------------------------------------------

; ---- self-declared globals (see INTEGRATION NOTES) ----
gkSHM_bypass init 1
gkSHM_mix    init 0.5
gkSHM_decay  init 0.6
gkSHM_shift  init 0        ; 0 = +12, 1 = +7&+12, 2 = -12 grotto
gkSHM_damp   init 0.35
gaSHM_sendL  init 0
gaSHM_sendR  init 0
gaSHM_outL   init 0
gaSHM_outR   init 0

;------------------------------------------------------------------
; ShmAP — Schroeder allpass diffuser section (fixed delay + gain).
; y = z - g*v ; v = x + g*z ; z = delay(v, iT)
;------------------------------------------------------------------
opcode ShmAP, a, aii
  aIn, iT, iG xin
  aZ delayr iT
  aV = aIn + iG * aZ
  aY = aZ - iG * aV
  delayw aV
  xout aY
endop

instr 188
  aInL = gaSHM_sendL
  aInR = gaSHM_sendR

  ; ---- controls (i() inits: no startup zipper, and mix stays EXACTLY at its
  ; set point when constant — mix=0 must be bit-transparent) ----
  kMix   port gkSHM_mix,   0.05, i(gkSHM_mix)
  kDecay port gkSHM_decay, 0.08, i(gkSHM_decay)
  kDamp  port gkSHM_damp,  0.08, i(gkSHM_damp)
  kShift = int(limit:k(gkSHM_shift, 0, 2) + 0.5)

  ; feedback: HARD CAP 0.85 (with the in-loop tanh this is unconditionally
  ; bounded: |aFb| <= 0.85 whatever builds up)
  kFb limit kDecay * 0.85, 0, 0.85

  ; damping/tone: 0 = open (~9.2 kHz), 1 = dark (~1.2 kHz), 6 dB/oct tone
  kDampCf = 1200 + (1 - kDamp) * 8000

  ; shift ratios/gains per stepped interval (shifter B idles at gain 0)
  kRatA = (kShift == 2 ? 0.5 : 2.0)
  kRatB = 1.5
  kGA   = (kShift == 1 ? 0.71 : 1.0)
  kGB   = (kShift == 1 ? 0.71 : 0.0)

  ; ---- input diffuser: 4 series allpasses (prime-ish ms, g=0.62) ----
  aDifL ShmAP aInL, 0.0047, 0.62
  aDifL ShmAP aDifL, 0.0128, 0.62
  aDifL ShmAP aDifL, 0.0231, 0.62
  aDifL ShmAP aDifL, 0.0347, 0.62
  aDifR ShmAP aInR, 0.0053, 0.62
  aDifR ShmAP aDifR, 0.0119, 0.62
  aDifR ShmAP aDifR, 0.0243, 0.62
  aDifR ShmAP aDifR, 0.0331, 0.62

  ; ---- recirculating loop ----
  aFbL init 0
  aFbR init 0
  aLoopInL = aDifL + aFbL
  aLoopInR = aDifR + aFbR

  ; loop delay ~120 ms (117 L / 123 R: decorrelated recirculation = width)
  aLpL delayr 0.117
  delayw aLoopInL
  aLpR delayr 0.123
  delayw aLoopInR

  ; in-loop smearing: 2 more allpasses per side (echo -> wash)
  aSmL ShmAP aLpL, 0.0291, 0.55
  aSmL ShmAP aSmL, 0.0437, 0.55
  aSmR ShmAP aLpR, 0.0307, 0.55
  aSmR ShmAP aSmR, 0.0413, 0.55

  ; pitch INSIDE the loop — dual-window delay-line shifter (house
  ; pitchshifter4: ~50 ms window, two crossfaded taps, setksmps 1).
  ; Window pair averages 0.5 gain -> *2 restores unity (delay.orc precedent).
  aShAL, aShAR pitchshifter4 aSmL, aSmR, kRatA, 1, 0.05, 0.02, 1, giHanning, 1
  aShBL, aShBR pitchshifter4 aSmL, aSmR, kRatB, 1, 0.05, 0.02, 1, giHanning, 1
  aShL = (kGA * aShAL + kGB * aShBL) * 2
  aShR = (kGA * aShAR + kGB * aShBR) * 2

  ; damping + low cut (stops octave-down grotto mud / DC creep), then the
  ; bounded regeneration: tanh soft wall, dcblock (IIR — zero latency), gain
  aDmpL tone aShL, kDampCf
  aDmpR tone aShR, kDampCf
  aDmpL butterhp aDmpL, 140
  aDmpR butterhp aDmpR, 140
  aFbL = tanh(aDmpL) * kFb
  aFbR = tanh(aDmpR) * kFb
  aFbL dcblock aFbL
  aFbR dcblock aFbR

  ; ---- wet tap + output balance ----
  ; loop-delay output carries the first arrival plus every climbing pass;
  ; 0.8 trim seats the wash beside the dry at mix=0.5
  aWetL = aLpL * 0.8
  aWetR = aLpR * 0.8

  gaSHM_outL = kMix * aWetL + (1 - kMix) * aInL
  gaSHM_outR = kMix * aWetR + (1 - kMix) * aInR
endin
