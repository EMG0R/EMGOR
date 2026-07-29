<Cabbage> bounds(0, 0, 0, 0)
form caption("EMGOR_GENERATIVE_SAMPLR") size(460, 166), guiMode("queue"), pluginId("def1") colour(0, 0, 0) bundle("./GSresources")
soundfiler bounds(10, 32, 355, 76) , channel("soundfiler1") colour(255, 239, 239, 255), outlineThickness(5), corners(10), outlineColour(0, 0, 0, 255) tableBackgroundColour(24, 23, 58, 255), file("samples/1")
combobox bounds(190, 0, 100, 25), populate("*.snaps"), channel("combo1") channelType("string") value("0.0") automatable(0)
filebutton bounds(128, 0, 60, 25), text("Save", "Save"), populate("*.snaps"),mode("named preset") channel("filebutton1") 
filebutton bounds(292, 0, 60, 25), text("Remove", "Remove"), populate("*.snaps", "test"), mode("remove preset") channel("filebutton6")

label bounds(88, 105, 197, 9) channel("label10055") text("(drag files onto each button and click to edit :0)") fontColour(255, 255, 255, 255)
;
</Cabbage>
<CsoundSynthesizer>
<CsOptions>
-n -d -+rtmidi=NULL -M0 -m0d --midi-key=4 --midi-velocity-amp=5
</CsOptions>
<CsInstruments>

;   get the full path of audio files
opcode getFullPath, S,S
    SFile xin
    xout sprintf("%s/%s", chnget:S("CSD_PATH"), SFile) 
endop

ksmps = 32 
nchnls = 2
0dbfs = 1


instr SampleSlot

    iSlotNumber = p4
    SFile sprintf getFullPath("GSresources/%d.wav"), (iSlotNumber + 1)

    
    iX = p5
    iY = p6
    iW = p7
    iH = p8
    
    SBKGCheckboxCode sprintf "bounds (%d, %d, 40, 40) channel(\"o%d\") corners(10) outlineColour(255, 255, 255, 255) colour:0(20, 60, 90, 255) colour:1(24, 23, 58, 255) value(0) active(0)", iX, iY, iSlotNumber
    SCheckboxCode sprintf "bounds (%d, %d, 40, 40) text(\" \")channel(\"sampleSlot%d\") corners(10) outlineColour(255, 255, 255, 255) colour:0(0, 0, 0, 0) colour:1(0, 0, 0, 0) value(0) active(1)", iX, iY, iSlotNumber
    SLabelCode sprintf "bounds(%d, %d, 15, 13), text(\"%d\") textColour(\"black\") channel(\"sampleSlotLabel%d\")", iX+3, iY+3, iSlotNumber, iSlotNumber
    SFreqCode sprintf "bounds(410, 26, 47, 42) text(\"freak\") channel(\"sampleSlotFreq%d\") range(0, 2, 1, 1, 0.001) trackerColour(186, 235, 255, 255) visible(1)", iSlotNumber
    SProbCode sprintf "bounds(367, 26, 47, 42) text(\"chance\") channel(\"sampleSlotProb%d\") range(0, 100, 100, 1, 1.0) trackerColour(186, 235, 255, 255) visible(1)", iSlotNumber
    SVolCode sprintf "bounds(367, 71, 47, 42) text(\"vol\") channel(\"sampleSlotVol%d\") range(0, 150, 100, 1, 0.001) trackerColour(186, 235, 255, 255) visible(1)", iSlotNumber
    SPanCode sprintf "bounds(410, 71, 47, 42) text(\"pan\") channel(\"sampleSlotPan%d\") range(0, 1, 0.5, 1, 0.001) trackerColour(186, 235, 255, 255) visible(1)", iSlotNumber



    cabbageCreate "checkbox", SBKGCheckboxCode
    cabbageCreate "label", SCheckboxCode
    cabbageCreate "label", SLabelCode
    cabbageCreate "rslider", SFreqCode
    cabbageCreate "rslider", SProbCode
    cabbageCreate "rslider", SVolCode
    cabbageCreate "rslider", SPanCode
    
  
     
    SFileChannel sprintf "sampleSlot%d_file", iSlotNumber
    SChannel sprintf "sampleSlot%d", iSlotNumber
    SPlayChannel sprintf "sampleSlot%d", iSlotNumber
    SLabelChannel sprintf "sampleSlotLabel%d", iSlotNumber
    kBounds[] cabbageGet SChannel, "bounds"
    kX chnget "MOUSE_X"
    kY chnget "MOUSE_Y"
    
    kMouseDownImage, kImageTrig cabbageGetValue SChannel
    kMouseDownLabel, kLabelTrig cabbageGetValue SLabelChannel
    
    if kLabelTrig == 1 || kImageTrig == 1 then
        cabbageSet 1, "soundfiler1", "file", chnget:S(SFileChannel)
        event "i", "ShowSliderz", 0, .1, iSlotNumber
    endif
    
    
    SCurrentWidget, kWidgetChanged cabbageGet "CURRENT_WIDGET"
    SFile, kFileChanged cabbageGet "LAST_FILE_DROPPED"
    
    
    if kFileChanged == 1 then
        if kX > kBounds[0] && kX < kBounds[0]+kBounds[2] && kY > kBounds[1] && kY < kBounds[1]+kBounds[3] then
            cabbageSet kFileChanged, "soundfiler1", "file", SFile
            chnset SFile, SFileChannel
        endif
    endif
    
    
           
    kButton, kButtonTrig cabbageGetValue SPlayChannel
    if kButtonTrig == 1 then
         event "i", 1, 0, 1, 60 + iSlotNumber - 1
    endif   
        
    
    SVolChannel sprintf "sampleSlotVol%d", iSlotNumber
    SPanChannel sprintf "sampleSlotPan%d", iSlotNumber
 
    chnset SFile, SFileChannel  ; Save the file path to the channel

        
endin



instr ShowSliderz
    iSlotNumber = p4
    iCnt = 0
    while iCnt <= 16 do
    SFreqChannel sprintf "sampleSlotFreq%d", iCnt

        if iCnt == iSlotNumber then   
            cabbageSet SFreqChannel, "visible(1)"
        else
            cabbageSet SFreqChannel, "visible(0)"
        endif
        iCnt += 1 
    od
    
    
    iCnt2 = 0
    while iCnt2 <= 16 do
    SProbChannel sprintf "sampleSlotProb%d", iCnt2

        if iCnt2 == iSlotNumber then   
            cabbageSet SProbChannel, "visible(1)"
        else
            cabbageSet SProbChannel, "visible(0)"
        endif
        iCnt2 += 1 
    od
    
    
    iCnt3 = 0
    while iCnt3 <= 16 do
    SVolChannel sprintf "sampleSlotVol%d", iCnt3

        if iCnt3 == iSlotNumber then   
            cabbageSet SVolChannel, "visible(1)"
        else
            cabbageSet SVolChannel, "visible(0)"
        endif
        iCnt3 += 1 
    od
    
    iCnt4 = 0
    while iCnt4 <= 16 do
    SPanChannel sprintf "sampleSlotPan%d", iCnt4

        if iCnt4 == iSlotNumber then   
            cabbageSet SPanChannel, "visible(1)"
        else
            cabbageSet SPanChannel, "visible(0)"
        endif
        iCnt4 += 1 
    od
    
endin

instr 1

    SFileChannel sprintf "sampleSlot%d_file", (p4-60) +1
    SFreqChannel sprintf "sampleSlotFreq%d", (p4-60) +1
    SVolChannel sprintf "sampleSlotVol%d", (p4-60) +1    
    SPanChannel sprintf "sampleSlotPan%d", (p4-60) +1    
    SProbChannel sprintf "sampleSlotProb%d", (p4-60) +1
    SFile sprintf "GSresources/%d.wav", (p4-60) + 1
    chnset SFile, SFileChannel
    kProb chnget SProbChannel

    ; Store the random value in an instance variable when the note is triggered
    if metro(0.1) == 0 then
        iRand random 0, 100
        kDummyEnv madsr .01, 0, 1, filelen(chnget:S(SFileChannel))
        if iRand < kProb then
            a1, a2 diskin2 chnget:S(SFileChannel), chnget:i(SFreqChannel), 0, 0 
        else
            a1 = 0
            a2 = 0
        endif
    endif
   
    if chnget:i(SPanChannel) <= 0.5 then
        a1 = a1
        a2 = a2 * (chnget:i(SPanChannel))*2
    elseif chnget:i(SPanChannel) >= 0.5 then
        a1 = a1 *((chnget:i(SPanChannel))-1)*-1*2
        a2 = a2
    endif
    
    a1 = a1 * ((chnget:i(SVolChannel)) / 100)
    a2 = a2 * ((chnget:i(SVolChannel)) / 100)
    
    outs a1, a2
    
    k1 rms a1, 20
    k2 rms a2, 20

    cabbageSetValue "vu1", portk(k1*10, .25), metro(10)
    cabbageSetValue "vu2", portk(k2*10, .25), metro(10)
  
endin



</CsInstruments>
<CsScore>
;causes Csound to run for about 7000 years...
f0 z

i"SampleSlot" 0 z 1 10 118 
i"SampleSlot" 0 z 2 60 118 
i"SampleSlot" 0 z 3 110 118
i"SampleSlot" 0 z 4 160 118 
i"SampleSlot" 0 z 5 210 118
i"SampleSlot" 0 z 6 260 118 
i"SampleSlot" 0 z 7 310 118
i"SampleSlot" 0 z 8 360 118
i"SampleSlot" 0 z 9 410 118





</CsScore>
</CsoundSynthesizer>

