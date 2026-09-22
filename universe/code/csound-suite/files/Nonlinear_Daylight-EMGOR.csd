<CsoundSynthesizer>
<CsOptions>
-odac
</CsOptions>
<CsInstruments>
sr = 44100
ksmps = 256
nchnls = 2
0dbfs = 1

gasigL init 0
gasigR init 0

gkRoomSize init 0.98
gkHFDamp init 0.3
gkmix init 1.0

gkModRoomSize random 0.9, 1.0
gkModHFDamp random 0.25, 0.35

instr 100
    seed 0                            
    gkkeyoffset random -8, 2          
    gktempo     random 40, 65         
endin

instr 1
    ktick metro gktempo / 60
    kchance random 0, 1
    knoterand random 0, 26
    if (ktick == 1 && kchance < 0.65) then
        if (knoterand < 4) then
            knote = 48
        elseif (knoterand < 6) then
            knote = 50
        elseif (knoterand < 12) then
            knote = 52
        elseif (knoterand < 14) then
            knote = 53
        elseif (knoterand < 18) then
            knote = 55
        elseif (knoterand < 23) then
            knote = 57
        else
            knote = 59
        endif
        knote = knote + gkkeyoffset
        kratiorand random 0, 36
        if (kratiorand < 12) then
            kratio = 0
        elseif (kratiorand < 18) then
            kratio = 1
        elseif (kratiorand < 24) then
            kratio = 2
        elseif (kratiorand < 28) then
            kratio = 3
        elseif (kratiorand < 31) then
            kratio = 4
        elseif (kratiorand < 33) then
            kratio = 5
        elseif (kratiorand < 34) then
            kratio = 6
        elseif (kratiorand < 35) then
            kratio = 7
        else
            kratio = 8
        endif
        kindexrand random 0, 36
        if (kindexrand < 3) then
            kindex = 0
        elseif (kindexrand < 9) then
            kindex = 1
        elseif (kindexrand < 18) then
            kindex = 2
        elseif (kindexrand < 27) then
            kindex = 3
        else
            kindex = 4
        endif
        schedkwhen 1, 0, 0, 2, 0, 10, knote, kratio, kindex
    endif
endin

instr 2
    imidi = p4
    iratio = p5
    iindex = p6
    ifreq = cpsmidinn(imidi)
    iamp = 0.3
    kvibfreq = 0.3
    kvibdepth = 11 / 1200
    kvib oscil kvibdepth, kvibfreq
    kmodfreq = ifreq * (1 + kvib)
    iatt = 1.5
    idec = 5
    isus = 0.0
    irel = 5
    kenv adsr iatt, idec, isus, irel
    iFMatt random 0, 2.5
    iFMdec random 0, 2.5
    iFMsus random 0, 0
    iFMrel random 0, 2.5
    kFMenv adsr iFMatt, iFMdec, iFMsus, iFMrel
    amod oscili kmodfreq * iindex * kFMenv, kmodfreq * iratio
    acar oscili iamp * kenv, kmodfreq + amod
    kphasorLFO oscil 0.002, 0.05
    aphasorL comb acar, 0.2 + kphasorLFO, 0.005, 0.8
    aphasorR comb acar, 0.2 - kphasorLFO, 0.005, 0.8
    amixPhasorL = acar
    amixPhasorR = acar
    aflangerL comb amixPhasorL, 0.02, 0.005, 0.9
    aflangerR comb amixPhasorR, 0.02, 0.005, 0.9
    amixFlangerL = 0.8 * amixPhasorL + 0.2 * aflangerL
    amixFlangerR = 0.8 * amixPhasorR + 0.2 * aflangerR
    kchorusLFO oscil 0.002, 0.1
    achorusL vdelay amixFlangerL, 0.03 + kchorusLFO, 0.05
    achorusR vdelay amixFlangerR, 0.03 - kchorusLFO, 0.05
    amixChorusL = 0.8 * amixFlangerL + 0.2 * achorusL
    amixChorusR = 0.8 * amixFlangerR + 0.2 * achorusR
    gasigL = gasigL + amixChorusL
    gasigR = gasigR + amixChorusR
endin

gasigL init 0
gasigR init 0
gkRoomSize init 0.98
gkHFDamp init 0.3
gkmix init 1.0
gkLFO1freq init 0.03
gkLFO2freq init 0.15

kLFO1 oscil 0.02, 0.03
kLFO2 oscil 0.05, 0.15

instr 3
    kLFO1 oscil 0.02, gkLFO1freq
    kLFO2 oscil 0.05, gkLFO2freq
    kModRoomSize = gkRoomSize + kLFO1 * 0.05
    kModHFDamp = gkHFDamp + kLFO2 * 0.1
    kPreDelayL oscil 0.01, 0.15
    kPreDelayR oscil 0.01, 0.17
    adelayedL vdelay gasigL, 0.05 + kPreDelayL, 0.1
    adelayedR vdelay gasigR, 0.05 + kPreDelayR, 0.1
    acombL comb adelayedL, 0.2, 0.1, 0.7
    acombR comb adelayedR, 0.2, 0.1, 0.7
    arvbL, arvbR freeverb acombL, acombR, kModRoomSize, kModHFDamp
    outs arvbL*0.8, arvbR*0.8
    clear gasigL, gasigR
endin

</CsInstruments>
<CsScore>
i 100 0 1
i 1 0 z
i 3 0 z
</CsScore>
</CsoundSynthesizer>
























<bsbPanel>
 <label>Widgets</label>
 <objectName/>
 <x>100</x>
 <y>100</y>
 <width>320</width>
 <height>240</height>
 <visible>true</visible>
 <uuid/>
 <bgcolor mode="background">
  <r>240</r>
  <g>240</g>
  <b>240</b>
 </bgcolor>
</bsbPanel>
<bsbPresets>
</bsbPresets>
