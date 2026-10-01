
;define UI ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
<Cabbage> bounds(0, 0, 0, 0)

;basic ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
form caption("EMGOR_FX") size(250, 470), colour("black"), guiMode("queue") pluginId("def1") bundle("EMGOR_EFFECTz_v3.snaps")
#define RSLIDER_ATTRIBUTES trackerColour(255, 255, 255, 255) textColour(255, 255, 255, 255)

;backgrounds ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
button bounds(13, 26, 225, 62) channel("backgrn6") outlineThickness(2), corners(0), outlineColour(255, 255, 255, 255) text("", "") active(0) colour:0(115, 0, 0, 255)
button bounds(13, 88, 225, 62) channel("backgrn1") outlineThickness(2), corners(0), outlineColour(255, 255, 255, 255) text("", "") active(0) colour:0(115, 46, 0, 255)
button bounds(13, 150, 225, 62) channel("backgrn2") outlineThickness(2), corners(0), outlineColour(255, 255, 255, 255) text("", "") active(0) colour:0(115, 104, 0, 255)
button bounds(6, 212, 118, 62) channel("backgrn3") outlineThickness(2), corners(0), outlineColour(255, 255, 255, 255) text("", "") active(0) colour:0(2, 115, 0, 255)
button bounds(123, 212, 121, 62) channel("backgrn4") outlineThickness(2), corners(0), outlineColour(255, 255, 255, 255) text("", "") active(0) colour:0(2, 115, 0, 255)
button bounds(13, 274, 227, 64) channel("backgrn5") outlineThickness(2), corners(0), outlineColour(255, 255, 255, 255) text("", "") active(0) colour:0(0, 95, 115, 255)
button bounds(36, 336, 173, 62) channel("backgrn8") outlineThickness(2), corners(0), outlineColour(255, 255, 255, 255) text("", "") active(0) colour:0(0, 11, 115, 255)
button bounds(36, 398, 173, 61) channel("backgrn7") outlineThickness(2), corners(0), outlineColour(255, 255, 255, 255) text("", "") active(0) colour:0(61, 0, 115, 255)

;presets --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
filebutton bounds(23, 0, 42, 25), text("Save", "Save"), populate("*.snaps"),mode("named preset") channel("filebutton1") file("/Users/emgor/Desktop/csound/_ME/fx/0.0")
filebutton bounds(186, 0, 38, 25), text("Del", "Del"), populate("*.snaps", "test"), mode("remove preset") channel("filebutton6") file("/Users/emgor/Desktop/csound/_ME/fx/0.0")
combobox bounds(62, 0, 126, 25), populate("*.snaps"), channel("combo1") channelType("string") value("0.0") automatable(0) text("dry", "chorus", "ensemble", "stepr", "stepr noise", "hello robot", "light delay", "slapback delay", "tremelo", "amplitude modulation", "am bitcrush", "crazy sauce")

;labels --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
label bounds(14, 44, 26, 19) channel("i6") text("S") fontColour(255, 255, 255, 255)
label bounds(16, 106, 23, 20) channel("i1") text("O") fontColour(255, 255, 255, 255)
label bounds(8, 228, 20, 34) channel("i2") text("*") fontColour(255, 255, 255, 255)
label bounds(14, 166, 26, 19) channel("i3") text("<") fontColour(255, 255, 255, 255)
label bounds(124, 230, 26, 19) channel("i3.5") text("|!") fontColour(255, 255, 255, 255)
label bounds(34, 418, 30, 17) channel("i5") text("U") fontColour(255, 255, 255, 255)
label bounds(12, 294, 30, 17) channel("i4") text("W") fontColour(255, 255, 255, 255)
label bounds(34, 356, 30, 17) channel("i8") text("Z") fontColour(255, 255, 255, 255)

;utility --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
rslider bounds(154, 404, 50, 50), channel("dry/wet"), range(0, 1, 1, 1, 0.001), text("dry/wet"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(106, 404, 50, 50), channel("gain"), range(0, 0.6, 0.3, 1, 0.001), text("Gain"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(56, 404, 50, 50), channel("width"), range(0, 0.3, 0, 1, 0.0001), text("width"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)

;phaser --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
rslider   bounds(36, 32, 50, 50), channel("ratePH"), range(0, 25, 0, 1, 0.0001), text("rate"),, $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider   bounds(84, 32, 50, 50), text("fback"), channel("feedbackPH"), range(-0.99, 0.99, 0, 1, 0.001), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider   bounds(134, 32, 50, 50), text("n.ords."), channel("ordPH"), range(1, 256, 32, 0.5, 1), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider   bounds(182, 32, 50, 50), text("depth"), channel("depthPH"), range(0, 1, 0, 1, 0.001), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)

;chorus --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
rslider bounds(182, 94, 50, 50), channel("depthCHOR"), range(0, 2, 0, 1, 0.001), text("depth"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(36, 94, 50, 50), channel("rateCHOR"), range(0, 25, 0, 1, 0.0001), text("rate"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider  bounds(134, 94, 50, 50), text("Dereg"), channel("deregCHOR"), range(0, 4, 0, 0.5, 0.01), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider  bounds(84, 94, 50, 50), text("Offset"), channel("offsetCHOR"), range(0.0001, 0.1, 0.001, 0.5, 0.0001), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)

;flanger --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
rslider bounds(36, 156, 50, 50), channel("rate2"), range(0.001, 40, 0.15, 0.5, 0.001), text("rate"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(184, 156, 50, 50), channel("depth2"), range(0, 0.01, 0, 1, 0.0001), text("depth"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(136, 156, 47, 50), channel("delayFLA"), range(2e-05, 0.1, 2e-05, 0.5, 0.0001), text("delay"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(84, 156, 50, 50), channel("fbackFLA"), range(-1, 1, 0, 1, 0.001), text("fback"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)

;vdelayx --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
rslider bounds(72, 218, 50, 50), channel("depth3"), range(0, 50, 0, 1, 0.0001), text("depth"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(24, 218, 50, 50), channel("rate3"), range(0, 10, 0, 1, 0.0001), text("rate"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)

;20 pole delay --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
rslider bounds(144, 218, 50, 50), channel("delayTime4"), range(0, 500, 0, 1, 0.001), text("time"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(190, 218, 53, 50), channel("feedback4"), range(0, 1000, 0, 1, 0.0001), text("fback"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)

;amplitude/frequency modulation modulation --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------                  
rslider bounds(84, 280, 50, 50), channel("AModDepth"), range(0, 100, 0, 1, 0.0001), text("amp"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(134, 280, 50, 50), channel("RMdepth"), range(0, 100, 0, 1, 0.0001), text("freak"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)
rslider bounds(34, 280, 50, 50), channel("ARModFreq"), range(0, 200, 0, 1, 0.0001), text("rate"), $RSLIDER_ATTRIBUTES textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)

;waveshaping --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------                  
rslider bounds(56, 342, 50, 50), text("bits"),     channel("bits"),  range(1, 32, 32, 1, 0.001), trackerColour(255, 255, 255, 255) textColour(255, 255, 255, 255)
rslider bounds(104, 342, 50, 50), text("foldover"), channel("fold"),  range(0, 1024, 0, 0.25, 0.001) trackerColour(255, 255, 255, 255) textColour(255, 255, 255, 255)
rslider  bounds(154, 342, 50, 50), text("power"),  channel("power"),  range(0, 1, 0, 1, 0.001), trackerColour(255, 255, 255, 255) textColour(255, 255, 255, 255)

;pitchShifr --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------                  
combobox bounds(98, 28, 21, 24), text("Semitone", "Ratio"), channel("IntervalModePS")       visible(0)
image     bounds(68, 26, 23, 27), plant("Ratio"), colour(0, 0, 0, 0), visible(0) { channel("image34PS")
nslider bounds(3, 0, 16, 11), channel("Numerator"),        range(1, 99, 3, 1, 1)
image     bounds(0, 13, 23, 0), channel("image36")
image    bounds(32, 10, 134, 70), plant("Semitones"), colour(0, 0, 0, 0), channel("image40PS")
rslider  bounds(182, 280, 55, 51), text("offset"),  channel("SemitonesPS"),      range(-48, 48, 7.62939e-06, 1, 0.001), textColour(255, 255, 255, 255) trackerColour(255, 255, 255, 255)}
combobox  bounds(124, 28, 27, 21), channel("ModePS"),       value(3) visible(0)
combobox bounds(154, 30, 21, 20), text("Indiv.","Global"),       channel("FBMethodPS"), visible(0)
combobox bounds(176, 30, 22, 21), text("Hanning","Triangle","Half Sine","Square","Pulse","Perc.","Rev.Perc."),  channel("WindowPS") value(3) visible(0)
 


</Cabbage>
<CsoundSynthesizer>
<CsOptions>
-n -d -+rtmidi=NULL -M0 -m0d 
</CsOptions>
<CsInstruments>
ksmps = 32
nchnls = 2
0dbfs = 1







;flanger op ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
opcode    Flanger,a,akkkkk
    ain,kRateFLA,kDepthFLA,kDelayFLA,kFbackFLA,klfoshapeFLA    xin           
    iparabola    ftgentmp    0, 0, 131072, 19, 0.5, 1, 180, 1   
    isine        ftgentmp    0, 0, 131072, 19, 1, 0.5, 0,   0.5     
    itriangle    ftgentmp    0, 0, 131072, 7, 0,131072/2,1,131072/2,0
    adlt        interp        kDelayFLA                           
    if klfoshapeFLA==1 then
     amod        oscili        kDepthFLA, kRateFLA, iparabola           
    elseif klfoshapeFLA==2 then
     amod        oscili        kDepthFLA, kRateFLA, isine           
    elseif klfoshapeFLA==3 then
     amod        oscili        kDepthFLA, kRateFLA, itriangle           
    elseif klfoshapeFLA==4 then    
     amod        randomi        0,kDepthFLA,kRateFLA,1
    else    
     amod        randomh        0,kDepthFLA,kRateFLA,1
    endif
    adlt        sum        adlt, amod                
    adelsig        flanger     ain, adlt, kFbackFLA , 1.2            
    adelsig        dcblock        adelsig
    aout        sum        ain*0.5, adelsig*0.5           
            xout        aout                   
endop








;chorus op ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
opcode    StChorus,aa,aakkakk
    ainL,ainR,krateCHOR,kdepthCHOR,aoffsetCHOR,kwidthCHOR,kmixCHOR    xin           
    ilfoshape    ftgentmp    0, 0, 131072, 19, 1, 0.5, 0,  0.5   
    kporttime    linseg    0,0.001,0.02                   
    kChoDepth    portk    kdepthCHOR*0.01, kporttime              
    aChoDepth    interp    kChoDepth                   
    amodL         osciliktp     krateCHOR, ilfoshape, 0            
    amodR         osciliktp     krateCHOR, ilfoshape, kwidthCHOR*0.5        
    amodL        =        (amodL*aChoDepth)+aoffsetCHOR           
    amodR        =        (amodR*aChoDepth)+aoffsetCHOR            
    aChoL        vdelay    ainL, amodL*1000, 1.2*1000            
    aChoR        vdelay    ainR, amodR*1000, 1.2*1000            
    aoutL        ntrpol     ainL*0.6, aChoL*0.6, kmixCHOR            
    aoutR        ntrpol     ainR*0.6, aChoR*0.6, kmixCHOR           
            xout    aoutL,aoutR                  
endop





gisine        ftgen    0,0,4096,10,1            ;A SINE WAVE SHAPE
gicos        ftgen    0,0,4096,11,1            ;A COSINE WAVE SHAPE
gishapes    ftgen    0,0,8,-2,0,1,2,4,5







;waveshaping op ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
opcode  LoFi,a,akk
    ain,kbits,kfold xin                                 
    kvalues pow     2, kbits                            
    aout    =       (int((ain/0dbfs)*kvalues))/kvalues  
    aout    fold    aout, kfold                         
            xout    aout                                
endop




;pitchshiftr ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
giHanning    ftgen    0, 0, 4097,  20, 2
giTriangle   ftgen    0, 0, 4097,  20, 3
giHalfSine   ftgen    0, 0, 4097,   9, 0.5, 1, 0
giSquare     ftgen    0, 0, 4097,   7, 0, 64, 1, 4096-128, 1, 64, 0
giPulse      ftgen    0, 0, 4097,  20, 6, 1, 0.5
giPerc       ftgen    0, 0, 4097,  16, 0, 64, -2, 1,   4096-64, -2, 0
giRevPerc    ftgen    0, 0, 4097,  16, 0, 4096-64, -2, 1, 64, -2, 0

opcode    pitchshifter4, aa, aakikkiip    
    ainL,ainR,kratio,iNIter,kDelay,kSmooth,imaxdelay,iwfn,iCount    xin
    setksmps    1

    kratio    =    kratio * (iCount+1)/iCount
    iratio    =    i(kratio) * (iCount+1)/iCount

    kPortTime    linseg    0,0.001,1
    if kSmooth>0 then                    ; portamento smoothing
     kratio        portk    kratio, kPortTime*kSmooth    
     kDelay        portk    kDelay, kPortTime*kSmooth    
    endif

    aDelay        interp    kDelay
 
    arate        =    (kratio-1)/kDelay        ;SUBTRACT 1/1 SPEED
    aphase1        phasor    -arate                ;MOVING PHASE 1-0
    aphase2        phasor    -arate, .5            ;MOVING PHASE 1-0 - PHASE OFFSET BY 180 DEGREES (.5 RADIANS)
    
    agate1        tablei    aphase1, iwfn, 1, 0, 1        ;
    agate2        tablei    aphase2, iwfn, 1, 0, 1        ;

    aGatedMixL,aGatedMixR    init    0
    
    abuf1        delayr    imaxdelay            ;DECLARE DELAY BUFFER
    adelsig1    deltap3    aphase1 * aDelay        ;VARIABLE TAP
    aGatedSig1    =    adelsig1 * agate1
            delayw    ainL                ;WRITE AUDIO TO THE BEGINNING OF THE DELAY BUFFER, MIX IN FEEDBACK SIGNAL - PROPORTION DEFINED BY gkFB
    
    abuf2        delayr    imaxdelay            ;DECLARE DELAY BUFFER
    adelsig2    deltap3    aphase2 * aDelay        ;VARIABLE TAP
    aGatedSig2    =    adelsig2 * agate2
            delayw    ainL                ;WRITE AUDIO TO THE BEGINNING OF THE DELAY BUFFER, MIX IN FEEDBACK SIGNAL - PROPORTION DEFINED BY gkFB

    abuf3        delayr    imaxdelay            ;DECLARE DELAY BUFFER
    adelsig3    deltap3    aphase1 * aDelay        ;VARIABLE TAP
    aGatedSig3    =    adelsig3 * agate1
            delayw    ainR                ;WRITE AUDIO TO THE BEGINNING OF THE DELAY BUFFER, MIX IN FEEDBACK SIGNAL - PROPORTION DEFINED BY gkFB
    
    abuf4        delayr    imaxdelay            ;DECLARE DELAY BUFFER
    adelsig4    deltap3    aphase2 * aDelay        ;VARIABLE TAP
    aGatedSig4    =    adelsig4 * agate2
            delayw    ainR                ;WRITE AUDIO TO THE BEGINNING OF THE DELAY BUFFER, MIX IN FEEDBACK SIGNAL - PROPORTION DEFINED BY gkFB
            
    aGatedMixL    =    (aGatedSig1 + aGatedSig2) * 0.5
    aGatedMixR    =    (aGatedSig3 + aGatedSig4) * 0.5
    
    aMixL,aMixR    init    0
    if iCount<iNIter then
     aMixL,aMixR    pitchshifter4    ainL,ainR,kratio,iNIter,kDelay,kSmooth,imaxdelay,iwfn,iCount+1
    endif
    
            xout    aGatedMixL + aMixL, aGatedMixR + aMixR
endop





;defining variables ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
instr 1  
a1 inch 1
a2 inch 2

ifullscale = 0dbfs
kGain chnget "gain"
kdrywet chnget "dry/wet"
kWidth chnget "width"
kporttime    linseg    0,0.001,0.1

;phaser --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
kRatePH        chnget    "ratePH"                   
kFeedbackPH    chnget    "feedbackPH"
kordPH        chnget    "ordPH"
kDepthPH        chnget    "depthPH"       

;chorus --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
krateCHOR chnget "rateCHOR"
kderegCHOR chnget "deregCHOR"
kdepthCHOR chnget "depthCHOR"
koffsetCHOR chnget "offsetCHOR"
kwidthCHOR init 0.8
klevelCHOR chnget "levelCHOR"
kmixCHOR      chnget "depthCHOR"
ktypeCHOR    init    1
kmixCHOR    portk    kmixCHOR,kporttime
kleveCHOR     portk    klevelCHOR,kporttime
koffsetCHOR    portk    koffsetCHOR,kporttime*0.5
aoffsetCHOR    interp    koffsetCHOR
kderegCHOR    rspline    -kderegCHOR, kderegCHOR, 0.1, 0.5
ktrem    rspline    0,-1,0.1,0.5
ktrem    pow    2,ktrem

;flanger --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
kDelayFLA chnget "delayFLA"
kDelayFLA    portk    kDelayFLA,kporttime
kFbackFLA chnget "fbackFLA"
klfoshapeFLA init 2
kThruZeroFLA init 0
kDepthFLA chnget "depth2"
kRateFLA chnget "rate2" 

;vdelayx --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
kDepth3 chnget "depth3"
kRate3 chnget "rate3" 
kWindowSize4 chnget "windowSize4"
iWindowSize3 = 20
iWindowSize4 = i(kWindowSize4)

;20 pole delay --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
kDelayTime4 chnget "delayTime4"
kFeedback4p chnget "feedback4"
kFeedback4    portk    kFeedback4p, kporttime           
kDepth4 = 1
iFeedback4 = i(kFeedback4)
kFeedbackScale = kFeedback4 / 1000
kDecayFactor = 1 - (kFeedback4/1000) 

;amplitude/frequency modulation --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
kARModFreq chnget "ARModFreq"
iARModFreq = i(kARModFreq)
kAModDepth chnget "AModDepth"
kRMdepth chnget "RMdepth"
aARModFreq = a(kARModFreq)
aAModDepth = a(kAModDepth)

;waveshaping --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
kbits     chnget  "bits"
kfold     init 1025  
kfold     chnget  "fold"
kfold     portk   kfold, (kporttime/3)
kshape        chnget      "power"
kshape = 1 - kshape
kshape        portk       kshape, (kporttime/3)

;pitchShiftn --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
aPsnL = a1
aPsnR = a2
kfeedbackPS    = 0
kNIterPS        init    1
kDelayPS        = 0.01
kSmoothPS        chnget    "SmoothPS"
kModePS        chnget    "ModePS"
kIntervalModePS    chnget    "IntervalModePS"
iMaxDelayPS    =    4
kWindowPS        chnget    "WindowPS"
kWindowPS        init    1 
kSemitonezReal    chnget    "SemitonesPS"
kSemitones = kSemitonezReal-0.00000762938998
kSnap        chnget    "SnapPS"
kRatio    =    semitone(kSemitones) 
        
;lfos --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
kLFOPH lfo kDepthPH, kRatePH
kLFOPHnorm = (kLFOPH + 1) / 2
kLFOPHscaled = 20 + (kLFOPHnorm * (5000 - 20))
aLFOPH = a((kLFOPH)*100)

aLFO1 lfo (kdepthCHOR*50), krateCHOR
aLFO2 lfo (kDepthFLA*10000) , kRateFLA
aLFO3 lfo kDepth3, kRate3

aAModLFO lfo kAModDepth, kARModFreq
aAModLFOdub lfo kAModDepth, (2*kARModFreq)

aRMosc lfo (kRMdepth/100), ((kARModFreq)/2)
kRMosc = k(aRMosc)

aLFOall = (aLFO1 + aLFO2 + aLFO3 + aAModLFOdub + aLFOPH + (aRMosc*200)) / 5
kLFOall = k(aLFOall)

;pitchShiftn --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
if kSemitones == 0 then
    aPS_L = a1
    aPS_R = a2
else
    if changed(kWindowPS)==1 then
    endif
    iWfnPS    =    giHanning + i(kWindowPS) - 1
    aPS_L,aPS_R    pitchshifter4    aPsnL,aPsnR,((semitone(kSemitones-12))*kRMosc)+(semitone(kSemitones-12)),i(kNIterPS),kDelayPS,kSmoothPS,iMaxDelayPS,iWfnPS    
    rireturn
endif

;phasor --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
kSwitch        changed    kordPH                    
if    kSwitch=1    then                  
    reinit    UPDATE                       
endif 

UPDATE:                               
aphaserL    phaser1    aPS_L, kLFOPHscaled, kordPH, kFeedbackPH   
aphaserR    phaser1    aPS_R, kLFOPHscaled, kordPH, kFeedbackPH    
rireturn                           

aPhaseLout        ntrpol    aPS_L,aphaserL,kDepthPH
aPhaseRout        ntrpol    aPS_R,aphaserR,kDepthPH






;chourus --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
giPow3  ftgen 1,0,2048,10,1           
iftlen =       ftlen(giPow3)          
icount    =    0             
loop3:                                                            
ix    =    ((icount/iftlen) * 2) -1                                
iy    =    ix ^ 3                                                         
    tableiw iy,icount,giPow3                                               
loop_lt,icount,1,iftlen,loop3                 
             
aCHORoutL,aCHORoutR    StChorus    aPS_L,aPS_R,krateCHOR*octave(kderegCHOR),kdepthCHOR*ktrem,aoffsetCHOR,kwidthCHOR,kmixCHOR






;flanger --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
aFLAL    Flanger    a1,kRateFLA,kDepthFLA,kDelayFLA,kFbackFLA,klfoshapeFLA    
aFLAR    Flanger    a2,kRateFLA,kDepthFLA,kDelayFLA,kFbackFLA,klfoshapeFLA   
aFLAoutL    ntrpol    aPS_L,aFLAL,(kDepthFLA * 100)          
aFLAoutR    ntrpol    aPS_R,aFLAR,(kDepthFLA * 100)  






;vdelayx --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
aDelay3L vdelayx aPS_L, aLFO3, 0.5, iWindowSize3
aDelay3R vdelayx aPS_R, aLFO3, 0.5, iWindowSize3





;FMshi --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
aFirst5L = (aPhaseLout + aCHORoutL + aFLAoutL + aDelay3L) 
aFirst5R = (aPhaseRout + aCHORoutR + aFLAoutR + aDelay3R)



;20 pole delay --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
kTapVolume1 = kFeedbackScale * (1 - kDecayFactor * 0.1)
kTapVolume2 = kFeedbackScale * (1 - kDecayFactor * 0.25)
kTapVolume3 = kFeedbackScale * (1 - kDecayFactor * 0.5)
kTapVolume4 = kFeedbackScale * (1 - kDecayFactor * 0.7)
kTapVolume5 = kFeedbackScale * (1 - kDecayFactor * 0.9)
kTapVolume6 = kFeedbackScale * (1 - kDecayFactor * 1.1)
kTapVolume7 = kFeedbackScale * (1 - kDecayFactor * 1.3)
kTapVolume8 = kFeedbackScale * (1 - kDecayFactor * 1.5)
kTapVolume9 = kFeedbackScale * (1 - kDecayFactor * 1.7)
kTapVolume10 = kFeedbackScale * (1 - kDecayFactor * 1.9)
kTapVolume11 = kFeedbackScale * (1 - kDecayFactor * 2.1)
kTapVolume12 = kFeedbackScale * (1 - kDecayFactor * 2.3)
kTapVolume13 = kFeedbackScale * (1 - kDecayFactor * 2.5)
kTapVolume14 = kFeedbackScale * (1 - kDecayFactor * 2.7)
kTapVolume15 = kFeedbackScale * (1 - kDecayFactor * 2.9)
kTapVolume16 = kFeedbackScale * (1 - kDecayFactor * 3.1)
kTapVolume17 = kFeedbackScale * (1 - kDecayFactor * 3.3)
kTapVolume18 = kFeedbackScale * (1 - kDecayFactor * 3.5)
kTapVolume19 = kFeedbackScale * (1 - kDecayFactor * 3.7)
kTapVolume20 = kFeedbackScale * (1 - kDecayFactor * 3.9)

; Ensure that the volumes are not negative
kTapVolume1 = max(0, kTapVolume1)
kTapVolume2 = max(0, kTapVolume2)
kTapVolume3 = max(0, kTapVolume3)
kTapVolume4 = max(0, kTapVolume4)
kTapVolume5 = max(0, kTapVolume5)
kTapVolume6 = max(0, kTapVolume6)
kTapVolume7 = max(0, kTapVolume7)
kTapVolume8 = max(0, kTapVolume8)
kTapVolume9 = max(0, kTapVolume9)
kTapVolume10 = max(0, kTapVolume10)
kTapVolume11 = max(0, kTapVolume11)
kTapVolume12 = max(0, kTapVolume12)
kTapVolume13 = max(0, kTapVolume13)
kTapVolume14 = max(0, kTapVolume14)
kTapVolume15 = max(0, kTapVolume15)
kTapVolume16 = max(0, kTapVolume16)
kTapVolume17 = max(0, kTapVolume17)
kTapVolume18 = max(0, kTapVolume18)
kTapVolume19 = max(0, kTapVolume19)
kTapVolume20 = max(0, kTapVolume20)

; Define Delay Signals
aTap1Delay = kDelayTime4
aTap2Delay = kDelayTime4
aTap3Delay = kDelayTime4
aTap4Delay = kDelayTime4
aTap5Delay = kDelayTime4
aTap6Delay = kDelayTime4
aTap7Delay = kDelayTime4
aTap8Delay = kDelayTime4
aTap9Delay = kDelayTime4
aTap10Delay = kDelayTime4
aTap11Delay = kDelayTime4
aTap12Delay = kDelayTime4
aTap13Delay = kDelayTime4
aTap14Delay = kDelayTime4
aTap15Delay = kDelayTime4
aTap16Delay = kDelayTime4
aTap17Delay = kDelayTime4
aTap18Delay = kDelayTime4
aTap19Delay = kDelayTime4
aTap20Delay = kDelayTime4

;do the shit
aTap1L vdelay3 aFirst5L, aTap1Delay, 100000
aTap2L vdelay3 aTap1L, aTap2Delay, 100000
aTap3L vdelay3 aTap2L, aTap3Delay, 100000
aTap4L vdelay3 aTap3L, aTap4Delay, 100000
aTap5L vdelay3 aTap4L, aTap5Delay, 100000
aTap6L vdelay3 aTap5L, aTap6Delay, 100000
aTap7L vdelay3 aTap6L, aTap7Delay, 100000
aTap8L vdelay3 aTap7L, aTap8Delay, 100000
aTap9L vdelay3 aTap8L, aTap9Delay, 100000
aTap10L vdelay3 aTap9L, aTap10Delay, 100000
aTap11L vdelay3 aTap10L, aTap11Delay, 100000
aTap12L vdelay3 aTap11L, aTap12Delay, 100000
aTap13L vdelay3 aTap12L, aTap13Delay, 100000
aTap14L vdelay3 aTap13L, aTap14Delay, 100000
aTap15L vdelay3 aTap14L, aTap15Delay, 100000
aTap16L vdelay3 aTap15L, aTap16Delay, 100000
aTap17L vdelay3 aTap16L, aTap17Delay, 100000
aTap18L vdelay3 aTap17L, aTap18Delay, 100000
aTap19L vdelay3 aTap18L, aTap19Delay, 100000
aTap20L vdelay3 aTap19L, aTap20Delay, 100000

aTap1R vdelay3 aFirst5R, aTap1Delay + 20, 100000
aTap2R vdelay3 aTap1R, aTap2Delay, 100000
aTap3R vdelay3 aTap2R, aTap3Delay, 100000
aTap4R vdelay3 aTap3R, aTap4Delay, 100000
aTap5R vdelay3 aTap4R, aTap5Delay, 100000
aTap6R vdelay3 aTap5R, aTap6Delay, 100000
aTap7R vdelay3 aTap6R, aTap7Delay, 100000
aTap8R vdelay3 aTap7R, aTap8Delay, 100000
aTap9R vdelay3 aTap8R, aTap9Delay, 100000
aTap10R vdelay3 aTap9R, aTap10Delay, 100000
aTap11R vdelay3 aTap10R, aTap11Delay, 100000
aTap12R vdelay3 aTap11R, aTap12Delay, 100000
aTap13R vdelay3 aTap12R, aTap13Delay, 100000
aTap14R vdelay3 aTap13R, aTap14Delay, 100000
aTap15R vdelay3 aTap14R, aTap15Delay, 100000
aTap16R vdelay3 aTap15R, aTap16Delay, 100000
aTap17R vdelay3 aTap16R, aTap17Delay, 100000
aTap18R vdelay3 aTap17R, aTap18Delay, 100000
aTap19R vdelay3 aTap18R, aTap19Delay, 100000
aTap20R vdelay3 aTap19R, aTap20Delay, 100000

; Apply feedback scaling to each tap
aTap1L *= kTapVolume1
aTap2L *= kTapVolume2
aTap3L *= kTapVolume3
aTap4L *= kTapVolume4
aTap5L *= kTapVolume5
aTap6L *= kTapVolume6
aTap7L *= kTapVolume7
aTap8L *= kTapVolume8
aTap9L *= kTapVolume9
aTap10L *= kTapVolume10
aTap11L *= kTapVolume11
aTap12L *= kTapVolume12
aTap13L *= kTapVolume13
aTap14L *= kTapVolume14
aTap15L *= kTapVolume15
aTap16L *= kTapVolume16
aTap17L *= kTapVolume17
aTap18L *= kTapVolume18
aTap19L *= kTapVolume19
aTap20L *= kTapVolume20

aTap1R *= kTapVolume1
aTap2R *= kTapVolume2
aTap3R *= kTapVolume3
aTap4R *= kTapVolume4
aTap5R *= kTapVolume5
aTap6R *= kTapVolume6
aTap7R *= kTapVolume7
aTap8R *= kTapVolume8
aTap9R *= kTapVolume9
aTap10R *= kTapVolume10
aTap11R *= kTapVolume11
aTap12R *= kTapVolume12
aTap13R *= kTapVolume13
aTap14R *= kTapVolume15
aTap16R *= kTapVolume16
aTap17R *= kTapVolume17
aTap18R *= kTapVolume18
aTap19R *= kTapVolume19
aTap20R *= kTapVolume20







;final summing ----------------------------------------------------------------------------------------------------------------------------------(15R fucked up)----------------------------------------------------------------------------------------------------------
aDelay4L = (aTap1L + aTap2L + aTap3L + aTap4L + aTap5L + aTap6L + aTap7L + aTap8L + aTap9L + aTap10L + aTap11L + aTap12L + aTap13L + aTap14L + aTap15L + aTap16L + aTap17L + aTap18L + aTap19L + aTap20L) * kDepth4
aDelay4R = (aTap1R + aTap2R + aTap3R + aTap4R + aTap5R + aTap6R + aTap7R + aTap8R + aTap9R + aTap10R + aTap11R + aTap12R + aTap13R + aTap14R + aTap15L + aTap16R + aTap17R + aTap18R + aTap19R + aTap20R) * kDepth4


aMixL = (aFirst5L + aDelay4L)
aMixR = (aFirst5R + aDelay4R)


aPaninL = aMixL * kLFOall
aPaninR = aMixR * (1 - kLFOall)


aPandL = ((aMixL * (1 - kWidth)) + (aPaninL * kWidth))
aPandR = ((aMixR * (1 - kWidth)) + (aPaninR * kWidth))
 

aWaveshaprL LoFi aPandL, kbits * 0.6, kfold
aWaveshaprR LoFi aPandR, kbits * 0.6, kfold


aPowershaprL powershape  aWaveshaprL, kshape, ifullscale
aPowershaprR powershape  aWaveshaprR, kshape, ifullscale


aAMoutL = aPowershaprL * (1 + (aAModLFO/101))
aAMoutR = aPowershaprR * (1 + (aAModLFO/101))


aFRoutL = ((1 - kdrywet) * (a1*2) + kdrywet * aAMoutL) * kGain * 1.5
aFRoutR = ((1 - kdrywet) * (a2*2) + kdrywet * aAMoutR) * kGain * 1.5


outs aFRoutL, aFRoutR 
endin








</CsInstruments>
<CsScore>
;causes Csound to run for about 7000 years...a
f0 z
;starts instrument 1 and runs it for a week
i1 0 z
</CsScore>
</CsoundSynthesizer>
