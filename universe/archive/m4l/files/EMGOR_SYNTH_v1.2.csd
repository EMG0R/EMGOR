<Cabbage> bounds(0, 0, 0, 0)
form size(539, 220), caption("EMGOR_SYNTH"), pluginId("GOR1"), colour(30, 0, 45)
keyboard bounds(52, 140, 463, 68)

rslider  bounds(8, 150, 43, 44), text("offset"), channel("pitchOffset"), range(0, 16384, 8192, 1, 0.0005), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 195, 255, 255) 

rslider  bounds(18, 34, 43, 44), text("gain"), channel("gain"), range(0, 1, 0.5, 1, 0.0005), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 195, 255, 255) 
rslider  bounds(0, 86, 46, 44), text("attack"), channel("attack"), range(0.01, 3, 0.01, 1, 0.001), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 210, 255, 255)
rslider  bounds(42, 86, 46, 44), text("decay"), channel("decay"), range(0, 5, 0.2, 1, 0.001), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 210, 255, 255)
rslider  bounds(86, 86, 45, 44), text("sustain"), channel("sustain"), range(0, 1, 1, 1, 0.001), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 210, 255, 255)
rslider  bounds(130, 86, 46, 44), text("release"), channel("release"), range(0, 5, 0.1, 1, 0.001), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 210, 255, 255)
rslider  bounds(60, 34, 44, 44), text("cutoff"), channel("cutoff"), range(1, 20000, 20000, 1, 0.001), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 193, 255, 255)
rslider  bounds(102, 34, 45, 44), text("peak"), channel("peak"), range(0, 1, 0, 1, 0.001), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 204, 255, 255)
rslider  bounds(352, 34, 46, 45), text("rate"), channel("rate"), range(0, 100, 0, 1, 0.001), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 193, 255, 255)
rslider  bounds(398, 34, 44, 45), text("amount"), channel("amount"), range(0, 1, 1, 1, 0.001), , , $RSliderStyle textColour(255, 255, 255, 255) fontColour(255, 255, 255, 255) trackerColour(255, 210, 255, 255)

combobox bounds(230, 0, 100, 25), populate("*.snaps"), channel("combo1") channelType("string") value("0.0") automatable(0)
filebutton bounds(166, 0, 60, 25), text("Save", "Save"), populate("*.snaps"),mode("named preset") channel("filebutton1") 
filebutton bounds(332, 0, 60, 25), text("Remove", "Remove"), populate("*.snaps", "test"), mode("remove preset") channel("filebutton6")

checkbox bounds(372, 90, 60, 20) channel("pitchMOD") text("pitch") fontColour:1(255, 255, 255, 255) fontColour:0(255, 255, 255, 255), radioGroup("99") colour:1(255, 205, 255, 255) value(0)
checkbox bounds(372, 110, 57, 20) channel("ampMOD")  text("amp") fontColour:1(255, 255, 255, 255) fontColour:0(255, 255, 255, 255), radioGroup("99") colour:1(255, 211, 255, 255) value(1)
checkbox bounds(450, 46, 86, 19) channel("LFOsawDOWN") value(1) text("saw down") fontColour:1(255, 255, 255, 255) fontColour:0(255, 255, 255, 255), radioGroup("98") colour:1(255, 211, 255, 255)
checkbox bounds(450, 68, 73, 19) channel("LFOsawUP")  text("saw up") fontColour:1(255, 255, 255, 255) fontColour:0(255, 255, 255, 255), radioGroup("98") colour:1(255, 211, 255, 255)
checkbox bounds(450, 90, 71, 19) channel("LFOsquare")  text("square") fontColour:1(255, 255, 255, 255) fontColour:0(255, 255, 255, 255), radioGroup("98") colour:1(255, 211, 255, 255)



vslider bounds(170, 30, 25, 103) channel("partial1") range(0, 1, 0, 1, 0.001) trackerColour(255, 195, 255, 255)
vslider bounds(190, 30, 25, 103) channel("partial2") range(0, 0.5, 0, 1, 0.001) trackerColour(255, 195, 255, 255)
vslider bounds(210, 30, 25, 103) channel("partial3") range(0, 0.4, 0, 1, 0.001) trackerColour(255, 195, 255, 255)
vslider bounds(230, 30, 25, 103) channel("partial4") range(0, 0.3, 0, 1, 0.001) trackerColour(255, 195, 255, 255)
vslider bounds(250, 30, 25, 103) channel("partial5") range(0, 0.2, 0, 1, 0.001) trackerColour(255, 195, 255, 255)
vslider bounds(270, 30, 25, 103) channel("partial6") range(0, 0.1, 0, 1, 0.001) trackerColour(255, 195, 255, 255)
vslider bounds(290, 30, 25, 103) channel("partial7") range(0, 0.1, 0, 1, 0.001) trackerColour(255, 195, 255, 255)
vslider bounds(310, 30, 25, 103) channel("partial8") range(0, 0.1, 0, 1, 0.001) trackerColour(255, 195, 255, 255)
vslider bounds(330, 30, 25, 103) channel("partial9") range(0, 0.08, 0, 1, 0.001) trackerColour(255, 195, 255, 255)


</Cabbage>
<CsoundSynthesizer>
<CsOptions>
-n -d -+rtmidi=NULL -M0 --midi-key-cps=4 --midi-velocity-amp=5
</CsOptions>
<CsInstruments>
;sr is set by the host
ksmps = 64			;audio samples per control cycle
nchnls = 2			;number of channels
0dbfs=1


instr 1

iLFOshapee init 3



kGain chnget "gain"

kcutoff chnget "cutoff"
kres chnget "peak"

kAttack chnget "attack"
kDecay chnget "decay"
kSustain chnget "sustain"
kRelease chnget "release"

kLFOrate chnget "rate"
kLFOintens chnget "amount"
kampMOD chnget "ampMOD"
kpitchMOD chnget "pitchMOD"
kLFOsawDOWN chnget "LFOsawDOWN"
kLFOsawUP chnget "LFOsawUP"
kLFOsquare chnget "LFOsquare"

kPartial1 chnget "partial1"
kPartial2 chnget "partial2"
kPartial3 chnget "partial3"
kPartial4 chnget "partial4"
kPartial5 chnget "partial5"
kPartial6 chnget "partial6"
kPartial7 chnget "partial7"
kPartial8 chnget "partial8"
kPartial9 chnget "partial9"


pitchBendRange = 12  ; +/- semitones for pitch bend
kPitchBend chnget "pitchOffset"
kPitchBend = (kPitchBend - 8192) / 8192  ; Convert to range -1 to 1
kPitchBend = kPitchBend * pitchBendRange ; Apply pitch bend range
kFreq = p4 * pow(2, kPitchBend / 12)


iLFOsawDOWN = i(kLFOsawDOWN)
iLFOsawUP = i(kLFOsawUP)
iLFOsquare = i(kLFOsquare)



if iLFOsawDOWN == 1 then
    iLFOshapee = 5
elseif iLFOsawUP == 1 then
    iLFOshapee = 4
elseif iLFOsquare == 1 then
    iLFOshapee = 3
else
    iLFOshapee = 0  ; Default value when no checkboxes are active
endif


;convert k values to i values 
iAttack = i(kAttack)
iDecay = i(kDecay)
iSustain = i(kSustain)
iRelease = i(kRelease)

        
   
kEnv madsr iAttack, iDecay, iSustain, iRelease  
kLFO lfo kLFOintens, kLFOrate, iLFOshapee




if kampMOD == 1 then
    aVCO1 foscil kEnv/10, kFreq, 1, 2, kPartial1*4, 1
    aVCO2 foscil kEnv/10, kFreq, 1, 3, kPartial2*4, 1
    aVCO3 foscil kEnv/10, kFreq, 1, 4, kPartial3*4, 1
    aVCO4 foscil kEnv/10, kFreq, 1, 5, kPartial4*4, 1
    aVCO5 foscil kEnv/10, kFreq, 1, 6, kPartial5*4, 1
    aVCO6 foscil kEnv/10, kFreq, 1, 7, kPartial6*4, 1
    aVCO7 foscil kEnv/10, kFreq, 1, 8, kPartial7*4, 1
    aVCO8 foscil kEnv/10, kFreq, 1, 9, kPartial8*4, 1
    aVCO9 foscil kEnv/10, kFreq, 1, 10, kPartial9*4, 1    

    aLp moogladder (aVCO1+aVCO2+aVCO3+aVCO4+aVCO5+aVCO6+aVCO7+aVCO8+aVCO9)/3, kcutoff*kLFO, kres	
else
    aVCO1 foscil kEnv/10, (kFreq*kLFO) + kFreq*1.5, 1, 2, kPartial1*4, 1
    aVCO2 foscil kEnv/10, (kFreq*kLFO) + kFreq*1.5, 1, 3, kPartial2*4, 1
    aVCO3 foscil kEnv/10, (kFreq*kLFO) + kFreq*1.5, 1, 4, kPartial3*4, 1
    aVCO4 foscil kEnv/10, (kFreq*kLFO) + kFreq*1.5, 1, 5, kPartial4*4, 1
    aVCO5 foscil kEnv/10, (kFreq*kLFO) + kFreq*1.5, 1, 6, kPartial5*4, 1
    aVCO6 foscil kEnv/10, (kFreq*kLFO) + kFreq*1.5, 1, 7, kPartial6*4, 1
    aVCO7 foscil kEnv/10, (kFreq*kLFO) + kFreq*1.5, 1, 8, kPartial7*4, 1
    aVCO8 foscil kEnv/10, (kFreq*kLFO) + kFreq*1.5, 1, 9, kPartial8*4, 1
    aVCO9 foscil kEnv/10, (kFreq*kLFO) + kFreq*1.5, 1, 10, kPartial9*4, 1    

    aLp moogladder (aVCO1+aVCO2+aVCO3+aVCO4+aVCO5+aVCO6+aVCO7+aVCO8+aVCO9)/3, kcutoff*kLFO, kres	
		
endif

outs aLp*kGain, aLp*kGain			;outputs, multiplied by gain
endin


</CsInstruments>  
<CsScore>
f1 0 4096 10 1		;function table one.  
f0 3600				;tell Csound to wait for events for 3600 seconds
</CsScore>
