<CsOptions>
-odac
-iadc
-b1024
-B1024
-B2
-Mhw:1,0,0
-+rtmidi=NULL
--daemon
</CsOptions>
<CsInstruments>
sr = 44100
ksmps = 8
nchnls = 2
0dbfs = 1
seed 0
giHanning ftgen 0,0,1024,20,2
gisine ftgen 0, 0, 16384, 10, 1
giwfn1 ftgen 0,0,16384,9,0.5,1,0
giwfn2 ftgen 0,0,16384,7,0,3072,1,12800,0
giwfn3 ftgen 0,0,16384,16,0,3072,0,1,12800,-2,0
giwfn4 ftgen 0,0,16384,7,0,3536,1,12400,1,3536,0
giwfn5 ftgen 0,0,16384,7,0,12800,1,3072,0
giwfn6 ftgen 0,0,16384,5,0.001,12800,1,3072,0.001
giwfn7 ftgen 0,0,16384,20,2,1
giwindows ftgen 0,0,8,-2, giwfn7, giwfn1, giwfn2, giwfn3, giwfn4
giBufL ftgen 0,0,524288,-2,0
giBufR ftgen 0,0,524288,-2,0
giGPSbuffer ftgen 0,0,524288,2,0
giGPSbufferR ftgen 0,0,524288,2,0
giGPSwfn1 ftgen 0,0,16384,9,0.5,1,0
giGPSwfn2 ftgen 0,0,16384,7,0,3072,1,12800,0
giGPSwfn3 ftgen 0,0,16384,5,0.001,3072,1,12800,0.001
giGPSwfn4 ftgen 0,0,16384,7,0,1536,1,12800,1,1536,0
giGPSwfn5 ftgen 0,0,16384,7,0,12800,1,3072,0
giGPSwfn6 ftgen 0,0,16384,5,0.001,12800,1,3072,0.001
giGPSnnsamp = ftlen(giGPSbuffer) -1
gkPowerMax init 50
gkPortTime init 0.03
gkSemi init 0
gkDelayMS init 0
gkFeedback init 0.5
gkARModDepth init 1
gkARModFreq init 0.1
gkVOLpedal init 1
gkDryPctPS init 100
gkBypassPS init 1
gkKnobRaw init 1
gkBitsBC init 16
gkBypassDist init 1
gkBypassBC init 1
gkBypassDelay init 1
gkVOLpedalBypass init 1
gkDelMix init 0.5
gkBypassRM init 1
gkverbBypass init 1
gkverbMix init 1
gkSemiDel init -0.01
gkDryPctPSdel init 100
gkBypassPSdel init 0
gkKnobRawDel init 100
gkReverbFB init 0.92
gkStepReverbFB init 0.01
gkGranBypass init 1
gkGranMix init 0.5
gkStepGranMix init 0.01
gkStepGPSdly init 0.1
gkGPSamp init 1
gkGPSInGain init 1
gkGPSmix init 1
gkGPSbalance init 1
gkGPSmonostereo init 3
gkGPSfback init 0
gkStepGPSfback init 0.1
gkGPSclip init 1
gkGPSClipLev init 0.5
gkGPSCutoff init 4000
gkGPSLPF_On init 0
gkGPSwsize init 2000
gkGPSrnd init 0
gkGPSsemis init 12
gkGPSolap init 1000
gkGPSwfn init 1
gkGPSdly init 0.001
gkGPSbeta init 1
gkGPSfreeze init 0
gkGPSManPtrPort init 0.5
gkGPSManPtr init 0
gkBypassGPS init 1
gkStepGPSMix init 0.01
gkLastToggle init 1
gkBypassTS init 1
gkTSAmtAcc init 0.0
gkStepTSAmt init 0.01
gkBypassChorus init 1
gkChorusMixAcc init 1
gkStepChorusMix init 0.1
gkChorusMix init 0.9
gkChorusRate init 0.7
gkChorusDereg init 0.5
gkChorusDepth init 0.4
gkChorusOffset init 0.02
gkChorusWidth init 1
gkChorusType init 1
gkStepChorusRate init 0.05
gkStepChorusDepth init 0.05
giFFT = 256
giHop = ksmps
giWin = giFFT
giFormat = 1
gkSpecDelBypass init 1
gkSpecDelMix init 0.5
gkSpecDelFB init 0.8
gkSpecDelMaxDel init 0.3
gkSpecDelPrev init 1
gaSpecDelSendL init 0
gaSpecDelSendR init 0
gaSpecDelWetL init 0
gaSpecDelWetR init 0
giSpecDelAmpTab ftgen 300, 0, giFFT, -21, 1, 0.3
giSpecDelFrqTab ftgen 301, 0, giFFT, -21, 1, 0.3
gaPSSendL init 0
gaPSSendR init 0
gaPSOutL init 0
gaPSOutR init 0
gaDistSendL init 0
gaDistSendR init 0
gaDistOutL init 0
gaDistOutR init 0
gaTSSendL init 0
gaTSSendR init 0
gaTSOutL init 0
gaTSOutR init 0
gaDelSendL init 0
gaDelSendR init 0
gaDelOutL init 0
gaDelOutR init 0
gaDelWetL init 0
gaDelWetR init 0
gaRMSendL init 0
gaRMSendR init 0
gaRMOutL init 0
gaRMOutR init 0
gaVerbSendL init 0
gaVerbSendR init 0
gaVerbWetL init 0
gaVerbWetR init 0
gaGranSendL init 0
gaGranSendR init 0
gaGranWetL init 0
gaGranWetR init 0
gaGPSSendL init 0
gaGPSSendR init 0
gaGPSWetL init 0
gaGPSWetR init 0
gaGPSFBackSig init 0
gaGPSFBackSigR init 0
gaChorusSendL init 0
gaChorusSendR init 0
gaChorusWetL init 0
gaChorusWetR init 0
gkLastToggle init 0
gkSTUTchance init 0.25
gkSTUTdrywet init 1.0
gkSTUTbpm init 150
gkSTUTbypass init 1
gkSTUTfltMix init 0.6
gkStepFeedback init 0.005
gkStepSpecDelMaxDel init 0.002
gkStepSTUTchance init 0.01
gkStepSemiDel init 0.1
gkStepSpecDelFB init 0.01
gkStepSTUTbpm init 2
gkStepSTUTdrywet init 0.01
gkStepDelMix init 0.01
gkStepSpecMix init 0.01
gkPSdryPctStep init 0.5
gkKnobRawStep init 0.01
gkBitsStep init 1
gkStepverbMix init 0.01
gkDistAmtAcc init 0.0
gkStepDistAmt init 0.01
gkBitsAmtAcc init 0.0
gkStepBitsAmt init 0.05
gkAUTOMOOG_bypass init 1
gkAUTOMOOG_sensAcc init 0.8
gkStepAUTOMOOG_sens init 0.01
gkAUTOMOOG_sens init 0.8
gkAUTOMOOG_att init 0.005
gkAUTOMOOG_rel init 0.15
gkAUTOMOOG_freq init 200
gkAUTOMOOG_type init 2
gkAUTOMOOG_res init 0.85
gkAUTOMOOG_dist init 0
gkAUTOMOOG_level init 1
gkLastToggle init 0
gaAUTOMOOG_sendL init 0
gaAUTOMOOG_sendR init 0
gaAUTOMOOG_outL init 0
gaAUTOMOOG_outR init 0
gisine ftgen 0,0,16384,10,1
giLog ftgen 0,0,4096,19, 0.5, 1, 0, 0
giExp ftgen 0,0,4096,19, 0.5, 1, 180, 1
gkVIB_bypass init 1
gkVIB_depthAcc init 0.0667
gkStepVIB_depth init 0.0011
gkVIB_depth init 0.0019
gkVIB_freq init 1
gkFLA_bypass init 1
gkFLA_depthAcc init 0.5
gkStepFLA_depth init 0.01
gkFLA_depth init 0.005
gkFLA_rate init 2
gkFLA_offset init 0.0001
gkFLA_fback init 0.2
gkFLA_lfoshape init 4
gkFLA_mix init 1
gkStepFLA_rate init 0.1
gkStepFLA_fback init 0.01
gkPHA_bypass init 1
gkPHA_depthAcc init 0.5
gkStepPHA_depth init 0.01
gkPHA_depth init 0.5
gkPHA_rate init 2
gkPHA_freq init 0.1
gkPHA_fback init 0.2
gkPHA_stages init 8
gkPHA_mix init 1
gkPHA_shape init 8
gkStepPHA_rate init 0.1
gkStepPHA_fback init 0.01
gkLastToggle init 0
gkChorusDepthAcc init 1
gaVIB_sendL init 0
gaVIB_sendR init 0
gaVIB_outL init 0
gaVIB_outR init 0
gaFLA_sendL init 0
gaFLA_sendR init 0
gaFLA_outL init 0
gaFLA_outR init 0
gaPHA_sendL init 0
gaPHA_sendR init 0
gaPHA_outL init 0
gaPHA_outR init 0
gisine ftgen 0, 0, 4096, 10, 1
gkLeslieBypass init 1
gkLeslieDepthAcc init 0
gkStepLeslieDepth init 0.01
gkLeslieDepth init 1
gkLeslieSpeedAcc init 0
gkStepLeslieSpeed init 0.01
gkLeslieSpeed init 7.5
gaLeslieSendL init 0
gaLeslieSendR init 0
gaLeslieOutL init 0
gaLeslieOutR init 0
seed 0
giHanning ftgen 0,0,1024,20,2
gisine ftgen 0, 0, 16384, 10, 1
giwfn1 ftgen 0,0,16384,9,0.5,1,0
giwfn2 ftgen 0,0,16384,7,0,3072,1,12800,0
giwfn3 ftgen 0,0,16384,16,0,3072,0,1,12800,-2,0
giwfn4 ftgen 0,0,16384,7,0,3536,1,12400,1,3536,0
giwfn5 ftgen 0,0,16384,7,0,12800,1,3072,0
giwfn6 ftgen 0,0,16384,5,0.001,12800,1,3072,0.001
giwfn7 ftgen 0,0,16384,20,2,1
giwindows ftgen 0,0,8,-2, giwfn7, giwfn1, giwfn2, giwfn3, giwfn4
giBufL ftgen 0,0,524288,-2,0
giBufR ftgen 0,0,524288,-2,0
giSemi ftgen 0,0,10,-2, 24,19,12,7,0,-5,-12,-17,-24,-29
giHalfSine ftgen 0,0,16384,9,0.5,1,0
giPitchBufL ftgen 0,0,524288,2,0
giPitchBufR ftgen 0,0,524288,2,0
gkMOOD_Bypass init 1
gkMOOD_MixAcc init 0.5
gkStepMOOD_Mix init 0.01
gkMOOD_Mix init 0.5
gkMOOD_Clock init 0
gkLastToggle init 0
gkMOOD_Dens init 1200
gkMOOD_Dly init 0.7
gkStepMOOD_Dens init 100
gkStepMOOD_Dly init 0.1
gaMOOD_SendL init 0
gaMOOD_SendR init 0
gaMOOD_PitchL init 0
gaMOOD_PitchR init 0
gaMOOD_Bit1L init 0
gaMOOD_Bit1R init 0
gaMOOD_GranL init 0
gaMOOD_GranR init 0
gaMOOD_FinalL init 0
gaMOOD_FinalR init 0
gkFormantBypass init 1
gkFormantShift init 0
gkFormantMix init 1
gkLastToggle init 0
gaFormantSendL init 0
gaFormantSendR init 0
gaFormantOutL init 0
gaFormantOutR init 0
giHanning ftgen 0,0,1024,20,2
gisine ftgen 0, 0, 16384, 10, 1
gkPortTime init 0.03
gkATTK_bypass init 1
gkATTK_mix init 1.0
gkATTK_atkMs init 500
gkATTK_sens init 0.1
gkStepATTK_sens init 0.1
gkATTK_holdMs init 500
gkStepATTK_hold init 50
gkATTK_decMs init 20
gkATTK_susLvl init 1.0
gkATTK_relMs init 100
gkATTK_killMs init 3
gkATTK_shapePow init 2.5
gkATTK_basePreSec init 0.005
gkATTK_maxPreSec init 0.030
giPreMaxMs init 200
gkATTK_susHoldSec init 0.1
gkATTK_minSusSec init 0.2
gkATTK_lockSec init 0.1
gkATTK_noiseFloor init 0.05
gaATTK_sendL init 0
gaATTK_sendR init 0
gaATTK_outL init 0
opcode SrReducer, a, ak
ain, ksrate xin
kperiod = sr / ksrate
aout = 0
kcnt init 0
kheld init 0
if (kperiod <= 1) then
aout = ain
else
ksmpl = 0
while ksmpl < ksmps do
if (kcnt >= kperiod) then
kheld = ain[ksmpl]
kcnt = kcnt - kperiod
endif
aout[ksmpl] = kheld
kcnt = kcnt + 1
ksmpl = ksmpl + 1
od
endif
xout aout
endop
opcode DopplerSpin, aa, akkkkki
asig, kfreq, kAmpDepth, kAmpPhase, kPanDepth, kDopDep, ishape xin
 
aAmp osciliktp kfreq, ishape, kAmpPhase
aAmp = (aAmp * 0.5 * kAmpDepth) + 0.5
 
aPan oscili (kPanDepth * 0.5), kfreq, ishape, 0.75
aPan = aPan + 0.5
aDelTim oscili kDopDep, kfreq, ishape, 0
aDelTim = aDelTim + kDopDep
 
iMaxDelay = 4
aDelTap vdelay asig * aAmp, aDelTim, iMaxDelay
 
aL, aR pan2 aDelTap, aPan, 1
 
xout aL, aR
endop
opcode EnvelopeFollower,a,akkkkkkk
ain, ksens, katt, krel, kfreq, ktype, kres, kdist xin
setksmps 16
aFollow follow2 ain, katt, krel
kFollow downsamp aFollow
kFollow expcurve kFollow/0dbfs, 0.3
kFrq = kfreq + (kFollow * ksens * 10000)
kFrq limit kFrq, 20,sr/2
if ktype==1 then
aout lpf18 ain, kFrq, kres, kdist
elseif ktype==2 then
aout moogladder ain, kFrq, kres
elseif ktype==3 then
aFrq interp kFrq
aout butlp ain, aFrq
elseif ktype==4 then
aout tone ain, kFrq
endif
xout aout
endop
opcode SwitchPort, k, kii
kin,iupport,idnport xin
kold init 0
kporttime = (kin < kold ? idnport : iupport)
kout portk kin, kporttime
kold = kout
xout kout
endop
opcode LoFi, a, akk
ain, kbits, kfold xin
kval = pow(2, kbits)
ain = clip(ain, -1, 1)
aout = int(ain * kval) / kval
aout fold aout, kfold
xout aout
endop
opcode pitchshifter4, aa, aakikkiip
ainL,ainR,kRatio,iNIter,kDelay,kSmooth,iMaxDel,iWfn,iCnt xin
setksmps 16
kPt linseg 0,0.001,1
if kSmooth>0 then
kRatio portk kRatio,kPt*kSmooth
kDelay portk kDelay,kPt*kSmooth
endif
aDel interp kDelay
aRate = (kRatio-1)/kDelay
a1 phasor -aRate
a2 phasor -aRate,0.5
ag1 tablei a1,iWfn,1,0,1
ag2 tablei a2,iWfn,1,0,1
abuf1 delayr iMaxDel
ad1 deltap3 a1*aDel
aG1 = ad1*ag1
delayw ainL
abuf2 delayr iMaxDel
ad2 deltap3 a2*aDel
aG2 = ad2*ag2
delayw ainL
abuf3 delayr iMaxDel
ad3 deltap3 a1*aDel
aG3 = ad3*ag1
delayw ainR
abuf4 delayr iMaxDel
ad4 deltap3 a2*aDel
aG4 = ad4*ag2
delayw ainR
aL = 0.5*(aG1+aG2)
aR = 0.5*(aG3+aG4)
if iCnt<iNIter then
aNL,aNR pitchshifter4 ainL,ainR,kRatio,iNIter,kDelay,kSmooth,iMaxDel,iWfn,iCnt+1
aL = aL + aNL
aR = aR + aNR
endif
xout aL,aR
endop
opcode STUTiteration, aa, aaiiiiiiiii
aL, aR, iBPS, isubdiv, ibarlen, iphrase, irepeats, istutspd, istutchnc, icount, ilayers xin
aOutL bbcutm aL, iBPS, isubdiv, ibarlen, iphrase, irepeats, istutspd, istutchnc
aOutR bbcutm aR, iBPS, isubdiv, ibarlen, iphrase, irepeats, istutspd, istutchnc
aMixL = 0
aMixR = 0
if icount < ilayers then
aMixL, aMixR STUTiteration aL, aR, iBPS, isubdiv, ibarlen, iphrase, irepeats, istutspd, istutchnc, icount + 1, ilayers
endif
xout aOutL + aMixL, aOutR + aMixR
endop
opcode STUTfreqshifter, a, aki
ain, kfshift, ifn xin
areal, aimag hilbert ain
asin oscili 1, kfshift, ifn, 0
acos oscili 1, kfshift, ifn, 0.25
amod1 = areal * acos
amod2 = aimag * asin
aFS = (amod1 - amod2)
xout aFS
endop
opcode StChorus, aa, aakkakk
ainL, ainR, krate, kdepth, aoffset, kwidth, kmix xin
ilfoshape ftgentmp 0, 0, 131072, 19, 1, 0.5, 0, 0.5
kporttime linseg 0, 0.001, 0.02
kChoDepth portk kdepth*0.01, kporttime
aChoDepth interp kChoDepth
amodL osciliktp krate, ilfoshape, 0
amodR osciliktp krate, ilfoshape, kwidth*0.5
amodL = (amodL*aChoDepth) + aoffset
amodR = (amodR*aChoDepth) + aoffset
aChoL vdelay ainL, amodL*1000, 1.2*1000
aChoR vdelay ainR, amodR*1000, 1.2*1000
aoutL ntrpol ainL*0.6, aChoL*0.6, kmix
aoutR ntrpol ainR*0.6, aChoR*0.6, kmix
xout aoutL, aoutR
endop
opcode StChorusRspline, aa, aakkkkkk
ainL, ainR, krate, kdereg, kdepth, koffset, kwidth, kmix xin
kporttime linseg 0, 0.001, 0.02
kChoDepth portk kdepth*0.01, kporttime
kmod1 rspline koffset, kChoDepth, krate*4+0.01, ((krate*4*kdereg)+0.01)
kmod2 rspline kChoDepth, koffset, krate*4+0.01, ((krate*4*kdereg)+0.01)
kmod1 limit kmod1, 0.0001, 1.2
kmod2 limit kmod2, 0.0001, 1.2
amod1 interp kmod1
amod2 interp kmod2
aCho1 vdelay ainL, amod1*1000, 1.2*1000
aCho2 vdelay ainR, amod2*1000, 1.2*1000
kpan rspline 0,1,krate,2*krate*kdereg
kpan limit kpan,0,1
apan interp kpan
aChoL = (aCho1*apan)+(aCho2*(1-apan))
aChoR = (aCho2*apan)+(aCho1*(1-apan))
aChoL ntrpol aChoL,aCho1,kwidth
aChoR ntrpol aChoR,aCho2,kwidth
aoutL ntrpol ainL*0.6, aChoL*0.6, kmix
aoutR ntrpol ainR*0.6, aChoR*0.6, kmix
xout aoutL, aoutR
endop
opcode Flanger,a,akkkkk
ain,krate,kdepth,kdelay,kfback,klfoshape xin ;read in input arguments
iparabola ftgentmp 0, 0, 131072, 19, 0.5, 1, 180, 1 ;u-shape parabola for lfo
isine ftgentmp 0, 0, 131072, 19, 1, 0.5, 0, 0.5 ;sine-shape for lfo
itriangle ftgentmp 0, 0, 131072, 7, 0,131072/2,1,131072/2,0;triangle-shape for lfo
adlt interp kdelay ;a new a-rate variable 'adlt' is created by interpolating the k-rate variable 'kdlt'
if klfoshape==1 then
amod oscili kdepth, krate, iparabola ;oscillator that makes use of the positive domain only u-shape parabola
elseif klfoshape==2 then
amod oscili kdepth, krate, isine ;oscillator that makes use of the positive domain only sine wave
elseif klfoshape==3 then
amod oscili kdepth, krate, itriangle ;oscillator that makes use of the positive domain only triangle
elseif klfoshape==4 then
amod randomi 0,kdepth,krate,1
else
amod randomh 0,kdepth,krate,1
endif
adlt sum adlt, amod ;static delay time and modulating delay time are summed
adelsig flanger ain, adlt, kfback , 1.2 ;flanger signal created
adelsig dcblock adelsig
aout sum ain*0.5, adelsig*0.5 ;create dry/wet mix
xout aout ;send audio back to caller instrument
endop
opcode PhaserSt,aa,aakkkkki
ainL,ainR,krate,kdepth,kfreq,kstages,kfback,ishape xin ;READ IN INPUT ARGUMENTS
if ishape=1 then
klfo lfo kdepth*0.5, krate, 1 ;LFO FOR THE PHASER (TRIANGULAR SHAPE)
elseif ishape=2 then
klfo lfo kdepth*0.5, krate, 0 ;LFO FOR THE PHASER (SINE SHAPE)
elseif ishape=3 then
klfo lfo kdepth*0.5, krate, 2 ;LFO FOR THE PHASER (SQUARE SHAPE)
elseif ishape=4 then
klfo lfo kdepth, krate, 4 ;LFO FOR THE PHASER (SAWTOOTH)
elseif ishape=5 then
klfo lfo kdepth, krate, 5 ;LFO FOR THE PHASER (SAWTOOTH)
elseif ishape=6 then
klfo oscil kdepth, krate, giExp ;LFO FOR THE PHASER (EXP)
elseif ishape=7 then
klfo oscil kdepth, krate, giLog ;LFO FOR THE PHASER (LOG)
elseif ishape=8 then
klfo randomi -kdepth*0.5, kdepth*0.5, krate*8 ;LFO FOR THE PHASER (RANDOMI SHAPE)
klfo portk klfo, 1/(krate*8) ;SMOOTH CHANGES OF DIRECTION
elseif ishape=9 then
klfo randomh -kdepth*0.5, kdepth*0.5, krate ;LFO FOR THE PHASER (RANDOMH SHAPE)
endif
aoutL phaser1 ainL, cpsoct((klfo+(kdepth*0.5)+kfreq)), kstages, kfback ;PHASER1 IS APPLIED TO THE INPUT AUDIO
aoutR phaser1 ainR, cpsoct((klfo+(kdepth*0.5)+kfreq)), kstages, kfback ;PHASER1 IS APPLIED TO THE INPUT AUDIO
xout aoutL,aoutR ;SEND AUDIO BACK TO CALLER INSTRUMENT
endop
instr 99
kSt,kCh,kCC,kVal midiin
if (kSt >= 176 && kSt < 192) then
kNorm = kVal/127.0
if (kCC == 110) then
gkMOOD_Bypass = (kVal < 64 ? 1 : 0)
if (gkMOOD_Bypass == 0) then
gkLastToggle = 18
gkMOOD_MixAcc = 1.0
gkMOOD_Mix = 1.0
endif
elseif (kCC == 3) then
if (kVal > 64) then
gkMOOD_MixAcc = min(gkMOOD_MixAcc + gkStepMOOD_Mix, 1)
elseif (kVal < 64) then
gkMOOD_MixAcc = max(gkMOOD_MixAcc - gkStepMOOD_Mix, 0)
endif
gkMOOD_Mix = gkMOOD_MixAcc
printks "MOOD Mix = %.2f\n", 0, gkMOOD_Mix
elseif (kCC == 61) then
if (gkLastToggle == 1) then
gkDelayMS = kNorm*700000
elseif (gkLastToggle == 5) then
gkSemi = kNorm*24 - 12
elseif (gkLastToggle == 6) then
gkARModFreq = 0.05 * pow(200, kNorm) * 25
elseif (gkLastToggle == 8) then
gkVOLpedal = kNorm
elseif (gkLastToggle == 10) then
gkGPSsemis = kNorm*24 - 12
elseif (gkLastToggle == 18) then
gkMOOD_Clock = 1 - kNorm
printks "MOOD Clock Position = %.2f\n", 0, gkMOOD_Clock
elseif (gkLastToggle == 19) then
gkFormantShift = (kNorm * 24) - 12
printks "Formant Shift = %.2f semitones\n", 0, gkFormantShift
endif
elseif (kCC == 107) then
gkBypassPS = 1 - kNorm
if (gkBypassPS < 0.5) then
gkLastToggle = 5
endif
elseif (kCC == 83) then
gkBypassDist = 1 - kNorm
gkBypassBC = 1 - kNorm
if (gkBypassDist < 0.5) then
gkLastToggle = 7
endif
elseif (kCC == 115) then
gkBypassDelay = 1 - kNorm
if (gkBypassDelay < 0.5) then
gkLastToggle = 1
endif
elseif (kCC == 87) then
gkVOLpedalBypass = 1 - kNorm
if (gkVOLpedalBypass < 0.5) then
gkLastToggle = 8
endif
elseif (kCC == 80) then
gkBypassRM = 1 - kNorm
if (gkBypassRM < 0.5) then
gkLastToggle = 6
endif
elseif (kCC == 108) then
gkverbBypass = 1 - kNorm
if (gkverbBypass < 0.5) then
gkLastToggle = 4
endif
elseif (kCC == 101) then
gkSpecDelBypass = 1 - kNorm
if (gkSpecDelBypass < 0.5) then
gkLastToggle = 2
endif
elseif (kCC == 112) then
gkSTUTbypass = (kVal < 64 ? 1 : 0)
if (gkSTUTbypass == 0) then
gkLastToggle = 3
endif
elseif (kCC == 106) then
gkBypassGPS = 1 - kNorm
if (gkBypassGPS < 0.5) then
gkLastToggle = 10
endif
elseif (kCC == 111) then
gkGranBypass = 1 - kNorm
if (gkGranBypass < 0.5) then
gkLastToggle = 9
endif
elseif (kCC == 84) then
gkBypassTS = (kVal < 64 ? 1 : 0)
if (gkBypassTS == 0) then
gkLastToggle = 11
endif
elseif (kCC == 98) then
gkBypassChorus = (kVal < 64 ? 1 : 0)
if (gkBypassChorus == 0) then
gkLastToggle = 12
endif
elseif (kCC == 97) then
if (kVal > 64) then
gkDryPctPS = min(gkDryPctPS + 10, 100)
elseif (kVal < 64) then
gkDryPctPS = max(gkDryPctPS - 10, 0)
endif
printks "PS Dry Pct = %.0f\n", 0, gkDryPctPS
elseif (kCC == 93) then
if (kVal > 64) then
gkDelMix = min(gkDelMix + gkStepDelMix, 1)
elseif (kVal < 64) then
gkDelMix = max(gkDelMix - gkStepDelMix, 0)
endif
printks "Delay Mix = %.2f\n", 0, gkDelMix
elseif (kCC == 91) then
if (kVal > 64) then
gkSpecDelMix = min(gkSpecDelMix + gkStepSpecMix, 1)
elseif (kVal < 64) then
gkSpecDelMix = max(gkSpecDelMix - gkStepSpecMix, 0)
endif
printks "SpecDel Mix = %.2f\n", 0, gkSpecDelMix
elseif (kCC == 102) then
if (kVal > 64) then
gkSTUTdrywet = min(gkSTUTdrywet + gkStepSTUTdrywet, 1)
elseif (kVal < 64) then
gkSTUTdrywet = max(gkSTUTdrywet - gkStepSTUTdrywet, 0)
endif
printks "STUT Mix = %.2f\n", 0, gkSTUTdrywet
elseif (kCC == 1) then
if (kVal > 64) then
gkverbMix = min(gkverbMix + gkStepverbMix, 1)
elseif (kVal < 64) then
gkverbMix = max(gkverbMix - gkStepverbMix, 0)
endif
printks "Reverb Mix = %.2f\n", 0, gkverbMix
elseif (kCC == 4) then
if (kVal > 64) then
gkGranMix = min(gkGranMix + gkStepGranMix, 1)
elseif (kVal < 64) then
gkGranMix = max(gkGranMix - gkStepGranMix, 0)
endif
printks "Gran Mix = %.2f\n", 0, gkGranMix
elseif (kCC == 96) then
if (kVal > 64) then
gkGPSmix = min(gkGPSmix + gkStepGPSMix, 1)
elseif (kVal < 64) then
gkGPSmix = max(gkGPSmix - gkStepGPSMix, 0)
endif
printks "GPS Mix = %.2f\n", 0, gkGPSmix
elseif (kCC == 74) then
if (kVal > 64) then
gkTSAmtAcc = min(gkTSAmtAcc + gkStepTSAmt, 1)
elseif (kVal < 64) then
gkTSAmtAcc = max(gkTSAmtAcc - gkStepTSAmt, 0)
endif
printks "Tube Screamer Amt = %.2f\n", 0, gkTSAmtAcc
elseif (kCC == 95) then
if (kVal > 64) then
gkChorusMixAcc = min(gkChorusMixAcc + 0.1, 1)
elseif (kVal < 64) then
gkChorusMixAcc = max(gkChorusMixAcc - 0.1, 0)
endif
gkChorusMix = gkChorusMixAcc
printks "Chorus Mix = %.2f\n", 0, gkChorusMix
elseif (kCC == 78) then
if (kVal > 64) then
if (gkLastToggle == 1) then
gkFeedback = min(gkFeedback + gkStepFeedback, 1.2)
printks "FB = %.3f\n", 0, gkFeedback
elseif (gkLastToggle == 2) then
gkSpecDelMaxDel = min(gkSpecDelMaxDel + gkStepSpecDelMaxDel, 1)
printks "Spec MaxDel = %.3f\n", 0, gkSpecDelMaxDel
elseif (gkLastToggle == 3) then
gkSTUTchance = min(gkSTUTchance + gkStepSTUTchance, 1)
printks "STUT Chance = %.2f\n", 0, gkSTUTchance
elseif (gkLastToggle == 4) then
gkReverbFB = min(gkReverbFB + gkStepReverbFB, 0.999)
printks "Reverb Room = %.2f\n", 0, gkReverbFB
elseif (gkLastToggle == 12) then
gkChorusRate = min(gkChorusRate + gkStepChorusRate, 7)
printks "Chorus Rate = %.2f\n", 0, gkChorusRate
elseif (gkLastToggle == 14) then
gkVIB_freq = min(gkVIB_freq + 0.1, 10)
printks "VIB Rate = %.2f\n", 0, gkVIB_freq
elseif (gkLastToggle == 15) then
gkFLA_rate = min(gkFLA_rate + gkStepFLA_rate, 40)
printks "FLA Rate = %.2f\n", 0, gkFLA_rate
elseif (gkLastToggle == 16) then
gkPHA_rate = min(gkPHA_rate + gkStepPHA_rate, 40)
printks "PHA Rate = %.2f\n", 0, gkPHA_rate
elseif (gkLastToggle == 17) then
gkLeslieSpeedAcc = min(gkLeslieSpeedAcc + gkStepLeslieSpeed, 1)
gkLeslieSpeed = 0.1 + gkLeslieSpeedAcc * 9.9
printks "Leslie Speed = %.2f Hz\n", 0, gkLeslieSpeed
elseif (gkLastToggle == 10) then
gkGPSfback = min(gkGPSfback + 0.01, 0.99)
printks "GPS Fback = %.2f\n", 0, gkGPSfback
elseif (gkLastToggle == 18) then
gkMOOD_Dens = min(gkMOOD_Dens + gkStepMOOD_Dens, 1400)
printks "MOOD Density = %.0f\n", 0, gkMOOD_Dens
endif
elseif (kVal < 64) then
if (gkLastToggle == 1) then
gkFeedback = max(gkFeedback - gkStepFeedback, 0)
printks "FB = %.3f\n", 0, gkFeedback
elseif (gkLastToggle == 2) then
gkSpecDelMaxDel = max(gkSpecDelMaxDel - gkStepSpecDelMaxDel, 0)
printks "Spec MaxDel = %.3f\n", 0, gkSpecDelMaxDel
elseif (gkLastToggle == 3) then
gkSTUTchance = max(gkSTUTchance - gkStepSTUTchance, 0)
printks "STUT Chance = %.2f\n", 0, gkSTUTchance
elseif (gkLastToggle == 4) then
gkReverbFB = max(gkReverbFB - gkStepReverbFB, 0)
printks "Reverb Room = %.2f\n", 0, gkReverbFB
elseif (gkLastToggle == 12) then
gkChorusRate = max(gkChorusRate - gkStepChorusRate, 0.001)
printks "Chorus Rate = %.2f\n", 0, gkChorusRate
elseif (gkLastToggle == 14) then
gkVIB_freq = max(gkVIB_freq - 0.1, 0.001)
printks "VIB Rate = %.2f\n", 0, gkVIB_freq
elseif (gkLastToggle == 15) then
gkFLA_rate = max(gkFLA_rate - gkStepFLA_rate, 0.001)
printks "FLA Rate = %.2f\n", 0, gkFLA_rate
elseif (gkLastToggle == 16) then
gkPHA_rate = max(gkPHA_rate - gkStepPHA_rate, 0.001)
printks "PHA Rate = %.2f\n", 0, gkPHA_rate
elseif (gkLastToggle == 17) then
gkLeslieSpeedAcc = max(gkLeslieSpeedAcc - gkStepLeslieSpeed, 0)
gkLeslieSpeed = 0.1 + gkLeslieSpeedAcc * 9.9
printks "Leslie Speed = %.2f Hz\n", 0, gkLeslieSpeed
elseif (gkLastToggle == 10) then
gkGPSfback = max(gkGPSfback - 0.01, 0)
printks "GPS Fback = %.2f\n", 0, gkGPSfback
elseif (gkLastToggle == 18) then
gkMOOD_Dens = max(gkMOOD_Dens - gkStepMOOD_Dens, 0)
printks "MOOD Density = %.0f\n", 0, gkMOOD_Dens
endif
endif
elseif (kCC == 79) then
if (kVal > 64) then
if (gkLastToggle == 1) then
gkSemiDel = gkSemiDel + gkStepSemiDel
gkSemiDel limit gkSemiDel, -5, 12
printks "Delay Semi = %.2f\n", 0, gkSemiDel
elseif (gkLastToggle == 2) then
gkSpecDelFB = min(gkSpecDelFB + gkStepSpecDelFB, 1)
printks "Spec FB = %.2f\n", 0, gkSpecDelFB
elseif (gkLastToggle == 3) then
gkSTUTbpm = gkSTUTbpm + gkStepSTUTbpm
gkSTUTbpm limit gkSTUTbpm, 40, 400
printks "STUT BPM = %.1f\n", 0, gkSTUTbpm
elseif (gkLastToggle == 12) then
gkChorusDepth = min(gkChorusDepth + gkStepChorusDepth, 1)
printks "Chorus Depth = %.2f\n", 0, gkChorusDepth
elseif (gkLastToggle == 15) then
gkFLA_fback = min(gkFLA_fback + gkStepFLA_fback, 1)
printks "FLA Fback = %.2f\n", 0, gkFLA_fback
elseif (gkLastToggle == 16) then
gkPHA_fback = min(gkPHA_fback + gkStepPHA_fback, 1)
printks "PHA Fback = %.2f\n", 0, gkPHA_fback
elseif (gkLastToggle == 10) then
gkGPSdly = min(gkGPSdly + 0.001, 0.1)
printks "GPS Dly = %.3f\n", 0, gkGPSdly
elseif (gkLastToggle == 18) then
gkMOOD_Dly = min(gkMOOD_Dly + gkStepMOOD_Dly, 6)
printks "MOOD Delay = %.1f\n", 0, gkMOOD_Dly
endif
elseif (kVal < 64) then
if (gkLastToggle == 1) then
gkSemiDel = gkSemiDel - gkStepSemiDel
gkSemiDel limit gkSemiDel, -5, 12
printks "Delay Semi = %.2f\n", 0, gkSemiDel
elseif (gkLastToggle == 2) then
gkSpecDelFB = max(gkSpecDelFB - gkStepSpecDelFB, 0)
printks "Spec FB = %.2f\n", 0, gkSpecDelFB
elseif (gkLastToggle == 3) then
gkSTUTbpm = gkSTUTbpm - gkStepSTUTbpm
gkSTUTbpm limit gkSTUTbpm, 40, 400
printks "STUT BPM = %.1f\n", 0, gkSTUTbpm
elseif (gkLastToggle == 12) then
gkChorusDepth = max(gkChorusDepth - gkStepChorusDepth, 0)
printks "Chorus Depth = %.2f\n", 0, gkChorusDepth
elseif (gkLastToggle == 15) then
gkFLA_fback = max(gkFLA_fback - gkStepFLA_fback, -1)
printks "FLA Fback = %.2f\n", 0, gkFLA_fback
elseif (gkLastToggle == 16) then
gkPHA_fback = max(gkPHA_fback - gkStepPHA_fback, 0)
printks "PHA Fback = %.2f\n", 0, gkPHA_fback
elseif (gkLastToggle == 10) then
gkGPSdly = max(gkGPSdly - 0.001, 0.001)
printks "GPS Dly = %.3f\n", 0, gkGPSdly
elseif (gkLastToggle == 18) then
gkMOOD_Dly = max(gkMOOD_Dly - gkStepMOOD_Dly, 0)
printks "MOOD Delay = %.1f\n", 0, gkMOOD_Dly
endif
endif
elseif (kCC == 73) then
if (kVal > 64) then
gkDistAmtAcc = min(gkDistAmtAcc + gkStepDistAmt, 1)
elseif (kVal < 64) then
gkDistAmtAcc = max(gkDistAmtAcc - gkStepDistAmt, 0)
endif
gkDistAmt = 0.010 + (1.0 - 0.010) * (1 - gkDistAmtAcc)
printks "Dist Amt = %.3f (acc=%.3f)\n", 0, gkDistAmt, gkDistAmtAcc
gkBitsAmtAcc = max(0, gkDistAmtAcc - 0.5) / 0.5
gkBitsBC = int(16 - 15*gkBitsAmtAcc)
gkBitsBC limit gkBitsBC, 1, 16
printks "Bits = %d\n", 0, gkBitsBC
elseif (kCC == 86) then
gkAUTOMOOG_bypass = (kVal < 64 ? 1 : 0)
if (gkAUTOMOOG_bypass == 0) then
gkLastToggle = 13
endif
elseif (kCC == 76) then
if (kVal > 64) then
gkAUTOMOOG_sensAcc = min(gkAUTOMOOG_sensAcc + gkStepAUTOMOOG_sens, 1)
elseif (kVal < 64) then
gkAUTOMOOG_sensAcc = max(gkAUTOMOOG_sensAcc - gkStepAUTOMOOG_sens, 0)
endif
gkAUTOMOOG_sens = gkAUTOMOOG_sensAcc
printks "AUTOMOOG Sens = %.2f\n", 0, gkAUTOMOOG_sens
elseif (kCC == 99) then
gkVIB_bypass = (kVal < 64 ? 1 : 0)
if (gkVIB_bypass == 0) then
gkLastToggle = 14
endif
elseif (kCC == 89) then
if (kVal > 64) then
gkVIB_depthAcc = min(gkVIB_depthAcc + gkStepVIB_depth, 1)
elseif (kVal < 64) then
gkVIB_depthAcc = max(gkVIB_depthAcc - gkStepVIB_depth, 0)
endif
gkVIB_depth = gkVIB_depthAcc * 0.03
printks "VIB Depth = %.4f\n", 0, gkVIB_depth
elseif (kCC == 47) then
gkFLA_bypass = (kVal < 64 ? 1 : 0)
if (gkFLA_bypass == 0) then
gkLastToggle = 15
endif
elseif (kCC == 37) then
if (kVal > 64) then
gkFLA_depthAcc = min(gkFLA_depthAcc + gkStepFLA_depth, 1)
elseif (kVal < 64) then
gkFLA_depthAcc = max(gkFLA_depthAcc - gkStepFLA_depth, 0)
endif
gkFLA_depth = gkFLA_depthAcc * 0.01
printks "FLA Depth = %.4f\n", 0, gkFLA_depth
elseif (kCC == 46) then
gkPHA_bypass = (kVal < 64 ? 1 : 0)
if (gkPHA_bypass == 0) then
gkLastToggle = 16
endif
elseif (kCC == 36) then
if (kVal > 64) then
gkPHA_depthAcc = min(gkPHA_depthAcc + gkStepPHA_depth, 1)
elseif (kVal < 64) then
gkPHA_depthAcc = max(gkPHA_depthAcc - gkStepPHA_depth, 0)
endif
gkPHA_depth = gkPHA_depthAcc
printks "PHA Depth = %.2f\n", 0, gkPHA_depth
elseif (kCC == 88) then
if (kVal > 64) then
gkChorusDepthAcc = min(gkChorusDepthAcc + gkStepChorusDepth, 1)
elseif (kVal < 64) then
gkChorusDepthAcc = max(gkChorusDepthAcc - gkStepChorusDepth, 0)
endif
gkChorusDepth = gkChorusDepthAcc
printks "Chorus Depth = %.2f\n", 0, gkChorusDepth
elseif (kCC == 42) then
gkLeslieBypass = (kVal < 64 ? 1 : 0)
if (gkLeslieBypass == 0) then
gkLastToggle = 17
endif
elseif (kCC == 32) then
if (kVal > 64) then
gkLeslieDepthAcc = min(gkLeslieDepthAcc + gkStepLeslieDepth, 1)
elseif (kVal < 64) then
gkLeslieDepthAcc = max(gkLeslieDepthAcc - gkStepLeslieDepth, 0)
endif
gkLeslieDepth = gkLeslieDepthAcc
printks "Leslie Depth = %.2f\n", 0, gkLeslieDepth
elseif (kCC == 81) then
gkFormantBypass = (kVal < 64 ? 1 : 0)
if (gkFormantBypass == 0) then
gkLastToggle = 19
endif
elseif (kCC == 85) then
gkATTK_bypass = (kVal < 64 ? 1 : 0)
printks "ATTACK bypass = %d\n", 0, gkATTK_bypass
elseif (kCC == 75) then
if (kVal > 64) then
gkATTK_sens min gkATTK_sens + gkStepATTK_sens, 1.0
elseif (kVal < 64) then
gkATTK_sens max gkATTK_sens - gkStepATTK_sens, 0.1
endif
printks "ATTACK sens = %.2f\n", 0, gkATTK_sens
elseif (kCC == 71) then
if (kVal > 64) then
gkATTK_holdMs min gkATTK_holdMs + gkStepATTK_hold, 2000
elseif (kVal < 64) then
gkATTK_holdMs max gkATTK_holdMs - gkStepATTK_hold, 100
endif
printks "ATTACK holdMs = %.2f\n", 0, gkATTK_holdMs
endif
endif
endin
instr 1
aInL inch 1
aInR inch 1
gaATTK_sendL = aInL
gaATTK_sendR = aInR
kATTKByp = gkATTK_bypass
kPrevATTK init -1
if (kPrevATTK == -1) then
if (kATTKByp == 0) then
event "i", 150, 0, -1
else
gaATTK_outL = gaATTK_sendL
gaATTK_outR = gaATTK_sendR
endif
else
if (kPrevATTK == 1 && kATTKByp == 0) then
event "i", 150, 0, -1
elseif (kPrevATTK == 0 && kATTKByp == 1) then
turnoff2 150, 0, 1
gaATTK_outL = gaATTK_sendL
gaATTK_outR = gaATTK_sendR
endif
endif
kPrevATTK = kATTKByp
if (kATTKByp == 1) then
gaATTK_outL = gaATTK_sendL
gaATTK_outR = gaATTK_sendR
endif
aPostATTKL ntrpol gaATTK_sendL, gaATTK_outL, gkATTK_mix
aPostATTKR ntrpol gaATTK_sendR, gaATTK_outR, gkATTK_mix
gaPSSendL = aPostATTKL
gaPSSendR = aPostATTKR
kPSByp = gkBypassPS
kPrevPS init -1
if (kPrevPS == -1) then
if (kPSByp < 0.5) then
event "i", 50, 0, -1
else
gaPSOutL = gaPSSendL
gaPSOutR = gaPSSendR
endif
else
if (kPrevPS >= 0.5 && kPSByp < 0.5) then
event "i", 50, 0, -1
elseif (kPrevPS < 0.5 && kPSByp >= 0.5) then
turnoff2 50, 0, 1
gaPSOutL = gaPSSendL
gaPSOutR = gaPSSendR
endif
endif
kPrevPS = kPSByp
if (kPSByp >= 0.5) then
gaPSOutL = gaPSSendL
gaPSOutR = gaPSSendR
endif
kPSPct = gkDryPctPS
aPostPSL = ((100 - kPSPct)*0.01)*aPostATTKL + (kPSPct*0.01)*gaPSOutL
aPostPSR = ((100 - kPSPct)*0.01)*aPostATTKR + (kPSPct*0.01)*gaPSOutR
gaGPSSendL = aPostPSL
gaGPSSendR = aPostPSR
kGPSByp = gkBypassGPS
kPrevGPS init -1
if (kPrevGPS == -1) then
if (kGPSByp < 0.5) then
event "i", 55, 0, -1
else
gaGPSWetL = gaGPSSendL
gaGPSWetR = gaGPSSendR
endif
else
if (kPrevGPS >= 0.5 && kGPSByp < 0.5) then
event "i", 55, 0, -1
elseif (kPrevGPS < 0.5 && kGPSByp >= 0.5) then
turnoff2 55, 0, 1
gaGPSWetL = gaGPSSendL
gaGPSWetR = gaGPSSendR
endif
endif
kPrevGPS = kGPSByp
if (kGPSByp >= 0.5) then
gaGPSWetL = gaGPSSendL
gaGPSWetR = gaGPSSendR
endif
aPostGPSL ntrpol gaGPSSendL, gaGPSWetL, gkGPSmix
aPostGPSR ntrpol gaGPSSendR, gaGPSWetR, gkGPSmix
gaPHA_sendL = aPostGPSL
gaPHA_sendR = aPostGPSR
kPHAByp = gkPHA_bypass
kPrevPHA init -1
if (kPrevPHA == -1) then
if (kPHAByp == 0) then
event "i", 233, 0, -1
else
gaPHA_outL = gaPHA_sendL
gaPHA_outR = gaPHA_sendR
endif
else
if (kPrevPHA == 1 && kPHAByp == 0) then
event "i", 233, 0, -1
elseif (kPrevPHA == 0 && kPHAByp == 1) then
turnoff2 233, 0, 1
gaPHA_outL = gaPHA_sendL
gaPHA_outR = gaPHA_sendR
endif
endif
kPrevPHA = kPHAByp
if (kPHAByp == 1) then
gaPHA_outL = gaPHA_sendL
gaPHA_outR = gaPHA_sendR
endif
aPostPHAL = gaPHA_outL
aPostPHAR = gaPHA_outR
gaFLA_sendL = aPostPHAL
gaFLA_sendR = aPostPHAR
kFLAByp = gkFLA_bypass
kPrevFLA init -1
if (kPrevFLA == -1) then
if (kFLAByp == 0) then
event "i", 222, 0, -1
else
gaFLA_outL = gaFLA_sendL
gaFLA_outR = gaFLA_sendR
endif
else
if (kPrevFLA == 1 && kFLAByp == 0) then
event "i", 222, 0, -1
elseif (kPrevFLA == 0 && kFLAByp == 1) then
turnoff2 222, 0, 1
gaFLA_outL = gaFLA_sendL
gaFLA_outR = gaFLA_sendR
endif
endif
kPrevFLA = kFLAByp
if (kFLAByp == 1) then
gaFLA_outL = gaFLA_sendL
gaFLA_outR = gaFLA_sendR
endif
aPostFLAL = gaFLA_outL
aPostFLAR = gaFLA_outR
gaChorusSendL = aPostFLAL
gaChorusSendR = aPostFLAR
kChorusByp = gkBypassChorus
kPrevChorus init -1
if (kPrevChorus == -1) then
if (kChorusByp < 0.5) then
event "i", 40, 0, -1
else
gaChorusWetL = gaChorusSendL
gaChorusWetR = gaChorusSendR
endif
else
if (kPrevChorus >= 0.5 && kChorusByp < 0.5) then
event "i", 40, 0, -1
elseif (kPrevChorus < 0.5 && kChorusByp >= 0.5) then
turnoff2 40, 0, 1
gaChorusWetL = gaChorusSendL
gaChorusWetR = gaChorusSendR
endif
endif
kPrevChorus = kChorusByp
if (kChorusByp >= 0.5) then
gaChorusWetL = gaChorusSendL
gaChorusWetR = gaChorusSendR
endif
aPostChorusL ntrpol gaChorusSendL, gaChorusWetL, gkChorusMix
aPostChorusR ntrpol gaChorusSendR, gaChorusWetR, gkChorusMix
gaVIB_sendL = aPostChorusL
gaVIB_sendR = aPostChorusR
kVIBByp = gkVIB_bypass
kPrevVIB init -1
if (kPrevVIB == -1) then
if (kVIBByp == 0) then
event "i", 211, 0, -1
else
gaVIB_outL = gaVIB_sendL
gaVIB_outR = gaVIB_sendR
endif
else
if (kPrevVIB == 1 && kVIBByp == 0) then
event "i", 211, 0, -1
elseif (kPrevVIB == 0 && kVIBByp == 1) then
turnoff2 211, 0, 1
gaVIB_outL = gaVIB_sendL
gaVIB_outR = gaVIB_sendR
endif
endif
kPrevVIB = kVIBByp
if (kVIBByp == 1) then
gaVIB_outL = gaVIB_sendL
gaVIB_outR = gaVIB_sendR
endif
aPostVIBL = gaVIB_outL
aPostVIBR = gaVIB_outR
gaLeslieSendL = aPostVIBL
gaLeslieSendR = aPostVIBR
kLeslieByp = gkLeslieBypass
kPrevLeslie init -1
if (kPrevLeslie == -1) then
if (kLeslieByp == 0) then
event "i", 300, 0, -1
else
gaLeslieOutL = gaLeslieSendL
gaLeslieOutR = gaLeslieSendR
endif
else
if (kPrevLeslie == 1 && kLeslieByp == 0) then
event "i", 300, 0, -1
elseif (kPrevLeslie == 0 && kLeslieByp == 1) then
turnoff2 300, 0, 1
gaLeslieOutL = gaLeslieSendL
gaLeslieOutR = gaLeslieSendR
endif
endif
kPrevLeslie = kLeslieByp
if (kLeslieByp == 1) then
gaLeslieOutL = gaLeslieSendL
gaLeslieOutR = gaLeslieSendR
endif
aPostLeslieL = gaLeslieOutL
aPostLeslieR = gaLeslieOutR
gaDistSendL = aPostLeslieL
gaDistSendR = aPostLeslieR
kDistByp = gkBypassDist
kPrevDist init -1
if (kPrevDist == -1) then
if (kDistByp < 0.5) then
event "i", 60, 0, -1
else
gaDistOutL = gaDistSendL
gaDistOutR = gaDistSendR
endif
else
if (kPrevDist >= 0.5 && kDistByp < 0.5) then
event "i", 60, 0, -1
elseif (kPrevDist < 0.5 && kDistByp >= 0.5) then
turnoff2 60, 0, 1
gaDistOutL = gaDistSendL
gaDistOutR = gaDistSendR
endif
endif
kPrevDist = kDistByp
kDistEng = (kDistByp < 0.5 ? 1 : 0)
aPostDistL = kDistEng*gaDistOutL + (1-kDistEng)*gaDistSendL
aPostDistR = kDistEng*gaDistOutR + (1-kDistEng)*gaDistSendR
gaTSSendL = aPostDistL
gaTSSendR = aPostDistR
kTSByp = gkBypassTS
kPrevTS init -1
if (kPrevTS == -1) then
if (kTSByp < 0.5) then
event "i", 100, 0, -1
else
gaTSOutL = gaTSSendL
gaTSOutR = gaTSSendR
endif
else
if (kPrevTS >= 0.5 && kTSByp < 0.5) then
event "i", 100, 0, -1
elseif (kPrevTS < 0.5 && kTSByp >= 0.5) then
turnoff2 100, 0, 1
gaTSOutL = gaTSSendL
gaTSOutR = gaTSSendR
endif
endif
kPrevTS = kTSByp
kTSEng = (kTSByp < 0.5 ? 1 : 0)
aPostTSL = kTSEng*gaTSOutL + (1-kTSEng)*gaTSSendL
aPostTSR = kTSEng*gaTSOutR + (1-kTSEng)*gaTSSendR
gaFormantSendL = aPostTSL
gaFormantSendR = aPostTSR
kFormByp = gkFormantBypass
kPrevForm init -1
if (kPrevForm == -1) then
if (kFormByp == 0) then
event "i", 120, 0, -1
else
gaFormantOutL = gaFormantSendL
gaFormantOutR = gaFormantSendR
endif
else
if (kPrevForm == 1 && kFormByp == 0) then
event "i", 120, 0, -1
elseif (kPrevForm == 0 && kFormByp == 1) then
turnoff2 120, 0, 1
gaFormantOutL = gaFormantSendL
gaFormantOutR = gaFormantSendR
endif
endif
kPrevForm = kFormByp
if (kFormByp == 1) then
gaFormantOutL = gaFormantSendL
gaFormantOutR = gaFormantSendR
endif
aPostFormL ntrpol gaFormantSendL, gaFormantOutL, gkFormantMix
aPostFormR ntrpol gaFormantSendR, gaFormantOutR, gkFormantMix
gaRMSendL = aPostFormL
gaRMSendR = aPostFormR
kRMByp = gkBypassRM
kPrevRM init -1
if (kPrevRM == -1) then
if (kRMByp < 0.5) then
event "i", 65, 0, -1
else
gaRMOutL = gaRMSendL
gaRMOutR = gaRMSendR
endif
else
if (kPrevRM >= 0.5 && kRMByp < 0.5) then
event "i", 65, 0, -1
elseif (kPrevRM < 0.5 && kRMByp >= 0.5) then
turnoff2 65, 0, 1
gaRMOutL = gaRMSendL
gaRMOutR = gaRMSendR
endif
endif
kPrevRM = kRMByp
kRMEng = (kRMByp < 0.5 ? 1 : 0)
aPostRML = kRMEng*gaRMOutL + (1-kRMEng)*gaRMSendL
aPostRMR = kRMEng*gaRMOutR + (1-kRMEng)*gaRMSendR
gaAUTOMOOG_sendL = aPostRML
gaAUTOMOOG_sendR = aPostRMR
kAutoByp = gkAUTOMOOG_bypass
kPrevAuto init -1
if (kPrevAuto == -1) then
if (kAutoByp == 0) then
event "i", 51, 0, -1
else
gaAUTOMOOG_outL = gaAUTOMOOG_sendL
gaAUTOMOOG_outR = gaAUTOMOOG_sendR
endif
else
if (kPrevAuto == 1 && kAutoByp == 0) then
event "i", 51, 0, -1
elseif (kPrevAuto == 0 && kAutoByp == 1) then
turnoff2 51, 0, 1
gaAUTOMOOG_outL = gaAUTOMOOG_sendL
gaAUTOMOOG_outR = gaAUTOMOOG_sendR
endif
endif
kPrevAuto = kAutoByp
if (kAutoByp == 1) then
gaAUTOMOOG_outL = gaAUTOMOOG_sendL
gaAUTOMOOG_outR = gaAUTOMOOG_sendR
endif
aPostAutoL = gaAUTOMOOG_outL
aPostAutoR = gaAUTOMOOG_outR
if (gkVOLpedalBypass < 0.5) then
aVolL = aPostAutoL * gkVOLpedal
aVolR = aPostAutoR * gkVOLpedal
else
aVolL = aPostAutoL
aVolR = aPostAutoR
endif
gaSpecDelSendL = aVolL
gaSpecDelSendR = aVolR
kSpecByp = gkSpecDelBypass
kPrevSpec init -1
if (kPrevSpec == -1) then
if (kSpecByp < 0.5) then
event "i", 200, 0, -1
else
gaSpecDelWetL = gaSpecDelSendL
gaSpecDelWetR = gaSpecDelSendR
endif
else
if (kPrevSpec >= 0.5 && kSpecByp < 0.5) then
event "i", 200, 0, -1
elseif (kPrevSpec < 0.5 && kSpecByp >= 0.5) then
turnoff2 200, 0, 1
gaSpecDelWetL = gaSpecDelSendL
gaSpecDelWetR = gaSpecDelSendR
endif
endif
kPrevSpec = kSpecByp
if (kSpecByp >= 0.5) then
gaSpecDelWetL = gaSpecDelSendL
gaSpecDelWetR = gaSpecDelSendR
endif
aAfterSpecL ntrpol gaSpecDelSendL, gaSpecDelWetL, gkSpecDelMix
aAfterSpecR ntrpol gaSpecDelSendR, gaSpecDelWetR, gkSpecDelMix
gaMOOD_SendL = aAfterSpecL
gaMOOD_SendR = aAfterSpecR
kMOODByp = gkMOOD_Bypass
kPrevMOOD init -1
if (kPrevMOOD == -1) then
if (kMOODByp == 0) then
event "i", 401, 0, -1
event "i", 402, 0, -1
event "i", 403, 0, -1
event "i", 404, 0, -1
event "i", 405, 0, -1
else
gaMOOD_FinalL = gaMOOD_SendL
gaMOOD_FinalR = gaMOOD_SendR
endif
else
if (kPrevMOOD == 1 && kMOODByp == 0) then
event "i", 401, 0, -1
event "i", 402, 0, -1
event "i", 403, 0, -1
event "i", 404, 0, -1
event "i", 405, 0, -1
elseif (kPrevMOOD == 0 && kMOODByp == 1) then
turnoff2 401, 0, 1
turnoff2 402, 0, 1
turnoff2 403, 0, 1
turnoff2 404, 0, 1
turnoff2 405, 0, 1
gaMOOD_FinalL = gaMOOD_SendL
gaMOOD_FinalR = gaMOOD_SendR
endif
endif
kPrevMOOD = kMOODByp
if (kMOODByp == 1) then
gaMOOD_FinalL = gaMOOD_SendL
gaMOOD_FinalR = gaMOOD_SendR
endif
aMoodL ntrpol gaMOOD_SendL, gaMOOD_FinalL, gkMOOD_Mix
aMoodR ntrpol gaMOOD_SendR, gaMOOD_FinalR, gkMOOD_Mix
gaGranSendL = aMoodL
gaGranSendR = aMoodR
kGranByp = gkGranBypass
kPrevGran init -1
if (kPrevGran == -1) then
if (kGranByp < 0.5) then
event "i", 80, 0, -1
else
gaGranWetL = gaGranSendL
gaGranWetR = gaGranSendR
endif
else
if (kPrevGran >= 0.5 && kGranByp < 0.5) then
event "i", 80, 0, -1
elseif (kPrevGran < 0.5 && kGranByp >= 0.5) then
turnoff2 80, 0, 1
gaGranWetL = gaGranSendL
gaGranWetR = gaGranSendR
endif
endif
kPrevGran = kGranByp
if (kGranByp >= 0.5) then
gaGranWetL = gaGranSendL
gaGranWetR = gaGranSendR
endif
aAfterGranL ntrpol gaGranSendL, gaGranWetL, gkGranMix
aAfterGranR ntrpol gaGranSendR, gaGranWetR, gkGranMix
gaDelSendL = aAfterGranL
gaDelSendR = aAfterGranR
kDelByp = gkBypassDelay
kPrevDel init -1
if (kPrevDel == -1) then
if (kDelByp < 0.5) then
event "i", 62, 0, -1
else
gaDelOutL = gaDelSendL
gaDelOutR = gaDelSendR
endif
else
if (kPrevDel >= 0.5 && kDelByp < 0.5) then
event "i", 62, 0, -1
elseif (kPrevDel < 0.5 && kDelByp >= 0.5) then
turnoff2 62, 0, 1
gaDelOutL = gaDelSendL
gaDelOutR = gaDelSendR
endif
endif
kPrevDel = kDelByp
kDelEng = (kDelByp < 0.5 ? 1 : 0)
aPostDelL = kDelEng*gaDelOutL + (1-kDelEng)*gaDelSendL
aPostDelR = kDelEng*gaDelOutR + (1-kDelEng)*gaDelSendR
gaVerbSendL = aPostDelL
gaVerbSendR = aPostDelR
kVerbByp = gkverbBypass
kPrevVerb init -1
if (kPrevVerb == -1) then
if (kVerbByp < 0.5) then
event "i", 70, 0, -1
else
gaVerbWetL = 0
gaVerbWetR = 0
endif
else
if (kPrevVerb >= 0.5 && kVerbByp < 0.5) then
event "i", 70, 0, -1
elseif (kPrevVerb < 0.5 && kVerbByp >= 0.5) then
turnoff2 70, 0, 1
gaVerbWetL = 0
gaVerbWetR = 0
endif
endif
kPrevVerb = kVerbByp
kVerbEng = (kVerbByp < 0.5 ? 1 : 0)
kverbMixUse = gkverbMix * kVerbEng
aPreSTUTL ntrpol gaVerbSendL, gaVerbWetL, kverbMixUse
aPreSTUTR ntrpol gaVerbSendR, gaVerbWetR, kverbMixUse
kBypST = gkSTUTbypass
kPrevST init -1
if (kPrevST == -1) then
if (kBypST < 0.5) then
event "i", 90, 0, -1
endif
else
if (kPrevST >= 0.5 && kBypST < 0.5) then
event "i", 90, 0, -1
elseif (kPrevST < 0.5 && kBypST >= 0.5) then
turnoff2 90, 0, 1
endif
endif
kPrevST = kBypST
gaSTUTdryL = aPreSTUTL
gaSTUTdryR = aPreSTUTR
if (kBypST >= 0.5) then
gaSTUTwetL = aPreSTUTL
gaSTUTwetR = aPreSTUTR
endif
aOutL ntrpol gaSTUTdryL, gaSTUTwetL, gkSTUTdrywet
aOutR ntrpol gaSTUTdryR, gaSTUTwetR, gkSTUTdrywet
outs aOutL, aOutR
clear gaGranWetL, gaGranWetR
clear gaMOOD_PitchL, gaMOOD_PitchR
clear gaMOOD_Bit1L, gaMOOD_Bit1R
clear gaMOOD_GranL, gaMOOD_GranR
clear gaMOOD_FinalL, gaMOOD_FinalR
endin
instr 401
aInL = gaMOOD_SendL
aInR = gaMOOD_SendR
ginsamp = ftlen(giPitchBufL) - 1
aphsW phasor sr / ginsamp
tablew aInL, aphsW, giPitchBufL,1
tablew aInR, aphsW, giPitchBufR,1
klen = ftlen(giSemi) - 1
kIndex = round(gkMOOD_Clock * klen)
ksemi table kIndex, giSemi
kratio = semitone(ksemi)
iwsize = 2048
irnd = 256
iolaps = 50
iwfn = giHalfSine
ikdly = 0.01
iMaxDur = (iwsize + irnd) / sr
kTransComp limit iMaxDur * (kratio - 1), 0, ginsamp / sr
kRndDly betarand ikdly,1,1
kRndDlyR betarand ikdly,1,1
kdelay = (kTransComp + kRndDly) / (ginsamp / sr)
kdelayR = (kTransComp + kRndDlyR) / (ginsamp / sr)
kmax_read = (ginsamp - iwsize - irnd) / ginsamp
aphsR wrap aphsW - kdelay, 0, kmax_read
aphsR_R wrap aphsW - kdelayR, 0, kmax_read
ktime = aphsR * (ginsamp / sr)
ktimeR = aphsR_R * (ginsamp / sr)
aShiftL, acL sndwarp 1, ktime, kratio, giPitchBufL, 0, iwsize, irnd, iolaps, iwfn, 1
aShiftL balance aShiftL, acL
aShiftR, acR sndwarp 1, ktimeR, kratio, giPitchBufR, 0, iwsize, irnd, iolaps, iwfn, 1
aShiftR balance aShiftR, acR
gaMOOD_PitchL = aShiftL
gaMOOD_PitchR = aShiftR
endin
instr 402
aInL = gaMOOD_PitchL
aInR = gaMOOD_PitchR
klen = ftlen(giSemi) - 1
kIndex = round(gkMOOD_Clock * klen)
ksemi table kIndex, giSemi
kTrig changed2 ksemi
kred_sr init sr
kbits init 16
kRandFactor init 1
if (kTrig == 1 && ksemi < 0) then
kRandFactor random 0.7, 1.3
endif
if (ksemi < 0) then
kBaseRed = 1
if (ksemi == -7) then
kBaseRed = 0.5
elseif (ksemi == -12) then
kBaseRed = 0.333
elseif (ksemi == -19) then
kBaseRed = 0.25
elseif (ksemi == -24) then
kBaseRed = 0.125
elseif (ksemi == -31) then
kBaseRed = 0.0625
endif
kred_sr = sr * kBaseRed * kRandFactor
kbits = (ksemi == -7 ? 12 : (ksemi == -12 ? 11 : (ksemi == -19 ? 10 : (ksemi == -24 ? 9 : 8))))
endif
if (ksemi >= 0) then
gaMOOD_Bit1L = aInL
gaMOOD_Bit1R = aInR
else
klevels = pow(2, kbits - 1)
a_quantL = round(aInL * klevels) / klevels
a_quantR = round(aInR * klevels) / klevels
a_crushL SrReducer a_quantL, kred_sr
a_crushR SrReducer a_quantR, kred_sr
gaMOOD_Bit1L = a_crushL
gaMOOD_Bit1R = a_crushR
endif
endin
instr 403
aInL = gaMOOD_Bit1L
aInR = gaMOOD_Bit1R
ilen = ftlen(giBufL)
aptr phasor sr / ilen
aptr *= ilen
tablew aInL, aptr, giBufL
tablew aInR, aptr, giBufR
kptr downsamp aptr
kGSize1 = 0.005
kGSize2 = 0.8
kDens = gkMOOD_Dens
kDly1 = 0
kDly2 = gkMOOD_Dly
kPanSpread = 1
kAmpSpread = 1
kFiltSpread = 1
kampdecay = 0.5
kreverse = 0.8
kwindow = 1
kDlyDst = 1
klevel = 1
kkr = sr / ksmps
kmaxfreq = kkr
kfreq limit kDens, 0, kmaxfreq
ktrig metro kfreq
if (ktrig == 0) kgoto skip
kdens_per_trig = kDens / kfreq
kint = int(kdens_per_trig)
kfrac = kdens_per_trig - kint
krnd random 0, 1
knum = kint + (krnd < kfrac ? 1 : 0)
kcount = 0
while (kcount < knum) do
kRnd random 0, 1
kGSize = kGSize1 + kRnd * (kGSize2 - kGSize1)
kRndDly random 0, 1
kRndDly expcurve kRndDly, 100
kDly = kDly1 + kRndDly * (kDly2 - kDly1)
schedkwhen 1, 0, 0, 404, kDly + 0.0001, kGSize, kptr, kPanSpread, kreverse, kDly, kDly1 + 1/sr, kDly2 + 0.0001, kampdecay, klevel, kwindow, kAmpSpread, kFiltSpread
kcount += 1
od
skip:
endin
instr 404
iGStart = p4
ispread = p5
ireverse = (random(0, 1) > p6 ? 1 : -1)
iwindow table p12 - 1, giwindows
idly = p7
iMinDly = p8
iMaxDly = p9
iampdecay = p10
ilevel = p11
iAmpSpr = p13
iFiltSpr = p14
iRtoAmp divz idly - iMinDly, iMaxDly - iMinDly, 0
iamp = (1 - iRtoAmp) ^ 2
iamp ntrpol 1, iamp, iampdecay
iRndAmp random 1 - iAmpSpr, 1
iamp = iamp * iRndAmp
aenv oscili iamp, 1/p3, iwindow
aline line iGStart, p3, iGStart + (p3 * sr * ireverse)
aL tablei aline, giBufL, 0, 0, 1
aR tablei aline, giBufR, 0, 0, 1
if iFiltSpr > 0 then
iRndCfOct random 14 - (iFiltSpr * 10), 14
iRndCf = cpsoct(iRndCfOct)
aL butlp aL, iRndCf
aR butlp aR, iRndCf
endif
ipan random 0.5 - (ispread * 0.5), 0.5 + (ispread * 0.5)
gaMOOD_GranL += aL * aenv * ipan * ilevel
gaMOOD_GranR += aR * aenv * (1 - ipan) * ilevel
endin
instr 405
aInL = gaMOOD_GranL
aInR = gaMOOD_GranR
klen = ftlen(giSemi) - 1
kIndex = round(gkMOOD_Clock * klen)
ksemi table kIndex, giSemi
kTrig changed2 ksemi
kred_sr init sr
kbits init 16
kRandFactor init 1
if (kTrig == 1 && ksemi < 0) then
kRandFactor random 0.7, 1.3
endif
if (ksemi < 0) then
kBaseRed = 1
if (ksemi == -7) then
kBaseRed = 0.5
elseif (ksemi == -12) then
kBaseRed = 0.333
elseif (ksemi == -19) then
kBaseRed = 0.25
elseif (ksemi == -24) then
kBaseRed = 0.125
elseif (ksemi == -31) then
kBaseRed = 0.0625
endif
kred_sr = sr * kBaseRed * kRandFactor
kbits = (ksemi == -7 ? 12 : (ksemi == -12 ? 11 : (ksemi == -19 ? 10 : (ksemi == -24 ? 9 : 8))))
endif
if (ksemi >= 0) then
gaMOOD_FinalL = aInL
gaMOOD_FinalR = aInR
else
klevels = pow(2, kbits - 1)
a_quantL = round(aInL * klevels) / klevels
a_quantR = round(aInR * klevels) / klevels
a_crushL SrReducer a_quantL, kred_sr
a_crushR SrReducer a_quantR, kred_sr
gaMOOD_FinalL = a_crushL
gaMOOD_FinalR = a_crushR
endif
endin
instr 51
ksens = gkAUTOMOOG_sens
katt = gkAUTOMOOG_att
krel = gkAUTOMOOG_rel
kfreq = gkAUTOMOOG_freq
ktype = gkAUTOMOOG_type
kres = gkAUTOMOOG_res
kdist = gkAUTOMOOG_dist
klevel = gkAUTOMOOG_level
a1 = gaAUTOMOOG_sendL
a2 = gaAUTOMOOG_sendR
 
a1 EnvelopeFollower a1, ksens, katt, krel, kfreq, ktype, kres * 0.95, kdist * 100
a2 EnvelopeFollower a2, ksens, katt, krel, kfreq, ktype, kres * 0.95, kdist * 100
a1 = a1 * klevel * (1 - ((kdist * 0.3) ^ 0.02)) ; scale amplitude according to distortion level (to compensate for gain increases it applies)
a2 = a2 * klevel * (1 - ((kdist * 0.3) ^ 0.02))
gaAUTOMOOG_outL = a1
gaAUTOMOOG_outR = a2
endin
instr 50
aInL = gaPSSendL
aInR = gaPSSendR
kSemi = gkSemi
kRatio = pow(2, kSemi/12)
aPSL, aPSR pitchshifter4 aInL, aInR, kRatio, 1, 0.07, 0.01, 4, giHanning, 1
aPSL *= 2
aPSR *= 2
gaPSOutL = aPSL
gaPSOutR = aPSR
endin
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
kpch = semitone(gkGPSsemis)
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
instr 60
aInL = gaDistSendL
aInR = gaDistSendR
kKnob = gkKnobRaw
kKnob limit kKnob, 0, 1
kShape = gkDistAmt
aDistL powershape aInL, kShape, 1
aDistR powershape aInR, kShape, 1
aDistL balance aDistL, aInL
aDistR balance aDistR, aInR
if gkBypassBC==0 then
aBitsOutL LoFi aDistL, gkBitsBC, 0
aBitsOutR LoFi aDistR, gkBitsBC, 0
else
aBitsOutL = aDistL
aBitsOutR = aDistR
endif
gaDistOutL = aBitsOutL
gaDistOutR = aBitsOutR
endin
instr 100
aInL = gaTSSendL
aInR = gaTSSendR
kDrive = 1 + 300 * gkTSAmtAcc
kLpCutoff = 2500 + 1000 * gkTSAmtAcc
aHpL = butterhp(aInL, 200)
aHpR = butterhp(aInR, 200)
aLpL = butterlp(aInL, 200)
aLpR = butterlp(aInR, 200)
aDistL = tanh(aHpL * kDrive) / tanh(kDrive)
aDistR = tanh(aHpR * kDrive) / tanh(kDrive)
aDistL = tone(aDistL, kLpCutoff)
aDistR = tone(aDistR, kLpCutoff)
aOutL = aDistL + aLpL
aOutR = aDistR + aLpR
kComp = .2 + 0.4556 * exp(-27.879 * gkTSAmtAcc)
aOutL *= kComp
aOutR *= kComp
gaTSOutL = aOutL
gaTSOutR = aOutR
endin
instr 62
aInL = gaDelSendL
aInR = gaDelSendR
 
kDelay portk gkDelayMS/1000, gkPortTime
aDelay interp kDelay
 
kFbRaw portk gkFeedback, gkPortTime
kFb = kFbRaw * 1.2
kFb limit kFb, 0, 1.2
 
aMemL init 0
aMemR init 0
 
iMaxDel = 800
aDelay limit aDelay, 0, iMaxDel
 
if (gkBypassDelay >= 0.5) then
gaDelOutL = aInL
gaDelOutR = aInR
else
aSumL = aInL + aMemL * kFb
aSumR = aInR + aMemR * kFb
 
aDelL vdelay3 aSumL, aDelay, iMaxDel
aDelR vdelay3 aSumR, aDelay, iMaxDel
 
kRatio = pow(2, gkSemiDel/12)
aShiftL, aShiftR pitchshifter4 aDelL, aDelR, kRatio, 1, 0.07, 0.01, 4, giHanning, 1
aShiftL *= 2
aShiftR *= 2
 
aMemL = aShiftL
aMemR = aShiftR
 
gaDelOutL = gkDelMix * aShiftL + (1 - gkDelMix) * aInL
gaDelOutR = gkDelMix * aShiftR + (1 - gkDelMix) * aInR
endif
endin
instr 120
aInL = gaFormantSendL
aInR = gaFormantSendR
 
iFFTsize = 4096
ioverlap = iFFTsize / 8
iwinsize = iFFTsize
iwintype = 1
 
kshift port gkFormantShift, 0.05
kformscale = pow(2, kshift / 12)
 
knyquist = sr * 0.5
kcutoff = (kformscale > 1) ? (knyquist / kformscale) : knyquist
kcutoff port kcutoff, 0.01
 
aFiltL = aInL
aFiltR = aInR
aFiltL butterlp aFiltL, kcutoff
aFiltL butterlp aFiltL, kcutoff
aFiltL butterlp aFiltL, kcutoff
aFiltL butterlp aFiltL, kcutoff
aFiltR butterlp aFiltR, kcutoff
aFiltR butterlp aFiltR, kcutoff
aFiltR butterlp aFiltR, kcutoff
aFiltR butterlp aFiltR, kcutoff
 
fsigL pvsanal aFiltL, iFFTsize, ioverlap, iwinsize, iwintype
fsigR pvsanal aFiltR, iFFTsize, ioverlap, iwinsize, iwintype
 
f_natL pvscale fsigL, kformscale, 0, 1
f_natR pvscale fsigR, kformscale, 0, 1
 
f_formL pvscale f_natL, 1/kformscale, 1, 1, 512
f_formR pvscale f_natR, 1/kformscale, 1, 1, 512
 
aOutL pvsynth f_formL
aOutR pvsynth f_formR
 
aOutL balance aOutL, aInL
aOutR balance aOutR, aInR
 
gaFormantOutL = aOutL
gaFormantOutR = aOutR
endin
instr 80
aInL = gaGranSendL
aInR = gaGranSendR
 
ilen = ftlen(giBufL)
aptr phasor sr / ilen
aptr *= ilen
tablew aInL, aptr, giBufL
tablew aInR, aptr, giBufR
kptr downsamp aptr
 
kGSize1 = 0.005
kGSize2 = 0.8
kDens = 1200
kDly1 = 0
kDly2 = 0.7
kTrns1 = 0.1
kTrns2 = 12
kPanSpread = 1
kAmpSpread = 1
kFiltSpread = 1
kampdecay = 0.5
kreverse = 0.8
kwindow = 1
kDlyDst = 1
klevel = 1
 
kRnd random 0, 1
kGSize = kGSize1 + kRnd * (kGSize2 - kGSize1)
 
kRndDly random 0, 1
kRndDly expcurve kRndDly, 100
kDly = kDly1 + kRndDly * (kDly2 - kDly1)
 
ktrig metro kDens
schedkwhen ktrig, 0, 0, 81, kDly + 0.0001, kGSize, kptr, kPanSpread, kreverse, \
kDly, kDly1 + 1/sr, kDly2 + 0.0001, kampdecay, klevel, kwindow, \
kAmpSpread, kFiltSpread, kTrns1, kTrns2
 
clear gaGranWetL, gaGranWetR
endin
instr 81
iGStart = p4
ispread = p5
ireverse = (random(0, 1) > p6 ? 1 : -1)
iwindow table p12 - 1, giwindows
idly = p7
iMinDly = p8
iMaxDly = p9
iampdecay = p10
ilevel = p11
iAmpSpr = p13
iFiltSpr = p14
iTrns = (random(0, 1) < p15 ? p16 : 0)
iRto = semitone(iTrns)
 
iRtoAmp divz idly - iMinDly, iMaxDly - iMinDly, 0
iamp = (1 - iRtoAmp) ^ 2
iamp ntrpol 1, iamp, iampdecay
iRndAmp random 1 - iAmpSpr, 1
iamp = iamp * iRndAmp
aenv oscili iamp, 1/p3, iwindow
 
if iRto > 1 then
iStrtOS = (iRto - 1) * sr * p3
else
iStrtOS = 0
endif
aline line iGStart - iStrtOS, p3, iGStart - iStrtOS + (p3 * iRto * sr * ireverse)
aL tablei aline, giBufL, 0, 0, 1
aR tablei aline, giBufR, 0, 0, 1
 
if iFiltSpr > 0 then
iRndCfOct random 14 - (iFiltSpr * 10), 14
iRndCf = cpsoct(iRndCfOct)
aL butlp aL, iRndCf
aR butlp aR, iRndCf
endif
 
ipan random 0.5 - (ispread * 0.5), 0.5 + (ispread * 0.5)
gaGranWetL += aL * aenv * ipan * ilevel
gaGranWetR += aR * aenv * (1 - ipan) * ilevel
endin
instr 200
kMaxDelSm portk gkSpecDelMaxDel, gkPortTime
kFdbk portk gkSpecDelFB, gkPortTime
 
kMaxDelMapped = kMaxDelSm
 
kMaxDelQuant = int(kMaxDelMapped * 1000) * 0.001
 
if changed(kMaxDelQuant) == 1 then
reinit RESTART
endif
 
RESTART:
iFFT = giFFT
iHop = giHop
iWin = giWin
iFormat = giFormat
 
iReqMax = i(kMaxDelQuant)
 
iMin = iFFT / sr
iMaxDelay = (iReqMax < iMin ? iMin : iReqMax)
 
aInL = gaSpecDelSendL
aInR = gaSpecDelSendR
 
fsigOutL pvsinit iFFT, iHop, iWin, iFormat
fsigInL pvsanal aInL, iFFT, iHop, iWin, iFormat
fsigFBL pvsgain fsigOutL, kFdbk
fsigMixL pvsmix fsigInL, fsigFBL
iHandleL, kTimeL pvsbuffer fsigMixL, iMaxDelay
fsigOutL pvsbufread2 kTimeL, iHandleL, giSpecDelAmpTab, giSpecDelFrqTab
aWetL pvsynth fsigOutL
 
fsigOutR pvsinit iFFT, iHop, iWin, iFormat
fsigInR pvsanal aInR, iFFT, iHop, iWin, iFormat
fsigFBR pvsgain fsigOutR, kFdbk
fsigMixR pvsmix fsigInR, fsigFBR
iHandleR, kTimeR pvsbuffer fsigMixR, iMaxDelay
fsigOutR pvsbufread2 kTimeR, iHandleR, giSpecDelAmpTab, giSpecDelFrqTab
aWetR pvsynth fsigOutR
 
gaSpecDelWetL = aWetL
gaSpecDelWetR = aWetR
endin
instr 65
aInL = gaRMSendL
aInR = gaRMSendR
aLFO lfo gkARModDepth, gkARModFreq
gaRMOutL = aInL * (1 + aLFO)
gaRMOutR = aInR * (1 + aLFO)
endin
instr 70
kFB = gkReverbFB
iDampCtl = 0.9
iPitchM = 1
kDamp = iDampCtl
kDamp expcurve kDamp, 4
kDampHz scale kDamp, 20000, 20
aInL = gaVerbSendL
aInR = gaVerbSendR
denorm aInL, aInR
aWetL, aWetR reverbsc aInL, aInR, kFB, kDampHz, sr, i(iPitchM)
gaVerbWetL = aWetL
gaVerbWetR = aWetR
endin
instr 150
aL = gaATTK_sendL
aR = gaATTK_sendR
 
aL clip aL, 1, 0.9
aR clip aR, 1, 0.9
 
aMono = (aL + aR) * 0.5
 
aMonoHi butterhp aMono, 1200
aMonoPick butterbp aMono, 4000, 2000
aMonoLo butterlp aMono, 400
 
kSens = gkATTK_sens
kDetGain = 1 + 12 * kSens
 
aMonoHi = aMonoHi * kDetGain
aMonoPick = aMonoPick * kDetGain * 2.0
aMonoLo = aMonoLo * kDetGain
 
aMonoTrans = aMonoHi + aMonoPick
 
aMonoTrans limit aMonoTrans, -1, 1
aMonoLo limit aMonoLo, -1, 1
 
aEnvF follow2 aMonoTrans, 0.0003, 0.030
aEnvS follow2 aMonoTrans, 0.010, 0.150
aEnvF limit aEnvF, 0, 1
aEnvS limit aEnvS, 0, 1
 
kEnvF downsamp aEnvF
kEnvS downsamp aEnvS
kEnvF expcurve kEnvF, 0.3
kEnvS expcurve kEnvS, 0.3
kEnvFsm portk kEnvF, 0.003
kEnvSsm portk kEnvS, 0.040
 
kRise = max(kEnvFsm - kEnvSsm, 0)
 
aEnvSus follow2 aMonoLo, 0.030, 1.500
aEnvSus limit aEnvSus, 0, 1
kEnvSus downsamp aEnvSus
kEnvSus expcurve kEnvSus, 0.3
kEnvSusSm portk kEnvSus, 0.200
 
kTrHi = max(0.0005, 0.10 - 0.09*kSens)
kTrRise = max(0.00001, 0.002 - 0.0018*kSens)
kSusTh = max(gkATTK_noiseFloor, 0.020 - 0.015*kSens)
 
kDT = ksmps/sr
kAtkSec = max(0.005, gkATTK_atkMs/1000.0)
kDecSec = max(0.001, gkATTK_decMs/1000.0)
kRelSec = max(0.010, gkATTK_relMs/1000.0)
kKillSec = max(0.001, gkATTK_killMs/1000.0)
kHoldSec = max(0.1, gkATTK_holdMs/1000.0)
 
kPreSec = gkATTK_basePreSec + min(kAtkSec*0.05, gkATTK_maxPreSec - gkATTK_basePreSec)
kPreMs = kPreSec * 1000
aLd vdelay aL, kPreMs, giPreMaxMs
aRd vdelay aR, kPreMs, giPreMaxMs
 
kState init 0
kPhaseA init 0
kPhaseD init 0
kPhaseK init 0
kPhaseR init 0
kGain init 0
kSusTimer init 0
kMinSusTimer init 0
kHoldTimer init 0
kLock init 0
 
if (kState == 4) then
kTrHi = kTrHi * 2.5
kTrRise = kTrRise * 2.5
endif
 
if (kLock > 0) then
kLock max kLock - kDT, 0
endif
 
kTransNow = ((kEnvFsm > kTrHi) && (kRise > kTrRise) ? 1 : 0)
kPrevTrans init 0
kTransEdge = (kTransNow==1 && kPrevTrans==0 && kLock==0 ? 1 : 0)
kPrevTrans = kTransNow
 
if (kTransEdge == 1 && (kState == 0 || kState == 4 || kState == 5)) then
kState = 1
kPhaseK = 0
kLock = gkATTK_lockSec
kSusTimer = 0
kMinSusTimer = 0
kHoldTimer = 0
else
if (kState == 0) then
if (kEnvFsm > kSusTh || kEnvSusSm > kSusTh) then
kState = 2
kPhaseA = 0
endif
elseif (kState == 1) then
if (kPhaseK >= 1) then
kState = 2
kPhaseA = 0
endif
elseif (kState == 2) then
if (kPhaseA >= 1) then
kState = 3
kPhaseD = 0
endif
elseif (kState == 3) then
if (kPhaseD >= 1) then
kState = 4
kMinSusTimer = 0
kHoldTimer = 0
endif
elseif (kState == 4) then
kMinSusTimer = kMinSusTimer + kDT
kHoldTimer = kHoldTimer + kDT
if (kHoldTimer > kHoldSec) then
kState = 5
kPhaseR = 0
elseif (kEnvSusSm > kSusTh) then
kSusTimer = 0
else
if (kMinSusTimer < gkATTK_minSusSec) then
kSusTimer = 0
else
kSusTimer = kSusTimer + kDT
if (kSusTimer > gkATTK_susHoldSec) then
kState = 5
kPhaseR = 0
endif
endif
endif
elseif (kState == 5) then
if (kPhaseR >= 1) then
kState = 0
endif
endif
endif
 
if (kState == 1) then
kPhaseK min kPhaseK + (kDT/kKillSec), 1
kGain max 1 - kPhaseK, 0
 
elseif (kState == 2) then
kPhaseA min kPhaseA + (kDT/kAtkSec), 1
kGain = 1 - pow(1 - kPhaseA, gkATTK_shapePow)
 
elseif (kState == 3) then
kPhaseD min kPhaseD + (kDT/kDecSec), 1
kGain = (1 - kPhaseD) + kPhaseD * gkATTK_susLvl
 
elseif (kState == 4) then
kGain = gkATTK_susLvl
 
elseif (kState == 5) then
kPhaseR min kPhaseR + (kDT/kRelSec), 1
kGain max gkATTK_susLvl * (1 - pow(kPhaseR, 1.5)), 0
 
else
kGain = 0
endif
 
aGain interp kGain
 
aLd dcblock2 aLd
aRd dcblock2 aRd
 
aWetL = aLd * aGain
aWetR = aRd * aGain
 
gaATTK_outL ntrpol aL, aWetL, gkATTK_mix
gaATTK_outR ntrpol aR, aWetR, gkATTK_mix
endin
instr 90
iSUBdiv init 8
iBARlen init 2
iPHRASE init 8
iREPEATS init 2
iSTUTspd init 4
iSTUTchnc init 0.5
iFLTdiv init 1
iBW init 1
iCFmin init 50
iCFmax init 10000
iIh init 0
iLAYERS init 1
iGAIN init 0.75
iWGUIDEmin init 50
iWGUIDEmax init 70
iSQMODmin init 6
iSQMODmax init 12
iFSHIFTmin init -1000
iFSHIFTmax init 1000
 
iBPM = i(gkSTUTbpm)
iBPS = iBPM/60
 
kTrig metro 4
kSw init 0
if kTrig == 1 then
kSw changed gkSTUTbpm, iREPEATS, iPHRASE, iSTUTspd, iSTUTchnc, iBARlen, iSUBdiv, iFLTdiv, iLAYERS
endif
if kSw == 1 then
reinit UPDATE
endif
 
UPDATE:
aL = gaSTUTdryL
aR = gaSTUTdryR
 
kmetr2 metro iBPS
a1, a2 STUTiteration aL, aR, iBPS, iSUBdiv, iBARlen, iPHRASE, iREPEATS, iSTUTspd, iSTUTchnc, 1, iLAYERS
 
ifreq = iBPS * iFLTdiv
kcf1h randomh iCFmin, iCFmax, ifreq
kcf1i lineto kcf1h, 1/ifreq
kcf1 ntrpol kcf1i, kcf1h, iIh
kcf1 = kcf1
aFltL resonz a1, kcf1, kcf1*iBW, 2
aMixL ntrpol a1, aFltL, gkSTUTfltMix
 
kcf2h randomh iCFmin, iCFmax, ifreq
kcf2i lineto kcf2h, 1/ifreq
kcf2 ntrpol kcf2i, kcf2h, iIh
kcf2 = kcf2
aFltR resonz a2, kcf2, kcf2*iBW, 2
aMixR ntrpol a2, aFltR, gkSTUTfltMix
 
kdice trandom kmetr2, 0, 1
if (kdice < gkSTUTchance) then
knum randomh iWGUIDEmin, iWGUIDEmax, iBPS
afrq interp cpsmidinn(int(knum))
kfb randomi 0.8, 0.99, iBPS/4
kcf randomi 800, 4000, iBPS
aMixL wguide1 aMixL*0.7, afrq, kcf, kfb
aMixR wguide1 aMixR*0.7, afrq, kcf, kfb
endif
 
kroll trandom kmetr2, 0, 1
if (kroll < gkSTUTchance) then
kratei randomi iSQMODmin, iSQMODmax, iBPS
krateh randomh iSQMODmin, iSQMODmax, iBPS
kx randomi 0, 1, iBPS
krate ntrpol kratei, krateh, kx
amod lfo 1, cpsoct(krate), 2
kcf limit cpsoct(krate)*4, 20, sr/3
amod clfilt amod, kcf, 0, 2
aMixL = aMixL * amod
aMixR = aMixR * amod
endif
 
kdice2 trandom kmetr2, 0, 1
if (kdice2 < 0.2 * gkSTUTchance) then
kfsqi randomi iFSHIFTmin, iFSHIFTmax, iBPS*2
kfsqh randomh iFSHIFTmin, iFSHIFTmax, iBPS*2
kx2 randomi 0, 1, iBPS*2
kfsq ntrpol kfsqi, kfsqh, kx2
aMixL STUTfreqshifter aMixL, kfsq, gisine
aMixR STUTfreqshifter aMixR, kfsq, gisine
endif
 
rireturn
gaSTUTwetL = aMixL * iGAIN
gaSTUTwetR = aMixR * iGAIN
endin
instr 300
aMix = (gaLeslieSendL + gaLeslieSendR) * 0.5
kfreq = gkLeslieSpeed
kAmpDepth = gkLeslieDepth
kAmpPhase = 0.5
kPanDepth = gkLeslieDepth
kDopDep = 0
ishape = gisine
 
aL, aR DopplerSpin aMix, kfreq, kAmpDepth, kAmpPhase, kPanDepth, kDopDep, ishape
 
gaLeslieOutL = aL
gaLeslieOutR = aR
endin
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
instr 211
kfreq = gkVIB_freq
kdepth = gkVIB_depth
a1 = gaVIB_sendL
a2 = gaVIB_sendR
 
aDelTim oscili kdepth, kfreq, gisine, 0
aDelTim = aDelTim + kdepth
iMaxDelay = 1
aDelTap1 vdelayxw a1, aDelTim, iMaxDelay, 16
aDelTap2 vdelayxw a2, aDelTim, iMaxDelay, 16
gaVIB_outL = aDelTap1
gaVIB_outR = aDelTap2
endin
instr 222
krate = gkFLA_rate
kdepth = gkFLA_depth
kdelay = gkFLA_offset
kfback = gkFLA_fback
klfoshape = gkFLA_lfoshape
kmix = gkFLA_mix
a1 = gaFLA_sendL
a2 = gaFLA_sendR
 
afla1 Flanger a1,krate,kdepth,kdelay,kfback,klfoshape
afla2 Flanger a2,krate,kdepth,kdelay,kfback,klfoshape
gaFLA_outL = afla1
gaFLA_outR = afla2
endin
instr 233
kporttime linseg 0,0.001,0.05
krate portk gkPHA_rate,kporttime
kdepth portk gkPHA_depth,kporttime
kfreq portk gkPHA_freq,kporttime
kfback portk gkPHA_fback,kporttime
ktrig changed gkPHA_shape,gkPHA_stages
if ktrig=1 then
reinit RESTART_PHASER
endif
RESTART_PHASER:
a1 = gaPHA_sendL
a2 = gaPHA_sendR
aPhs1,aPhs2 PhaserSt a1,a2,krate,kdepth*8,(kfreq*10)+4,i(gkPHA_stages),kfback*0.9,i(gkPHA_shape)
rireturn
aOut1 ntrpol a1,aPhs1,gkPHA_mix
aOut2 ntrpol a2,aPhs2,gkPHA_mix
gaPHA_outL = aOut1
gaPHA_outR = aOut2
endin
</CsInstruments>
<CsScore>
i99 0 z
i1 0 z
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
