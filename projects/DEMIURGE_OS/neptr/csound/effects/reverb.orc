; Reverb
; Signal chain position: 70
; Reads: gaVerbSendL/R
; Writes: gaVerbWetL/R
; Bypass: N/A

instr 70
    kFB      = gkReverbFB
    iPitchM  = 1
    kDamp = gkReverbDamp                ; 2026-06-23 expose ReverbDamp (was iDampCtl=0.9)
    kDamp expcurve kDamp, 4
    kDampHz scale kDamp, 20000, 20
    aInL = gaVerbSendL
    aInR = gaVerbSendR
    denorm aInL, aInR
    aWetL, aWetR reverbsc aInL, aInR, kFB, kDampHz, sr, i(iPitchM)
    gaVerbWetL = aWetL
    gaVerbWetR = aWetR
endin
