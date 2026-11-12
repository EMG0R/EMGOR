; Glitch Effects (split from original Stutter)
; 4 independent instruments sharing gkGLITCH_bpm and gkGLITCH_chance
;
; BBcut (90)     — beat-cutting/stutter with resonant filter
; WGuide (91)    — waveguide pluck glitch
; SqMod (92)     — square wave amplitude modulation
; FreqShift (93) — random frequency shifting
;
; Hold/click semantics (handled at MIDI layer, reflected here by gk*_held):
;   click  → effect bypassed-state flips, uses user probability (gkGLITCH_chance)
;   hold   → effect ON while held, chance forced to 1.0 (always-engaged)
;
; Dice-gated effects (WGuide, SqMod, FShift) now ramp their gate with a
; short portk so activation/deactivation doesn't pop as a sudden square wave.

;==============================================================
; BBCUT — Beat Cutter
; Signal chain position: 90
; Reads: gaBBcut_sendL/R
; Writes: gaBBcut_wetL/R
;
; bbcutm is i-rate only, so BPM changes still require reinit.
; We limit reinit triggers to the things that actually change
; (BPM + held flag — held flips stutter chance i-value to 1.0).
;==============================================================
instr 90
 iSUBdiv    init 8
 iBARlen    init 2
 iPHRASE    init 8
 iREPEATS   init 4
 iSTUTspd   init 8
 iFLTdiv    init 1
 iBW        init 0.9
 iCFmin     init 80
 iCFmax     init 12000
 iIh        init 0
 iLAYERS    init 2
 iGAIN      init 1.1

 ; Held → force stutter chance to 1.0 at i-rate via reinit
 iSTUTchnc  = (i(gkBBcut_held) > 0.5 ? 1.0 : 0.5)

 iBPM = i(gkGLITCH_bpm)
 iBPS = iBPM/60

 kTrig metro 4
 kSw   init 0
 if kTrig == 1 then
   kSw changed gkGLITCH_bpm, gkBBcut_held
 endif
 if kSw == 1 then
   reinit UPDATE
 endif

UPDATE:
 aL = gaBBcut_sendL
 aR = gaBBcut_sendR

 a1, a2 STUTiteration aL, aR, iBPS, iSUBdiv, iBARlen, iPHRASE, iREPEATS, iSTUTspd, iSTUTchnc, 1, iLAYERS

 ifreq = iBPS * iFLTdiv
 kcf1h randomh iCFmin, iCFmax, ifreq
 kcf1i lineto  kcf1h, 1/ifreq
 kcf1  ntrpol  kcf1i, kcf1h, iIh
 aFltL resonz  a1, kcf1, kcf1*iBW, 2
 aMixL ntrpol  a1, aFltL, gkBBcut_fltMix

 kcf2h randomh iCFmin, iCFmax, ifreq
 kcf2i lineto  kcf2h, 1/ifreq
 kcf2  ntrpol  kcf2i, kcf2h, iIh
 aFltR resonz  a2, kcf2, kcf2*iBW, 2
 aMixR ntrpol  a2, aFltR, gkBBcut_fltMix

 rireturn
 gaBBcut_wetL = aMixL * iGAIN
 gaBBcut_wetR = aMixR * iGAIN
endin

;==============================================================
; WGUIDE — Waveguide Pluck Glitch
; Signal chain position: 91
; Reads: gaWGuide_sendL/R
; Writes: gaWGuide_wetL/R
;
; Fully k-rate — no reinit, no pops on BPM change.
;==============================================================
instr 91
 iWGUIDEmin init 36
 iWGUIDEmax init 84
 iGAIN      init 1.2
 iGateSmMs  init 4    ; gate ramp (ms) — kills sudden-onset clicks

 kBPS = gkGLITCH_bpm / 60

 aL = gaWGuide_sendL
 aR = gaWGuide_sendR

 ; Per-effect chance — 1.0 while held, else user probability
 kChance = (gkWGuide_held > 0.5 ? 1.0 : gkGLITCH_chance)

 kmetr metro kBPS
 kdice trandom kmetr, 0, 1
 kGate init 0
 if (kmetr == 1) then
   kGate = (kdice < kChance ? 1 : 0)
 endif
 kGateSm portk kGate, iGateSmMs * 0.001
 aGate  interp kGateSm

 ; Always compute the wet signal, then crossfade with the smoothed gate
 knum randomh iWGUIDEmin, iWGUIDEmax, kBPS
 afrq interp cpsmidinn(int(knum))
 kfb  = gkWGuide_fb + (randomi:k(-0.02, 0.02, kBPS/4))   ; 2026-06-23 expose WGuide_fb (was randomi 0.92,0.998)
 kfb  limit kfb, 0.90, 0.998
 kcf  randomi 600, 6000, kBPS
 aWetL wguide1 aL*1.2, afrq, kcf, kfb
 aWetR wguide1 aR*1.2, afrq, kcf, kfb

 aOutL = aL + aGate * (aWetL - aL)
 aOutR = aR + aGate * (aWetR - aR)

 gaWGuide_wetL = aOutL * iGAIN
 gaWGuide_wetR = aOutR * iGAIN
endin

;==============================================================
; SQMOD — Square Wave Amplitude Modulation
; Signal chain position: 92
; Reads: gaSqMod_sendL/R
; Writes: gaSqMod_wetL/R
;
; Fully k-rate — the old reinit was the main culprit for the
; "square wave for a second" blip when editing BPM/chance.
;==============================================================
instr 92
 iSQMODmin  init 4
 iSQMODmax  init 14
 iGAIN      init 1.1
 iGateSmMs  init 4

 kBPS = gkGLITCH_bpm / 60

 aL = gaSqMod_sendL
 aR = gaSqMod_sendR

 kChance = (gkSqMod_held > 0.5 ? 1.0 : gkGLITCH_chance)

 kmetr metro kBPS
 kroll trandom kmetr, 0, 1
 kGate init 0
 if (kmetr == 1) then
   kGate = (kroll < kChance ? 1 : 0)
 endif
 kGateSm portk kGate, iGateSmMs * 0.001
 aGate  interp kGateSm

 kratei randomi gkSqMod_freq-5, gkSqMod_freq+5, kBPS   ; 2026-06-23 expose SqMod_freq (was iSQMODmin/max 4..14)
 krateh randomh gkSqMod_freq-5, gkSqMod_freq+5, kBPS   ; 2026-06-23 expose SqMod_freq
 kx     randomi 0, 1, kBPS
 krate  ntrpol  kratei, krateh, kx
 krate  limit   krate, iSQMODmin, iSQMODmax            ; 2026-06-23 clamp to [4,14]
 amod   lfo 1, cpsoct(krate), 2
 kcf    limit cpsoct(krate)*4, 20, sr/3
 amod   clfilt amod, kcf, 0, 2
 aWetL = aL * amod
 aWetR = aR * amod

 aOutL = aL + aGate * (aWetL - aL)
 aOutR = aR + aGate * (aWetR - aR)

 gaSqMod_wetL = aOutL * iGAIN
 gaSqMod_wetR = aOutR * iGAIN
endin

;==============================================================
; FREQSHIFT — Random Frequency Shifting
; Signal chain position: 93
; Reads: gaFShift_sendL/R
; Writes: gaFShift_wetL/R
;
; Fully k-rate. Chance is scaled by 0.2 (matches original weighting).
;==============================================================
instr 93
 iFSHIFTmin init -2400
 iFSHIFTmax init 2400
 iGAIN      init 1.15
 iGateSmMs  init 4

 kBPS = gkGLITCH_bpm / 60

 aL = gaFShift_sendL
 aR = gaFShift_sendR

 ; Hold → 100%; in TOTAL GLITCH mode (gkTotalGlitch<0.5) 3x more likely; else rare
 kFShiftScale = (gkTotalGlitch < 0.5 ? 0.20 : 0.2)
 kChance = (gkFShift_held > 0.5 ? 1.0 : gkGLITCH_chance * kFShiftScale)

 kmetr metro kBPS
 kdice trandom kmetr, 0, 1
 kGate init 0
 if (kmetr == 1) then
   kGate = (kdice < kChance ? 1 : 0)
 endif
 kGateSm portk kGate, iGateSmMs * 0.001
 aGate  interp kGateSm

 kfsqi randomi gkFShift_amount-2400, gkFShift_amount+2400, kBPS*2   ; 2026-06-23 expose FShift_amount (was iFSHIFTmin/max ±2400)
 kfsqh randomh gkFShift_amount-2400, gkFShift_amount+2400, kBPS*2   ; 2026-06-23 expose FShift_amount
 kx    randomi 0, 1, kBPS*2
 kfsq  ntrpol  kfsqi, kfsqh, kx
 aWetL STUTfreqshifter aL, kfsq, gisine
 aWetR STUTfreqshifter aR, kfsq, gisine

 aOutL = aL + aGate * (aWetL - aL)
 aOutR = aR + aGate * (aWetR - aR)

 gaFShift_wetL = aOutL * iGAIN
 gaFShift_wetR = aOutR * iGAIN
endin
