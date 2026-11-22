; Chorus
; Signal chain position: 40
; Reads: gaChorusSendL/R
; Writes: gaChorusWetL/R
; Bypass: gkChorusMix

instr 40
    aInL = gaChorusSendL
    aInR = gaChorusSendR
    kporttime linseg 0,0.001,0.05
    krate = gkChorusRate
    kdereg = gkChorusDereg
    kdepth = gkChorusDepth
    koffset = gkChorusOffset
    kwidth = gkChorusWidth
    kmix = gkChorusMix
    ktype = gkChorusType
    aoffset interp koffset
    if ktype==1 then
        kdereg rspline -kdereg, kdereg, 0.1, 0.5
        ktrem rspline 0,-1,0.1,0.5
        ktrem pow 2,ktrem
        aOutL, aOutR StChorus aInL, aInR, krate*octave(kdereg), kdepth*ktrem, aoffset, kwidth, kmix
    else
        aOutL, aOutR StChorusRspline aInL, aInR, krate, kdereg, kdepth, koffset, kwidth, kmix
    endif
    gaChorusWetL = aOutL * 2
    gaChorusWetR = aOutR * 2
endin
