; DIFFUSION — reverb-front-half-only transient smear (Chroma Console Diffuse)
; ---------------------------------------------------------------------------
; Signal chain position : 18.7 (proposed — after VOLUME PEDAL/18, before
;                          SPECTRAL DLY/19 and the delay/reverb tail section,
;                          so the smear feeds the real reverb but is NOT one)
; Menu / slot           : proposed AIR menu — toggle 64, instr 187, code dif
; Reads                 : gaDIF_sendL / gaDIF_sendR
; Writes                : gaDIF_outL / gaDIF_outR  (100% wet — chain ntrpol
;                         against the send with gkDIF_wet)
; Bypass                : gkDIF_bypass (FX_BYPASS)
;
; INTEGRATION NOTES (integration agent owns all shared files; every number
; below is PROPOSED — the integration pass resolves cross-batch collisions):
;   * instr 187 — verified unused by grep over csound/*.orc, csound/effects/*.orc,
;     signal_chain.orc, neptrPhase4.csd at build time (2026-07-13). PROPOSED.
;   * toggle 64 — verified unused by grep over effects/*.orc + midi_handler.orc
;     at build time. PROPOSED.
;   * neptrPhase4.csd: #include "effects/diffusion.orc" (self-contained; the
;     gk/ga declarations below may MOVE to globals.orc verbatim, then delete
;     them here).
;   * signal_chain.orc @ stage 18.7:
;       gaDIF_sendL = aPrevL
;       gaDIF_sendR = aPrevR
;       $FX_BYPASS(gkDIF_bypass'187'gaDIF_sendL'gaDIF_sendR'gaDIF_outL'gaDIF_outR'dif)
;       aPrevL ntrpol gaDIF_sendL, gaDIF_outL, gkDIF_wet
;       aPrevR ntrpol gaDIF_sendR, gaDIF_outR, gkDIF_wet
;   * midi_handler.orc: FX_TOGGLE gkDIF_bypass'64 on its menu slot.
;     Encoder takeover v2 dispatch on gkLastToggle==64:
;       P1 (home-slot rot)  -> gkDIF_wet   [0..1] lin .02 -> /dif_wet  "DryWet"
;       P2 (CC20, always)   -> gkDIF_depth [0..1] lin .02 -> /dif_depth "Depth"
;                              (scales allpass times, gains AND live stage
;                              count 2->6 — the whole smear grows together)
;       P3 (CC23, always)   -> gkDIF_rate  [0..1] lin .02 -> /dif_rate "ModRate"
;                              (0.08..2.6 Hz exp inside the DSP)
;       P4 (next slot rot)  -> gkDIF_duck  [0..1] lin .02 -> /dif_duck "Duck"
;     NO CC29 claim (pedal stays free; a future option is wet-morph, do not
;     wire now — gkLastToggle29 is contested this batch).
;   * OSC feedback: /dif_state, /dif_wet, /dif_depth, /dif_rate, /dif_duck.
;   * neptrPhase4_UI.py: MENUS entry ("dif","DIFFUSION");
;     EFFECT_PARAMS["dif"] = [("wet","DryWet",0,1),("depth","Depth",0,1),
;                             ("rate","ModRate",0,1),("duck","Duck",0,1)]
;
; WHAT IT DOES
;   The FRONT HALF of a reverb, amputated on purpose: a series cascade of 6
;   LFO-modulated Schroeder allpasses (custom delayr/deltap3 form — stock
;   alpass can't modulate its loop time), golden-ratio-spaced 5–40 ms, with
;   NO recirculating tank. Transients smear into a soft cloud that dies out
;   in well under 400 ms — texture, not space. DEPTH scales delay times,
;   allpass gains and the number of live stages together (2 stages tight ->
;   6 stages max smear). RATE moves each stage's delay with a de-phased sine
;   (chorus-like inner motion). DUCK follows the input envelope and pushes
;   the wet cloud down while you play so it only blooms in the gaps.
;   L/R run decorrelated time sets (R = L * 1.07). Cheap: 12 deltap3 taps.
; ---------------------------------------------------------------------------

gkDIF_bypass init 1
gkDIF_wet    init 0.5    ; P1 — dry/wet (applied by the chain ntrpol)
gkDIF_depth  init 0.6    ; P2 (CC20) — diffusion depth (stages/time scaling)
gkDIF_rate   init 0.3    ; P3 (CC23) — allpass LFO rate
gkDIF_duck   init 0      ; P4 — envelope duck amount
gaDIF_sendL  init 0
gaDIF_sendR  init 0
gaDIF_outL   init 0
gaDIF_outR   init 0

; --- DifAP: one modulated Schroeder allpass ------------------------------
; w[n] = x[n] + g*w[n-D];  y[n] = w[n-D] - g*w[n]   (D modulated, deltap3)
; iMaxS sized so max depth + LFO never clips the tap; floor 3.2 ms keeps
; deltap3 above the 1-kperiod (2.67 ms @ ksmps=128) minimum.
opcode DifAP, a, akkkki
  aIn, kDelS, kG, kModS, kRate, iPh xin
  iMaxS = 0.055
  kLfo poscil kModS, kRate, -1, iPh
  aDel interp limit:k(kDelS + kLfo, 0.0032, iMaxS - 0.001)
  aW   delayr iMaxS
  aTap deltap3 aDel
       delayw aIn + kG * aTap
  aOut = aTap - kG * (aIn + kG * aTap)
  xout aOut
endop

; --- DifCascade: 6 stages, depth-scaled times/gains/stage-count ----------
; Base times golden-ratio spaced across 5..40 ms (5 * phi^i, capped):
;   5.00, 8.09, 13.09, 21.18, 34.27, 40.00 ms  (iSpread decorrelates R)
opcode DifCascade, a, akkki
  aIn, kDepth, kG, kRate, iSpread xin
  iT1 = 0.00500 * iSpread
  iT2 = 0.00809 * iSpread
  iT3 = 0.01309 * iSpread
  iT4 = 0.02118 * iSpread
  iT5 = 0.03427 * iSpread
  iT6 = 0.04000 * iSpread
  ; time scale: shallow depth tightens the cloud toward 30% size
  kTS = 0.3 + 0.7 * kDepth
  kMod = 0.0004 + 0.0009 * kDepth       ; 0.4..1.3 ms LFO throw
  ; stage enable weights: 2 stages always, 3..6 fade in with depth
  kW3 = limit:k(kDepth * 5 - 0.5, 0, 1)
  kW4 = limit:k(kDepth * 5 - 1.5, 0, 1)
  kW5 = limit:k(kDepth * 5 - 2.5, 0, 1)
  kW6 = limit:k(kDepth * 5 - 3.5, 0, 1)
  a1 DifAP aIn, iT1 * kTS, kG, kMod, kRate * 1.00, 0.00
  a2 DifAP a1,  iT2 * kTS, kG, kMod, kRate * 0.83, 0.17
  a3 DifAP a2,  iT3 * kTS, kG * kW3, kMod, kRate * 1.19, 0.34
  a4 DifAP a3,  iT4 * kTS, kG * kW4, kMod, kRate * 0.71, 0.52
  a5 DifAP a4,  iT5 * kTS, kG * kW5, kMod, kRate * 1.31, 0.68
  a6 DifAP a5,  iT6 * kTS, kG * kW6, kMod, kRate * 0.91, 0.85
  xout a6
endop

instr 187  ; DIFFUSION
  aInL = gaDIF_sendL
  aInR = gaDIF_sendR

  kDepth portk limit:k(gkDIF_depth, 0, 1), 0.05
  kRate  = 0.08 * exp(log(2.6 / 0.08) * limit:k(gkDIF_rate, 0, 1))  ; 0.08..2.6 Hz
  ; allpass gain: capped at 0.67 so the slowest stage tau (~100 ms) keeps
  ; the whole cascade dead well before 500 ms — this must NOT become a reverb
  kG = 0.35 + 0.32 * kDepth

  aWetL DifCascade aInL, kDepth, kG, kRate, 1.0
  aWetR DifCascade aInR, kDepth, kG, kRate, 1.07

  ; ---- envelope duck: cloud drops under playing, blooms in the gaps ----
  aEnv follow2 (aInL + aInR) * 0.5, 0.005, 0.09
  kEnv downsamp aEnv
  kDuck portk limit:k(gkDIF_duck, 0, 1), 0.05
  kDkG  = 1 - kDuck * limit:k(kEnv * 14, 0, 1)
  kDkG  portk kDkG, 0.03
  aWetL = aWetL * kDkG
  aWetR = aWetR * kDkG

#ifdef DIF_BENCH_DBG
  kTdbg timeinsts
  printks "DIF t=%.3f depth=%.2f g=%.3f env=%.4f dk=%.3f\n", 0.05, kTdbg, kDepth, kG, kEnv, kDkG
#endif

  ; 100% wet out; dry/wet is the chain's ntrpol (dcblock NOT dcblock2)
  aWetL dcblock aWetL
  aWetR dcblock aWetR
  aWetL limit aWetL, -0.99, 0.99
  aWetR limit aWetR, -0.99, 0.99
  gaDIF_outL = aWetL
  gaDIF_outR = aWetR
endin
