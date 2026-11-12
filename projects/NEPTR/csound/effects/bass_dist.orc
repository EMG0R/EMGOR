; Bass distortion — bass-optimized dynamic clipper (instr 162, toggle 42).
; Splits at ~120 Hz: the low band stays clean and is blended back in
; (gkBDS_blend) so the fundamentals stay solid under heavy clipping; the upper
; band gets upper-mid emphasis pre-clip then an asymmetric tanh, so it grinds
; without turning to mush. gkBDS_tone sweeps the post low-pass.
; FX_BYPASS-launched; reads gaBDSSend{L,R}, writes gaBDSOut{L,R}.
; NOTE: voicing coefficients are first-pass — tune on-ear on the rig.

opcode BassDistChan, a, akkk
    ain, kGain, kBlend, kTone xin
    aLo  butterlp ain, 150                ; clean low band (wider split for bigger punch)
    aHi  =  ain - aLo
    aHi  pareq aHi, 1500, 2.2, 0.7, 0    ; strong upper-mid emphasis pre-clip
    aDrv =  aHi * kGain
    aClip = (tanh(aDrv + 0.12) - tanh(0.12))  ; asymmetric clip, less offset = more grind
    kCut =  1500 + kTone * 4500           ; post tone LP 1.5k..6k
    aClip tone aClip, kCut
    aOut =  aLo * (0.5 + kBlend * 0.5) + aClip * 0.85   ; more distorted signal
    xout aOut
endop

instr 162  ; BASS DISTORTION
    aL  =  gaBDSSendL
    aR  =  gaBDSSendR
    aL  dcblock2 aL
    aR  dcblock2 aR
    kDrv  portk gkBDS_drive, 0.02
    kBlnd portk gkBDS_blend, 0.02
    kTone portk gkBDS_tone, 0.02
    kGain =  2 + kDrv * 20               ; was 1+9 (max 10) → now max 22 — real bite
    aoL BassDistChan aL, kGain, kBlnd, kTone
    aoR BassDistChan aR, kGain, kBlnd, kTone
    gaBDSOutL = tanh(aoL) * 0.99
    gaBDSOutR = tanh(aoR) * 0.99
endin
