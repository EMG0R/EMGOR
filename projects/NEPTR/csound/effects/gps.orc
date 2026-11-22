; Granular Pitch Shifter
; Signal chain position: 55
; Reads: gaGPSSendL/R
; Writes: gaGPSWetL/R
; Bypass: N/A

instr 55
    ainL = gaGPSSendL * gkGPSInGain
    ainR = gaGPSSendR * gkGPSInGain

    ; writing part
    kUseFreeze = gkGPSfreeze
    ktrigger trigger gkGPSfreeze,0.5,1
    if ktrigger==1 then
        gkGPSManPtr = 0
    endif
    aFBackSig dcblock2 gaGPSFBackSig
    aFBackSigR dcblock2 gaGPSFBackSigR
    if gkGPSLPF_On==1 then
        aFBackSig tone aFBackSig,gkGPSCutoff
        aFBackSigR tone aFBackSigR,gkGPSCutoff
    endif
    ktrig changed gkGPSClipLev
    if gkGPSclip==1 then
        if ktrig==1 then
            reinit UPDATE_CLIP_L
        endif
        UPDATE_CLIP_L:
        aFBackSig clip aFBackSig, 0, 0dbfs*i(gkGPSClipLev)
        aFBackSigR clip aFBackSigR, 0, 0dbfs*i(gkGPSClipLev)
        if ktrig==1 then
            rireturn
        endif
    endif
    gaphsW phasor (sr*(1-kUseFreeze))/giGPSnnsamp
    if kUseFreeze==0 then
        tablew ainL + aFBackSig,gaphsW,giGPSbuffer,1,0,1
        tablew ainR + aFBackSigR,gaphsW,giGPSbufferR,1,0,1
    endif
    clear gaGPSFBackSig,gaGPSFBackSigR

    ; reading part
    imode = 1
    ibeg = 0
    iwsize = i(gkGPSwsize)
    irnd = i(gkGPSrnd)
    iolap = i(gkGPSolap)
    iwfn = giGPSwfn1 + i(gkGPSwfn) -1
    iMaxDur = (iwsize + irnd)/sr
    kporttime linseg 0,0.001,0.03
    kGPSsemisC limit gkGPSsemis, gkGPS_min, gkGPS_max
    kpch = semitone(kGPSsemisC)
    kpch portk kpch,kporttime
    apch interp kpch
    kManPtr portk gkGPSManPtr, kporttime*10*gkGPSfreeze*gkGPSManPtrPort
    kRndDly betarand gkGPSdly,1,gkGPSbeta
    kRndDlyR betarand gkGPSdly,1,gkGPSbeta
    kTransComp limit iMaxDur*(kpch-1),0,giGPSnnsamp/sr
    kdelay = (kTransComp+kRndDly) / (giGPSnnsamp/sr)
    kdelayR = (kTransComp+kRndDlyR) / (giGPSnnsamp/sr)
    if gkGPSfreeze==1 then
        kdelay = kdelay + (sr/(giGPSnnsamp)*1.75*iwsize/sr)
        kdelayR = kdelayR + (sr/(giGPSnnsamp)*1.75*iwsize/sr)
    endif
    aphsR wrap (gaphsW-kdelay+kManPtr)*(giGPSnnsamp/sr),0,(giGPSnnsamp-iwsize-irnd)/sr
    aphsR_R wrap (gaphsW-kdelayR+kManPtr)*(giGPSnnsamp/sr),0,(giGPSnnsamp-iwsize-irnd)/sr
    asig,ac sndwarp 1, aphsR, apch, giGPSbuffer, ibeg, iwsize, irnd, iolap, iwfn, imode
    if gkGPSbalance==1 then
        asig balance asig,ac
    endif
    aR,acR sndwarp 1, aphsR_R, apch, giGPSbufferR, ibeg, iwsize, irnd, iolap, iwfn, imode
    if gkGPSbalance==1 then
        aR balance aR,acR
    endif
    gaGPSFBackSig = gaGPSFBackSig+(asig*gkGPSfback)
    gaGPSFBackSigR = gaGPSFBackSigR+(aR*gkGPSfback)
    gaGPSWetL = asig * gkGPSamp
    gaGPSWetR = aR * gkGPSamp
endin
