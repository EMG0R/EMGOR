; IMPULSE SYNTH v2 — onset-excited percussive synth voice, CLOCKED
; (Lost + Found mode port; v1 free-running trigger archived at
; effects/_retired/impulse_synth_v1_freerun.orc)
; ---------------------------------------------------------------------------
; Signal chain position : GLITCH block (moved POST-LOOPER at integration —
;                         see stutter.orc INTEGRATION NOTES item A; IMPULSE
;                         stays FIRST of the five glitch stanzas)
; Menu / slot           : GLITCH menu (9), slot 0 — toggle 48, instr 156
; Reads                 : gaIMP_sendL / gaIMP_sendR
; Writes                : gaIMP_outL / gaIMP_outR  (DRY UNITY + voice*mix;
;                         chain ntrpol mix constant 1 — unchanged)
; Bypass                : gkIMP_bypass (FX_BYPASS — unchanged)
;
; v2 = THE GLITCH CONTRACT (see stutter.orc header for the full statement):
;   * CLOCKED: a pick onset no longer fires the voice immediately. It ARMS,
;     and the hit FIRES on the next shared subdivision tick (gkGLT_tick from
;     stutter.orc instr 89 — loop-phase-anchored, blooper-style arm/fire).
;     Pitch + velocity are still captured AT THE PICK (responsive), only the
;     launch is quantized. instr 156 > 89, so it reads THIS block's tick.
;   * PROBABILITY gkIMP_prob (init 0.5): an armed pick fires with prob p on
;     its tick; the miss is a dropped hit (that stutter of presence/absence
;     IS the glitch). p=1 -> every pick lands, quantized. p=0 -> silent.
;   * EXPRESSION gkIMP_expr (CC29, sticky claim 48): crossing 0.10 upward
;     force-fires on the next tick; held higher it force-fires a fraction of
;     ticks scaling with position — RETRIGGERING THE LAST HELD PITCH even
;     with no new pick, velocity riding the pedal. Toe = a hit every tick =
;     the synth hammers the grid, wall to wall.
;
; INTEGRATION NOTES (integration agent owns all shared files — this is the
; complete diff spec for IMPULSE; stutter.orc's notes cover the menu's rest):
;   * signal_chain.orc: stanza text unchanged; MOVE post-looper with the
;     glitch block (stutter.orc notes item A).
;   * handler_menus.orc CC74 menu 9 (s0 toggle): keep $FX_TOGGLE(gkIMP_bypass
;     '48); change the toggle-on defaults to
;         gkIMP_mix=0.5  gkIMP_tone=0.5  gkIMP_dec=0.25
;         gkIMP_prob=0.5 gkIMP_expr=0    gkLastToggle29=48
;     (the CC29 claim is NEW — v1 explicitly had none).
;   * handler_menus.orc CC20 (P2), lastToggle 48: WAS gkIMP_tone. REPLACE:
;         $ENC_ROT(gkIMP_prob'0.05'0'1)          ; menu-wide P2=prob rule
;   * handler_menus.orc CC25 (P4 fill), lastToggle 48: ADD (unused today):
;         $ENC_ROT(gkIMP_tone'0.02'0'1)          ; tone moves here
;   * handler_menus.orc CC23 (P3), lastToggle 48: gkIMP_dec — UNCHANGED.
;   * handler_menus.orc CC24 (P1), lastToggle 48: gkIMP_mix — UNCHANGED.
;   * handler_menus.orc CC29: ADD
;         elseif (gkLastToggle29 == 48) then gkIMP_expr = kNorm
;   * handler_osc.orc: keep /imp_state /imp_mix /imp_tone /imp_dec; ADD
;         /imp_prob (f, gkIMP_prob)
;   * neptrPhase4_UI.py: EFFECT_PARAMS["imp"] = [("mix","Mix",0,1),
;         ("prob","Prob",0,1),("dec","Decay",0,1),("tone","Tone",0,1)]
;
; WHAT IT DOES (voice unchanged from v1)
;   Pitch is read from ptrack ONCE at pick time and held (no continuous
;   tracking — mistracks become per-note character, never warble). Voice =
;   saw @ f + square @ f/2 + sub sine @ f/2 -> moogladder whose cutoff rides
;   a per-hit AD envelope; velocity comes from the input envelope at the
;   pick. The dry path is untouched (parallel voice).
; ---------------------------------------------------------------------------

gkIMP_bypass init 1
gkIMP_mix    init 0.5
gkIMP_tone   init 0.5
gkIMP_dec    init 0.25
gkIMP_prob   init 0.5    ; v2: probability knob (P2 CC20)
gkIMP_expr   init 0      ; v2: CC29 instant trigger (sticky claim 48)
gkIMP_trigCount init 0   ; bench/debug telemetry (fired hits)
gkIMP_armCount  init 0   ; bench/debug telemetry (armed picks)
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

  ; ---- pick detection ARMS; pitch/velocity sampled at the pick ----
  kTrig, kEnv OnsetPick aMono, 0.55, 0.05
  kCps, kAmp ptrack aMono, 512
  kHeld init 220
  kVel  init 0
  kVEnv init 0
  kArm  init 0
  if (kTrig == 1) then
    kHeld = (kCps > 55 && kCps < 1200 ? kCps : kHeld)  ; reject wild reads
    kVel = limit(kEnv * 12, 0.15, 1)
    kArm = 1
    gkIMP_armCount = gkIMP_armCount + 1
  endif

  ; ---- FIRE on the shared tick (arm/fire split, blooper-style) ----
  kFire, kInt GLT_FireK gkGLT_tick, (kArm == 1 ? gkIMP_prob : 0), gkIMP_expr, 0
  if (gkGLT_tick == 1) then
    if (kFire == 1) then
      if (kArm == 0) then
        ; pedal-forced retrigger of the held pitch: velocity rides the pedal
        kVel = limit(0.25 + 0.75 * kInt, 0.15, 1)
      else
        ; v2 bonus of the arm/fire delay: by fire time ptrack has had up to
        ; a full tick of post-transient audio, so re-sample the pitch NOW if
        ; it reads sane — far fewer octave/transient mistracks than at the
        ; raw pick (the arm-time capture stays as the fallback).
        kHeld = (kCps > 55 && kCps < 1200 ? kCps : kHeld)
      endif
      kVEnv = 1
      gkIMP_trigCount = gkIMP_trigCount + 1
      gkIMP_heldCps = kHeld
    endif
    kArm = 0            ; an armed pick lives until its tick, hit or miss
  endif

  ; ---- per-hit AD (attack via 3ms port on output gain) ----
  kDecT = 0.03 + 1.97 * gkIMP_dec * gkIMP_dec        ; 30ms .. 2s
  kVEnv = kVEnv * exp(-kDT / kDecT)

  ; ---- voice (unchanged) ----
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
  printks "IMP t=%.3f c=%.0f a=%.0f cps=%.1f venv=%.4f vrms=%.5f\n", 0.005, timeinsts:k(), gkIMP_trigCount, gkIMP_armCount, gkIMP_heldCps, kVEnv, kVrms
  ; zero-period fire line: exact k-block launch time for the grid bench
  kFchg changed gkIMP_trigCount
  if (kFchg == 1) then
    printks "IMPFIRE t=%.5f c=%.0f cps=%.1f\n", 0, timeinsts:k(), gkIMP_trigCount, gkIMP_heldCps
  endif
#endif
endin
