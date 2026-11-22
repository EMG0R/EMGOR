; Bass preamps — Hudson-Broadcast-derived but BASS-voiced.
; Key difference from the guitar Preamp: the low band is split off and kept
; CLEAN (parallel); only the mids/highs get the envelope-driven germanium drive,
; so the fundamentals stay tight instead of farting out. Radio pair (mutual
; exclusion handled in midi_handler): LIGHT = clean DI warmth, HEAVY = crunch.
;   instr 160 = LIGHT (gkBPL_*)   instr 161 = HEAVY (gkBPH_*)
; FX_BYPASS-launched; reads gaBP{L,H}Send{L,R}, writes gaBP{L,H}Out{L,R}.
; NOTE: drive/voicing coefficients are first-pass — tune on-ear on the rig.

opcode BassPreChan, a, akk
    ain, kEnv, kDrive xin
    aSi  dcblock ain
    aSi  =  butterhp(aSi, 18)             ; keep sub fundamentals
    aLo  butterlp aSi, 160                ; clean low band (parallel)
    aHi  =  aSi - aLo
    ; envelope-driven asymmetric germanium clip on the HIGH band only.
    kBias =  0.12 + kEnv * 0.28
    aBias interp kBias
    kIn  =  1.6 + kDrive * 5.5            ; wider drive range (was 1.4+4.5)
    kGe  =  1.5 + kDrive * 4.0            ; more germanium character (was 1.3+3.2)
    aHiGe = (tanh((aHi*kIn + aBias) * kGe) - tanh(aBias*kGe)) / (kIn*kGe)
    aX   =  aLo + aHiGe                   ; clean lows + driven highs
    aX   tone  aX, 5500                   ; tame fizz (was 11000 — much more effective)
    aX   dcblock aX
    xout aX
endop

instr 160  ; BASS PREAMP LIGHT
    aL  =  gaBPLSendL
    aR  =  gaBPLSendR
    aL  dcblock2 aL
    aR  dcblock2 aR
    kDrv portk gkBPL_drive, 0.02
    aEnv follow2 (aL + aR) * 0.5, 0.005, 0.12
    kEnv downsamp aEnv
    kEnv portk   kEnv, 0.008
    kEnv limit   kEnv, 0, 1
    aoL BassPreChan aL, kEnv, kDrv
    aoR BassPreChan aR, kEnv, kDrv
    gaBPLOutL = tanh(aoL * 1.4) * 0.99
    gaBPLOutR = tanh(aoR * 1.4) * 0.99
endin

instr 161  ; BASS PREAMP HEAVY
    aL  =  gaBPHSendL
    aR  =  gaBPHSendR
    aL  dcblock2 aL
    aR  dcblock2 aR
    kDrv portk gkBPH_drive, 0.02
    aEnv follow2 (aL + aR) * 0.5, 0.005, 0.12
    kEnv downsamp aEnv
    kEnv portk   kEnv, 0.008
    kEnv limit   kEnv, 0, 1
    aoL BassPreChan aL, kEnv, kDrv
    aoR BassPreChan aR, kEnv, kDrv
    gaBPHOutL = tanh(aoL * 1.7) * 0.99
    gaBPHOutR = tanh(aoR * 1.7) * 0.99
endin
