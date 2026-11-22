;==============================================================
; GLOBALS.ORC — Phase 4 Modular Guitar Effects Processor
; Deduplicated global declarations, function tables, macros, UDOs
;==============================================================

sr = 48000
ksmps = 128
nchnls = 2
nchnls_i = 2
0dbfs = 1
seed 0

;==============================================================
; MACROS
;==============================================================

; Effect bypass/launch/kill — standardized for signal chain
; All bypass checks use < 0.5 (active) / >= 0.5 (bypassed)
#define FX_BYPASS(kByp'iNum'gaSndL'gaSndR'gaOutL'gaOutR'uid)#
  kfx_$uid._byp = $kByp
  kfx_$uid._prv init -1
  if (kfx_$uid._prv == -1) then
    if (kfx_$uid._byp < 0.5) then
      event "i", $iNum, 0, -1
    else
      $gaOutL = $gaSndL
      $gaOutR = $gaSndR
    endif
  else
    if (kfx_$uid._prv >= 0.5 && kfx_$uid._byp < 0.5) then
      event "i", $iNum, 0, -1
    elseif (kfx_$uid._prv < 0.5 && kfx_$uid._byp >= 0.5) then
      turnoff2 $iNum, 0, 1
      $gaOutL = $gaSndL
      $gaOutR = $gaSndR
    endif
  endif
  kfx_$uid._prv = kfx_$uid._byp
  if (kfx_$uid._byp >= 0.5) then
    $gaOutL = $gaSndL
    $gaOutR = $gaSndR
  endif
#

; Toggle effect bypass and set last-toggle tracker
#define FX_TOGGLE(kByp'iToggle)#
  $kByp = 1 - $kByp
  if ($kByp < 0.5) then
    gkLastToggle = $iToggle
  endif
#

; Glitch press (button down): capture pre-press bypass, force effect ON,
; mark held, stamp press time, set lastToggle trackers for BPM + chance dispatch.
#define GLITCH_PRESS(kByp'kHeld'kPressT'kPrev'iToggle)#
  $kPrev = $kByp
  $kByp = 0
  $kHeld = 1
  $kPressT times
  gkLastToggle = $iToggle
  gkLastToggle29 = $iToggle
#

; Glitch release (button up): classify hold vs click.
; - hold  (>= giGLITCH_holdThresh): effect goes OFF
; - click (<  giGLITCH_holdThresh): bypass flips its pre-press state
#define GLITCH_RELEASE(kByp'kHeld'kPressT'kPrev'iToggle)#
  kNow_$iToggle. times
  kDt_$iToggle. = kNow_$iToggle. - $kPressT
  $kHeld = 0
  if (kDt_$iToggle. >= giGLITCH_holdThresh) then
    $kByp = 1
  else
    $kByp = 1 - $kPrev
    if ($kByp < 0.5) then
      gkLastToggle = $iToggle
      gkLastToggle29 = $iToggle
    endif
  endif
#

; Encoder rotation: relative increment/decrement with clamp
; Assumes kVal is in scope (MIDI CC value, >64 = CW, <64 = CCW)
#define ENC_ROT(kVar'kStep'kMin'kMax)#
  if (kVal > 64) then
    $kVar = min($kVar + $kStep, $kMax)
  elseif (kVal < 64) then
    $kVar = max($kVar - $kStep, $kMin)
  endif
#

; OSC feedback: send float on change
#define OSC_F(kVar'Saddr'uid)#
  kchg_$uid. changed $kVar
  if (kchg_$uid. == 1) then
    OSC3x_f "$Saddr.", $kVar
  endif
#

; OSC feedback: send integer on change
#define OSC_I(kVar'Saddr'uid)#
  kchg_$uid. changed $kVar
  if (kchg_$uid. == 1) then
    OSC3x_i "$Saddr.", $kVar
  endif
#

; MOOD bitcrusher — used by instrs 402 and 405
#define MOOD_BITCRUSH(aInL'aInR'aOutL'aOutR)#
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
    klevels = pow(2, kbits - 1)
    a_quantL = round($aInL * klevels) / klevels
    a_quantR = round($aInR * klevels) / klevels
    $aOutL SrReducer a_quantL, kred_sr
    $aOutR SrReducer a_quantR, kred_sr
  else
    $aOutL = $aInL
    $aOutR = $aInR
  endif
#

;==============================================================
; FUNCTION TABLES
;==============================================================

; General tables
giHanning ftgen 0, 0, 1024, 20, 2
gisine ftgen 0, 0, 131072, 10, 1

; Granular / pitch-shift window functions
giwfn1 ftgen 0, 0, 131072, 9, 0.5, 1, 0
giwfn2 ftgen 0, 0, 131072, 7, 0, 3072, 1, 128000, 0
giwfn3 ftgen 0, 0, 131072, 16, 0, 3072, 0, 1, 128000, -2, 0
giwfn4 ftgen 0, 0, 131072, 7, 0, 3536, 1, 124000, 1, 3536, 0
giwfn5 ftgen 0, 0, 131072, 7, 0, 128000, 1, 3072, 0
giwfn6 ftgen 0, 0, 131072, 5, 0.001, 128000, 1, 3072, 0.001
giwfn7 ftgen 0, 0, 131072, 20, 2, 1
giwindows ftgen 0, 0, 8, -2, giwfn7, giwfn1, giwfn2, giwfn3, giwfn4

; Granular buffers (standalone granular effect)
giBufL ftgen 0, 0, 1048576, -2, 0
giBufR ftgen 0, 0, 1048576, -2, 0

; MOOD dedicated buffers (separate from standalone granular)
giMOOD_BufL ftgen 0, 0, 1048576, -2, 0
giMOOD_BufR ftgen 0, 0, 1048576, -2, 0

; GPS tables and buffers
giGPSbuffer ftgen 0, 0, 1048576, 2, 0
giGPSbufferR ftgen 0, 0, 1048576, 2, 0
giGPSwfn1 ftgen 0, 0, 131072, 9, 0.5, 1, 0
giGPSwfn2 ftgen 0, 0, 131072, 7, 0, 3072, 1, 128000, 0
giGPSwfn3 ftgen 0, 0, 131072, 5, 0.001, 3072, 1, 128000, 0.001
giGPSwfn4 ftgen 0, 0, 131072, 7, 0, 1536, 1, 128000, 1, 1536, 0
giGPSwfn5 ftgen 0, 0, 131072, 7, 0, 128000, 1, 3072, 0
giGPSwfn6 ftgen 0, 0, 131072, 5, 0.001, 128000, 1, 3072, 0.001
giGPSnnsamp = ftlen(giGPSbuffer) -1

; Spectral delay tables
giFFT = 256
giHop = ksmps
giWin = giFFT
giFormat = 1
giSpecDelAmpTab ftgen 300, 0, giFFT, -21, 1, 0.3
giSpecDelFrqTab ftgen 301, 0, giFFT, -21, 1, 0.3

; Phaser shape tables
giLog ftgen 0, 0, 4096, 19, 0.5, 1, 0, 0
giExp ftgen 0, 0, 4096, 19, 0.5, 1, 180, 1

; MOOD tables
giSemi ftgen 0, 0, 10, -2, 24, 19, 12, 7, 0, -5, -12, -17, -24, -29
giHalfSine ftgen 0, 0, 131072, 9, 0.5, 1, 0
giPitchBufL ftgen 0, 0, 1048576, 2, 0
giPitchBufR ftgen 0, 0, 1048576, 2, 0

; Attack-shaper constant
giPreMaxMs init 200

;==============================================================
; GLOBAL CONTROL VARIABLES
;==============================================================

gkPortTime init 0.03
gkPowerMax init 50
gkLastToggle init 0
gkLastToggle29 init 0
gkVOLpedal init 1
gkVOLpedalBypass init 1

; Pitch shift controls
gkSemi init 0
gkDryPctPS init 100
gkBypassPS init 1
gkKnobRaw init 1

; Distortion controls
gkBypassDist init 1
gkDistAmtAcc init 0.0
gkDistAmt init 1.0

; Bitcrusher controls
gkBitsBC init 16
gkBypassBC init 1
gkBitsAmtAcc init 0.0

; Delay controls
gkDelayMS init 0
gkFeedback init 0.5
gkDelMix init 0.5
gkBypassDelay init 1
gkSemiDel init -0.01
gkDryPctPSdel init 100
gkBypassPSdel init 0
gkKnobRawDel init 100

; Ring modulator controls
gkARModDepth init 1
gkARModFreq init 0.1
gkBypassRM init 1

; Reverb controls
gkverbBypass init 1
gkverbMix init 1
gkReverbFB init 0.92

; Granular controls
gkGranBypass init 1
gkGranMix init 0.5

; GPS controls
gkGPSamp init 1
gkGPSInGain init 1
gkGPSmix init 1
gkGPSbalance init 1
gkGPSmonostereo init 3
gkGPSfback init 0
gkGPSclip init 1
gkGPSClipLev init 0.5
gkGPSCutoff init 4000
gkGPSLPF_On init 0
gkGPSwsize init 2000
gkGPSrnd init 0
gkGPSsemis init 12
gkGPSolap init 200
gkGPSwfn init 1
gkGPSdly init 0.001
gkGPSbeta init 1
gkGPSfreeze init 0
gkGPSManPtrPort init 0.5
gkGPSManPtr init 0
gkBypassGPS init 1

; Time stretch controls
gkBypassTS init 1
gkTSAmtAcc init 0.0

; Chorus controls
gkBypassChorus init 1
gkChorusMixAcc init 1
gkChorusMix init 0.9
gkChorusRate init 0.7
gkChorusDereg init 0.5
gkChorusDepth init 0.4
gkChorusOffset init 0.02
gkChorusWidth init 1
gkChorusType init 1
gkChorusDepthAcc init 1

; Spectral delay controls
gkSpecDelBypass init 1
gkSpecDelMix init 0.5
gkSpecDelFB init 0.8
gkSpecDelMaxDel init 0.3
gkSpecDelPrev init 1

; Glitch shared controls
gkGLITCH_bpm init 95
gkGLITCH_chance init 0.25

; Glitch hold/click state (shared mechanics for all 4 glitch toggles)
; _held    = 1 while button physically held down (forces 100% chance)
; _pressT  = perf-time when button went down (for hold-vs-click classification)
; _prevByp = bypass state captured at moment of press (restored on short click)
giGLITCH_holdThresh init 0.30   ; seconds — shorter than this = click

gkBBcut_held   init 0
gkBBcut_pressT init 0
gkBBcut_prevByp init 1

gkWGuide_held   init 0
gkWGuide_pressT init 0
gkWGuide_prevByp init 1

gkSqMod_held   init 0
gkSqMod_pressT init 0
gkSqMod_prevByp init 1

gkFShift_held   init 0
gkFShift_pressT init 0
gkFShift_prevByp init 1

; BBcut (beat-cutter) controls
gkBBcut_bypass init 1
gkBBcut_drywet init 1.0
gkBBcut_fltMix init 0.6

; WGuide (waveguide glitch) controls
gkWGuide_bypass init 1
gkWGuide_drywet init 1.0

; SqMod (square modulation) controls
gkSqMod_bypass init 1
gkSqMod_drywet init 1.0

; FreqShift (frequency shifter) controls
gkFShift_bypass init 1
gkFShift_drywet init 1.0

; AutoMoog controls
gkAUTOMOOG_bypass init 1
gkAUTOMOOG_sensAcc init 0.8
gkAUTOMOOG_sens init 0.8
gkAUTOMOOG_att init 0.005
gkAUTOMOOG_rel init 0.15
gkAUTOMOOG_freq init 200
gkAUTOMOOG_type init 2
gkAUTOMOOG_res init 0.85
gkAUTOMOOG_dist init 0
gkAUTOMOOG_level init 1

; WAH controls
gkWAH_bypass init 1
gkWAH_freq init 0.5
gkWAH_res init 0.7

; Vibrato controls
gkVIB_bypass init 1
gkVIB_depthAcc init 0.0667
gkVIB_depth init 0.0019
gkVIB_freq init 1
gkVIB_fback init 0

; Flanger controls
gkFLA_bypass init 1
gkFLA_depthAcc init 0.5
gkFLA_depth init 0.005
gkFLA_rate init 2
gkFLA_offset init 0.0001
gkFLA_fback init 0.2
gkFLA_lfoshape init 4
gkFLA_mix init 1

; Phaser controls
gkPHA_bypass init 1
gkPHA_depthAcc init 0.5
gkPHA_depth init 0.5
gkPHA_rate init 2
gkPHA_freq init 0.1
gkPHA_fback init 0.2
gkPHA_stages init 8
gkPHA_mix init 1
gkPHA_shape init 8

; Leslie controls
gkLeslieBypass init 1
gkLeslieDepthAcc init 0
gkLeslieDepth init 1
gkLeslieSpeedAcc init 0
gkLeslieSpeed init 7.5

; MOOD controls
gkMOOD_Bypass init 1
gkMOOD_MixAcc init 0.5
gkMOOD_Mix init 0.5
gkMOOD_Clock init 0
gkMOOD_Dens init 200
gkMOOD_Dly init 0.7

; Formant controls
gkFormantBypass init 1
gkFormantShift init 0
gkFormantMix init 1

; Attack-shaper controls
gkATTK_bypass init 1
gkATTK_mix init 1.0
gkATTK_atkMs init 500
gkATTK_sens init 0.1
gkATTK_holdMs init 500
gkATTK_decMs init 20
gkATTK_susLvl init 1.0
gkATTK_relMs init 100
gkATTK_killMs init 3
gkATTK_shapePow init 2.5
gkATTK_basePreSec init 0.005
gkATTK_maxPreSec init 0.030
gkATTK_susHoldSec init 0.1
gkATTK_minSusSec init 0.2
gkATTK_lockSec init 0.1
gkATTK_noiseFloor init 0.05

; Dropout controls
gkDropoutBypass init 1
gkDropoutAmount init 50
gkDropoutDepth init 0.07
gkDropoutSmoothing init 0

;==============================================================
; GLOBAL AUDIO BUSES
;==============================================================

; Pitch shift
gaPSSendL init 0
gaPSSendR init 0
gaPSOutL init 0
gaPSOutR init 0

; Distortion
gaDistSendL init 0
gaDistSendR init 0
gaDistOutL init 0
gaDistOutR init 0

; Time stretch
gaTSSendL init 0
gaTSSendR init 0
gaTSOutL init 0
gaTSOutR init 0

; Delay
gaDelSendL init 0
gaDelSendR init 0
gaDelOutL init 0
gaDelOutR init 0
gaDelWetL init 0
gaDelWetR init 0

; Ring modulator
gaRMSendL init 0
gaRMSendR init 0
gaRMOutL init 0
gaRMOutR init 0

; Reverb
gaVerbSendL init 0
gaVerbSendR init 0
gaVerbWetL init 0
gaVerbWetR init 0

; Granular
gaGranSendL init 0
gaGranSendR init 0
gaGranWetL init 0
gaGranWetR init 0

; GPS
gaGPSSendL init 0
gaGPSSendR init 0
gaGPSWetL init 0
gaGPSWetR init 0
gaGPSFBackSig init 0
gaGPSFBackSigR init 0
gaphsW init 0

; Chorus
gaChorusSendL init 0
gaChorusSendR init 0
gaChorusWetL init 0
gaChorusWetR init 0

; Spectral delay
gaSpecDelSendL init 0
gaSpecDelSendR init 0
gaSpecDelWetL init 0
gaSpecDelWetR init 0

; AutoMoog
gaAUTOMOOG_sendL init 0
gaAUTOMOOG_sendR init 0
gaAUTOMOOG_outL init 0
gaAUTOMOOG_outR init 0

; Vibrato
gaVIB_sendL init 0
gaVIB_sendR init 0
gaVIB_outL init 0
gaVIB_outR init 0

; Flanger
gaFLA_sendL init 0
gaFLA_sendR init 0
gaFLA_outL init 0
gaFLA_outR init 0

; Phaser
gaPHA_sendL init 0
gaPHA_sendR init 0
gaPHA_outL init 0
gaPHA_outR init 0

; Leslie
gaLeslieSendL init 0
gaLeslieSendR init 0
gaLeslieOutL init 0
gaLeslieOutR init 0

; MOOD
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

; Formant
gaFormantSendL init 0
gaFormantSendR init 0
gaFormantOutL init 0
gaFormantOutR init 0

; Attack-shaper
gaATTK_sendL init 0
gaATTK_sendR init 0
gaATTK_outL init 0
gaATTK_outR init 0

; Dropout
gaDropSendL init 0
gaDropSendR init 0
gaDropOutL init 0
gaDropOutR init 0

; WAH
gaWAH_sendL init 0
gaWAH_sendR init 0
gaWAH_outL init 0
gaWAH_outR init 0

; BBcut
gaBBcut_sendL init 0
gaBBcut_sendR init 0
gaBBcut_wetL init 0
gaBBcut_wetR init 0

; WGuide
gaWGuide_sendL init 0
gaWGuide_sendR init 0
gaWGuide_wetL init 0
gaWGuide_wetR init 0

; SqMod
gaSqMod_sendL init 0
gaSqMod_sendR init 0
gaSqMod_wetL init 0
gaSqMod_wetR init 0

; FreqShift
gaFShift_sendL init 0
gaFShift_sendR init 0
gaFShift_wetL init 0
gaFShift_wetR init 0

;==============================================================
; STEP SIZE VARIABLES
;==============================================================

gkStepReverbFB init 0.01
gkStepGranMix init 0.01
gkStepGPSdly init 0.1
gkStepGPSfback init 0.1
gkStepGPSMix init 0.01
gkStepTSAmt init 0.01
gkStepChorusMix init 0.1
gkStepChorusRate init 0.05
gkStepChorusDepth init 0.05
gkStepFeedback init 0.005
gkStepSpecDelMaxDel init 0.002
gkStepGLITCH_chance init 0.01
gkStepSemiDel init 0.1
gkStepSpecDelFB init 0.01
gkStepGLITCH_bpm init 2
gkStepBBcut_drywet init 0.01
gkStepWGuide_drywet init 0.01
gkStepSqMod_drywet init 0.01
gkStepFShift_drywet init 0.01
gkStepVIB_fback init 0.01
gkStepDelMix init 0.01
gkStepSpecMix init 0.01
gkPSdryPctStep init 0.5
gkKnobRawStep init 0.01
gkBitsStep init 1
gkStepverbMix init 0.01
gkStepDistAmt init 0.01
gkStepBitsAmt init 0.05
gkStepAUTOMOOG_sens init 0.01
gkStepVIB_depth init 0.0011
gkStepFLA_depth init 0.01
gkStepFLA_rate init 0.1
gkStepFLA_fback init 0.01
gkStepPHA_depth init 0.01
gkStepPHA_rate init 0.1
gkStepPHA_fback init 0.01
gkStepLeslieDepth init 0.01
gkStepLeslieSpeed init 0.01
gkStepMOOD_Mix init 0.01
gkStepMOOD_Dens init 100
gkStepMOOD_Dly init 0.1
gkStepATTK_sens init 0.1
gkStepATTK_hold init 50
gkStepDropoutSmoothing init 0.01
gkStepDropoutDepth init 0.01

;==============================================================
; MENU SYSTEM VARIABLES
;==============================================================

gkCurrentMenu init 0
gkMenuChanged init 0

;==============================================================
; UDOs (User Defined Opcodes)
;==============================================================

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

opcode SwitchPort, k, kii
  kin, iupport, idnport xin
  kold init 0
  kporttime = (kin < kold ? idnport : iupport)
  kout portk kin, kporttime
  kold = kout
  xout kout
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

opcode EnvelopeFollower, a, akkkkkkk
  ain, ksens, katt, krel, kfreq, ktype, kres, kdist xin
  setksmps 4
  aFollow follow2 ain, katt, krel
  kFollow downsamp aFollow
  kFollow expcurve kFollow/0dbfs, 0.3
  kFrq = kfreq + (kFollow * ksens * 10000)
  kFrq limit kFrq, 20, sr/2
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

opcode LoFi, a, akk
  ain, kbits, kfold xin
  kval = pow(2, kbits)
  ain = clip(ain, -1, 1)
  aout = int(ain * kval) / kval
  aout fold aout, kfold
  xout aout
endop

opcode pitchshifter4, aa, aakikkiip
  ainL, ainR, kRatio, iNIter, kDelay, kSmooth, iMaxDel, iWfn, iCnt xin
  setksmps 1
  kPt linseg 0, 0.001, 1
  if kSmooth>0 then
    kRatio portk kRatio, kPt*kSmooth
    kDelay portk kDelay, kPt*kSmooth
  endif
  aDel interp kDelay
  aRate = (kRatio-1)/kDelay
  a1 phasor -aRate
  a2 phasor -aRate, 0.5
  ag1 tablei a1, iWfn, 1, 0, 1
  ag2 tablei a2, iWfn, 1, 0, 1
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
    aNL, aNR pitchshifter4 ainL, ainR, kRatio, iNIter, kDelay, kSmooth, iMaxDel, iWfn, iCnt+1
    aL = aL + aNL
    aR = aR + aNR
  endif
  xout aL, aR
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
  kpan rspline 0, 1, krate, 2*krate*kdereg
  kpan limit kpan, 0, 1
  apan interp kpan
  aChoL = (aCho1*apan)+(aCho2*(1-apan))
  aChoR = (aCho2*apan)+(aCho1*(1-apan))
  aChoL ntrpol aChoL, aCho1, kwidth
  aChoR ntrpol aChoR, aCho2, kwidth
  aoutL ntrpol ainL*0.6, aChoL*0.6, kmix
  aoutR ntrpol ainR*0.6, aChoR*0.6, kmix
  xout aoutL, aoutR
endop

opcode Flanger, a, akkkkk
  ain, krate, kdepth, kdelay, kfback, klfoshape xin
  iparabola ftgentmp 0, 0, 131072, 19, 0.5, 1, 180, 1
  isine ftgentmp 0, 0, 131072, 19, 1, 0.5, 0, 0.5
  itriangle ftgentmp 0, 0, 131072, 7, 0, 131072/2, 1, 131072/2, 0
  adlt interp kdelay
  if klfoshape==1 then
    amod oscili kdepth, krate, iparabola
  elseif klfoshape==2 then
    amod oscili kdepth, krate, isine
  elseif klfoshape==3 then
    amod oscili kdepth, krate, itriangle
  elseif klfoshape==4 then
    amod randomi 0, kdepth, krate, 1
  else
    amod randomh 0, kdepth, krate, 1
  endif
  adlt sum adlt, amod
  adelsig flanger ain, adlt, kfback, 1.2
  adelsig dcblock adelsig
  aout sum ain*0.5, adelsig*0.5
  xout aout
endop

opcode PhaserSt, aa, aakkkkki
  ainL, ainR, krate, kdepth, kfreq, kstages, kfback, ishape xin
  if ishape=1 then
    klfo lfo kdepth*0.5, krate, 1
  elseif ishape=2 then
    klfo lfo kdepth*0.5, krate, 0
  elseif ishape=3 then
    klfo lfo kdepth*0.5, krate, 2
  elseif ishape=4 then
    klfo lfo kdepth, krate, 4
  elseif ishape=5 then
    klfo lfo kdepth, krate, 5
  elseif ishape=6 then
    klfo oscil kdepth, krate, giExp
  elseif ishape=7 then
    klfo oscil kdepth, krate, giLog
  elseif ishape=8 then
    klfo randomi -kdepth*0.5, kdepth*0.5, krate*8
    klfo portk klfo, 1/(krate*8)
  elseif ishape=9 then
    klfo randomh -kdepth*0.5, kdepth*0.5, krate
  endif
  aoutL phaser1 ainL, cpsoct((klfo+(kdepth*0.5)+kfreq)), kstages, kfback
  aoutR phaser1 ainR, cpsoct((klfo+(kdepth*0.5)+kfreq)), kstages, kfback
  xout aoutL, aoutR
endop

;==============================================================
; OSC TRIPLE-SEND UDOs
;==============================================================

opcode OSC3x_f, 0, Sk
  Saddr, kvalue xin
  ksend init 0
  ksend = ksend + 1
  OSCsend ksend, "127.0.0.1", 8000, Saddr, "f", kvalue
  ksend = ksend + 1
  OSCsend ksend, "127.0.0.1", 8000, Saddr, "f", kvalue
  ksend = ksend + 1
  OSCsend ksend, "127.0.0.1", 8000, Saddr, "f", kvalue
endop

opcode OSC3x_i, 0, Sk
  Saddr, kvalue xin
  ksend init 0
  ksend = ksend + 1
  OSCsend ksend, "127.0.0.1", 8000, Saddr, "i", kvalue
  ksend = ksend + 1
  OSCsend ksend, "127.0.0.1", 8000, Saddr, "i", kvalue
  ksend = ksend + 1
  OSCsend ksend, "127.0.0.1", 8000, Saddr, "i", kvalue
endop
<bsbPanel>
 <label>Widgets</label>
 <objectName/>
 <x>0</x>
 <y>0</y>
 <width>0</width>
 <height>0</height>
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
