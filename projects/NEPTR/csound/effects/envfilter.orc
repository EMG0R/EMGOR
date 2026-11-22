;==============================================================
; ENV FILTER — boutique bass envelope filter v2 (instr 171)
; Mu-Tron III / Aguilar Filter Twin / EBS BassIQ class. Standalone on purpose:
; AUTOMOOG (instr 51) is the surf autowah voicing and stays untouched — this
; one is bass-first (shaped attack/decay, dual opposing sweeps, low-end
; crossover) and the two can coexist in different menus.
;
; Envelope follower -> SHAPED attack/decay (independent port lags, not the
; raw envelope) -> drives TWO resonant moogladders over the >150 Hz band:
; an up-sweep and a down-sweep filter. One knob (MODE) morphs the blend:
;   0.0 = down-sweep only  (reverse quack)
;   0.5 = up-sweep only    (classic Mu-Tron)
;   1.0 = dual-opposed     (Filter Twin 3-D quack)
; Everything below the 150 Hz crossover passes dry inside the wet path, so
; the bass bottom never disappears at any setting. Cheap inline, zero latency.
; (Crossover is fixed at 150 Hz — a parameter here is a foot-gun live; change
; the constant below if the rig's bass wants 120/180.)
;
; INTEGRATION NOTES (integration agent: apply these; numbers are proposed —
; resolve cross-batch collisions):
;   instr 171 (verified unused in existing csd/midi_handler; proposed,
;     integration agent resolves cross-batch collisions), toggle 55 (proposed).
;   Menu: new BASS menu (primary home). OSC prefix "env".
;   globals used (self-declared below): gkENV_bypass, gkENV_wet, gkENV_sens,
;     gkENV_mode, gkENV_att, gkENV_dec, gaENVSendL/R, gaENVOutL/R.
;   signal_chain.orc: filter block, suggested stage 16.5 (after WAH 52, so
;     env-filter-into-drive users re-order via presets, not chain surgery):
;     gaENVSendL = aPrev_L ; gaENVSendR = aPrev_R
;     $FX_BYPASS(gkENV_bypass'171'gaENVSendL'gaENVSendR'gaENVOutL'gaENVOutR'env)
;     aPrev_L ntrpol gaENVSendL, gaENVOutL, gkENV_wet   (same for R)
;   midi_handler.orc dispatch: slot rotation -> gkENV_wet; CC20 -> gkENV_sens;
;     CC23 -> gkENV_mode; P4/P5 encoders (takeover v2 "rest") -> gkENV_att,
;     gkENV_dec. NO CC29 claim (future option: manual sweep override via
;     pedal — do not wire now, sticky gkLastToggle29 is contested).
;     OSC sends: /env_state, /env_wet, /env_sens, /env_mode, /env_att, /env_dec.
;   neptrPhase4_UI.py EFFECT_PARAMS "env": [("wet","WET",0,1),
;     ("sens","SENS",0,1),("mode","MODE",0,1),("att","ATT",0,1),
;     ("dec","DEC",0,1)]  label "ENV FILTER".
;==============================================================

gkENV_bypass init 1
gkENV_wet    init 0.8
gkENV_sens   init 0.55
gkENV_mode   init 0.5
gkENV_att    init 0.15   ; 0..1 -> 4..250 ms attack shape
gkENV_dec    init 0.35   ; 0..1 -> 40..600 ms decay shape
gaENVSendL   init 0
gaENVSendR   init 0
gaENVOutL    init 0
gaENVOutR    init 0

instr 171  ; ENV FILTER
  aInL = gaENVSendL
  aInR = gaENVSendR
  aMono = (aInL + aInR) * 0.5
  aMono dcblock aMono, 0.9995

  ; ---- envelope: raw follower, then shaped attack/decay ----
  aEnvRaw follow2 aMono, 0.003, 0.05
  kEnvRaw downsamp aEnvRaw
  kAttT = 0.004 + gkENV_att * 0.246          ; 4..250 ms
  kDecT = 0.040 + gkENV_dec * 0.560          ; 40..600 ms
  kShape init 0
  kLag   = (kEnvRaw > kShape ? kAttT : kDecT)
  kShape portk kEnvRaw, kLag

  ; sensitivity -> normalized sweep drive 0..1
  kSens portk gkENV_sens, 0.05
  ; gain law: sens 0 -> needs a hot signal to move; sens 1 -> light touch
  ; sweeps fully. Kept low enough that the DECAY of a normal note travels the
  ; whole range instead of pinning at max (pinning = no quack).
  kDrive limit kShape * (0.5 + kSens * 7), 0, 1

  ; ---- sweep targets (exponential, musical) ----
  kFup = 200 * exp(log(2) * kDrive * 3.6)     ; 200 Hz -> ~2.4 kHz
  kFdn = 2400 * exp(-log(2) * kDrive * 3.3)   ; 2.4 kHz -> ~240 Hz
  kFup limit kFup, 180, 5000
  kFdn limit kFdn, 180, 5000

  ; ---- mode morph weights ----
  kMode portk gkENV_mode, 0.05
  kWup = limit:k(2 * kMode, 0, 1)
  kWdn = (kMode < 0.5 ? 1 - 2 * kMode : 2 * kMode - 1)
  kNorm = 1 / (kWup + kWdn + 0.0001)
  kNorm limit kNorm, 0.5, 1                  ; dual mode: equal halves, no boost

  ; ---- low-end crossover: <150 Hz stays dry inside the wet path ----
  aLoL butterlp aInL, 150
  aLoR butterlp aInR, 150
  aHiL = aInL - aLoL
  aHiR = aInR - aLoR

  aUpL moogladder aHiL, kFup, 0.58
  aUpR moogladder aHiR, kFup, 0.58
  aDnL moogladder aHiL, kFdn, 0.58
  aDnR moogladder aHiR, kFdn, 0.58

  aFiltL = (aUpL * kWup + aDnL * kWdn) * kNorm
  aFiltR = (aUpR * kWup + aDnR * kWdn) * kNorm

  ; makeup: moogladder eats level at high res; keep quack present
  aWetL = aLoL + aFiltL * 1.5
  aWetR = aLoR + aFiltR * 1.5

  gaENVOutL = tanh(aWetL) * 0.99
  gaENVOutR = tanh(aWetR) * 0.99
endin
