; IMPULSE SYNTH — onset-excited percussive synth voice (Lost + Found mode port)
; ---------------------------------------------------------------------------
; Signal chain position : 19.2 (after GRANULAR 19, before TREMOLO 19.5)
; Menu / slot           : GLITCH menu (9), slot 0 — toggle 48, instr 156, code imp
; Reads                 : gaIMP_sendL / gaIMP_sendR
; Writes                : gaIMP_outL / gaIMP_outR  (DRY UNITY + voice*mix inside;
;                         chain ntrpol mix constant 1 — see notes)
; Bypass                : gkIMP_bypass (FX_BYPASS)
;
; INTEGRATION NOTES (integration agent owns all shared files)
;   * neptrPhase4.csd: needs #include "onset_pick.orc" (shared with ONWARD),
;     then #include "effects/impulse_synth.orc".
;   * globals.orc: gk/ga declarations below may MOVE there verbatim.
;   * signal_chain.orc @ stage 19.2:
;       gaIMP_sendL = aPrevL
;       gaIMP_sendR = aPrevR
;       $FX_BYPASS(gkIMP_bypass'156'gaIMP_sendL'gaIMP_sendR'gaIMP_outL'gaIMP_outR'imp)
;       aPrevL ntrpol gaIMP_sendL, gaIMP_outL, 1
;       aPrevR ntrpol gaIMP_sendR, gaIMP_outR, 1
;   * midi_handler.orc: menu 9 slot 0 dispatch = plain FX_TOGGLE gkIMP_bypass'48.
;     EXPLICITLY NOT in the glitch chance/BPM group — s1..s4 (BBCUT/WGUIDE/
;     SQMOD/FSHIFT) keep their shared chance semantics; s0 is a normal toggle.
;     lastToggle 48 cases: rotation -> gkIMP_mix, CC20 -> gkIMP_tone,
;     CC23 -> gkIMP_dec. No CC29. Defaults: mix .5 tone .5 dec .25.
;   * OSC feedback: /imp_state, /imp_mix, /imp_tone, /imp_dec.
;   * neptrPhase4_UI.py: MENUS m9 s0 ("imp","IMPULSE");
;     EFFECT_PARAMS["imp"] = [("mix","Mix",0,1),("tone","Tone",0,1),
;                             ("dec","Decay",0,1)]
;
; WHAT IT DOES
;   Every pick onset fires a synth hit: pitch is read from ptrack ONCE at
;   trigger time and held (no continuous tracking — mistracks become per-note
;   character, never warble). Voice = saw @ f + square @ f/2 + sub sine @ f/2
;   -> moogladder whose cutoff rides a per-hit AD envelope; amplitude takes
;   its velocity from the input envelope at the moment of the pick, so
;   dynamics carry through. The dry path is untouched (parallel voice).
; ---------------------------------------------------------------------------

gkIMP_bypass init 1
gkIMP_mix    init 0.5
gkIMP_tone   init 0.5
gkIMP_dec    init 0.25
gkIMP_trigCount init 0   ; bench/debug telemetry
gkIMP_heldCps   init 0   ; bench/debug telemetry
gaIMP_sendL init 0
gaIMP_sendR init 0
gaIMP_outL  init 0
gaIMP_outR  init 0

instr 156
  aInL = gaIMP_sendL
  aInR = gaIMP_sendR
  aMono = (aInL + aInR) * 0.5
  kDT = ksmps/sr

  ; ---- trigger + one-shot pitch sample ----
  kTrig, kEnv OnsetPick aMono, 0.55, 0.05
  kCps, kAmp ptrack aMono, 512
  kHeld init 220
  kVel  init 0
  kVEnv init 0
  if (kTrig == 1) then
    kHeld = (kCps > 55 && kCps < 1200 ? kCps : kHeld)  ; reject wild reads
    kVel = limit(kEnv * 12, 0.15, 1)
    kVEnv = 1
    gkIMP_trigCount = gkIMP_trigCount + 1
    gkIMP_heldCps = kHeld
  endif

  ; ---- per-hit AD (attack via 3ms port on output gain) ----
  kDecT = 0.03 + 1.97 * gkIMP_dec * gkIMP_dec        ; 30ms .. 2s
  kVEnv = kVEnv * exp(-kDT / kDecT)

  ; ---- voice ----
  aSaw vco2 0.28, kHeld
  aSqr vco2 0.20, kHeld * 0.5, 10
  aSub poscil 0.25, kHeld * 0.5
  aVoice = aSaw + aSqr + aSub
  kCutB = 180 * pow(33, limit(gkIMP_tone, 0, 1))     ; 180Hz .. ~6kHz base
  kCut  = limit(kCutB * (1 + 2.2 * kVEnv), 40, 12000)
  aFlt moogladder aVoice, kCut, 0.4

  kGain portk kVEnv * kVel, 0.003
  aGain interp kGain
  aV = aFlt * aGain

  ; ---- output: dry unity + voice under it ----
  kMix portk limit(gkIMP_mix, 0, 1), 0.02
  aMix interp kMix
  aOutL = aInL + aV * aMix
  aOutR = aInR + aV * aMix
  aOutL dcblock aOutL
  aOutR dcblock aOutR
  aOutL limit aOutL, -0.99, 0.99
  aOutR limit aOutR, -0.99, 0.99
  gaIMP_outL = aOutL
  gaIMP_outR = aOutR

#ifdef IMPDBG
  kVrms rms aV
  printks "IMP t=%.3f c=%.0f cps=%.1f venv=%.4f vrms=%.5f\n", 0.005, timeinsts:k(), gkIMP_trigCount, gkIMP_heldCps, kVEnv, kVrms
#endif
endin
