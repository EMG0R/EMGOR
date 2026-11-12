; TRACKSYNTH — monophonic tracking synth (Meris Enzo / Source Audio C4 class)
; ---------------------------------------------------------------------------
; Signal chain position : 19.3 (after IMPULSE SYNTH 19.2, before TREMOLO 19.5)
; Menu / slot           : proposed NEW menu "SYNTH" slot 0 — toggle 61, instr 184, code tsy
; Reads                 : gaTSY_sendL / gaTSY_sendR
; Writes                : gaTSY_outL / gaTSY_outR
; Bypass                : gkTSY_bypass (FX_BYPASS)
;
; INTEGRATION NOTES (integration agent owns all shared files — verified by
; grep 2026-07-13: toggle 61 unused (max in midi_handler is 56), instr 184
; unused; integration resolves collisions if another parallel builder claimed
; either id first)
;   * PRIME BASS-BANK MATERIAL: tracker clamp reaches 55 Hz (A1) so bass
;     fundamentals track; a bass-rig preset slot with mix~0.6, sub~0.7,
;     cut~0.45 makes a Deep-Tracking-Synth preset. Flag for the bass preset
;     bank alongside PLLFUZZ (pllfuzz.orc).
;   * neptrPhase4.csd: needs #include "onset_pick.orc" BEFORE this file
;     (shared with ONWARD/IMPULSE — already included for those), then
;     #include "effects/tracksynth.orc".
;   * globals.orc: gk/ga declarations below may MOVE there verbatim.
;   * signal_chain.orc @ stage 19.3:
;       gaTSY_sendL = aPrevL
;       gaTSY_sendR = aPrevR
;       $FX_BYPASS(gkTSY_bypass'184'gaTSY_sendL'gaTSY_sendR'gaTSY_outL'gaTSY_outR'tsy)
;       aPrevL ntrpol gaTSY_sendL, gaTSY_outL, 1
;       aPrevR ntrpol gaTSY_sendR, gaTSY_outR, 1
;     (chain mix constant 1: P1 blend happens INSIDE the instr.)
;   * midi_handler.orc: menu SYNTH slot 0 dispatch = plain FX_TOGGLE
;     gkTSY_bypass'61. Encoder map v2 (home slot 0 = CC24):
;       P1 (CC24, home rot) gkTSY_mix   [0..1]  lin .02  "Blend"
;       P2 (CC20)           gkTSY_cut   [0..1]  lin .02  "Cutoff" (exp inside DSP)
;       P3 (CC23)           gkTSY_res   [0..1]  lin .02  "Res"
;       P4 (CC25, slot1)    gkTSY_glide [0..1]  lin .02  "Glide"
;       P5 (CC26, slot2)    gkTSY_sub   [0..1]  lin .02  "Sub"
;       P6 (CC21, slot3)    gkTSY_det   [0..1]  lin .02  "Detune"
;     CC29 expression: STICKY CLAIM -> gkTSY_cut (gkLastToggle29 = 61 on
;     toggle-on). Integration arbitrates vs other CC29 claimants; if the
;     claim loses, TSY still works — cutoff just stays encoder-only.
;   * OSC feedback: /tsy_state, /tsy_mix, /tsy_cut, /tsy_res, /tsy_glide,
;     /tsy_sub, /tsy_det ($OSC_F, house pattern).
;   * neptrPhase4_UI.py: proposed new MENUS row (with PLLFUZZ):
;       {"name": "SYNTH", "effects": [("tsy","TRACKSYN"), ("pll","PLLFUZZ"),
;                                     ("—","—"), ("—","—"), ("—","—")]}
;     EFFECT_PARAMS["tsy"] = [("mix","Blend",0,1),("cut","Cutoff",0,1),
;         ("res","Res",0,1),("glide","Glide",0,1),("sub","Sub",0,1),
;         ("det","Detune",0,1)]
;
; WHAT IT DOES
;   ptrack (hop 1024, ~21ms latency — benched: hop 512 octave-errors on E2/A2
;   saws, 1024 is clean 55-1200) follows the
;   input pitch, clamped 55-1200 Hz with hold-previous on tracking loss
;   (out-of-range read or input below the level gate). The held pitch runs
;   through a glide (portk, P4) into a 3-oscillator voice: vco2 saw @ f,
;   vco2 square @ f*detune (P6, 0..50 cents), vco2 square sub @ f/2 (P5
;   level) -> moogladder whose cutoff rides a per-onset AD envelope
;   (OnsetPick trigger, shared UDO) on top of the CC20 base cutoff.
;   Amplitude is SLAVED to the input envelope — the synth dies with the
;   string, no gate hangover. P1 blends dry vs synth.
; ---------------------------------------------------------------------------

gkTSY_bypass init 1
gkTSY_mix    init 0.5
gkTSY_cut    init 0.6
gkTSY_res    init 0.3
gkTSY_glide  init 0.1
gkTSY_sub    init 0.5
gkTSY_det    init 0.3
gaTSY_sendL  init 0
gaTSY_sendR  init 0
gaTSY_outL   init 0
gaTSY_outR   init 0

instr 184
  aInL = gaTSY_sendL
  aInR = gaTSY_sendR
  aMono = (aInL + aInR) * 0.5
  kDT = ksmps/sr

  ; ---- onset trigger (shared UDO) for the filter envelope ----
  kTrig, kEnvP OnsetPick aMono, 0.55, 0.05

  ; ---- amplitude slave envelope (synth dies with the string) ----
  aAmpE follow2 aMono, 0.004, 0.045
  kAmpE downsamp aAmpE

  ; ---- pitch tracking: 55-1200 clamp, hold-previous on loss ----
  kCps, kAdb ptrack aMono, 1024
  kHeld init 110
  if (kCps >= 55 && kCps <= 1200 && kAmpE > 0.004) then
    kHeld = kCps
  endif

  ; ---- glide / portamento: htim 4ms .. ~600ms (squared taper) ----
  kGl = limit(gkTSY_glide, 0, 1)
  kGlideT = 0.004 + 0.6 * kGl * kGl
  kFq portk kHeld, kGlideT, 110

  ; ---- per-onset filter AD (4ms attack shaping, ~350ms decay) ----
  kFEnv init 0
  if (kTrig == 1) then
    kFEnv = 1
  endif
  kFEnv = kFEnv * exp(-kDT / 0.35)
  kFA portk kFEnv, 0.004

  ; ---- voice: saw + detuned square + square sub ----
  kDetR = pow(2, limit(gkTSY_det, 0, 1) * 50 / 1200)   ; 0..50 cents
  aOsc1 vco2 0.30, kFq
  aOsc2 vco2 0.22, kFq * kDetR, 10
  aSub  vco2 0.30 * limit(gkTSY_sub, 0, 1), kFq * 0.5, 10
  aVce = aOsc1 + aOsc2 + aSub

  ; ---- moogladder: CC20 base cutoff (exp 100..8k) + envelope ride ----
  kCutB = 100 * pow(80, limit(gkTSY_cut, 0, 1))
  kCut  = limit(kCutB * (1 + 1.5 * kFA), 40, 14000)
  kRes  = limit(gkTSY_res, 0, 1) * 0.9
  aFlt moogladder aVce, kCut, kRes

  ; ---- amplitude slaving (a-rate, no zipper) ----
  aGain = limit(aAmpE * 5, 0, 1)
  aSyn = aFlt * aGain

  ; ---- P1 synth/dry blend ----
  kMix portk limit(gkTSY_mix, 0, 1), 0.02
  aOutL ntrpol aInL, aSyn, kMix
  aOutR ntrpol aInR, aSyn, kMix
  aOutL dcblock aOutL
  aOutR dcblock aOutR
  aOutL limit aOutL, -0.99, 0.99
  aOutR limit aOutR, -0.99, 0.99
  gaTSY_outL = aOutL
  gaTSY_outR = aOutR

#ifdef TSYDBG
  kSrms rms aSyn
  printks "TSY t=%.3f held=%.1f fq=%.1f env=%.4f fenv=%.3f srms=%.5f\n", 0.005, timeinsts:k(), kHeld, kFq, kAmpE, kFA, kSrms
#endif
endin
