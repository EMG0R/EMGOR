<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; SM_Harmonizer bench. csound --omacro:SCN=<n> --omacro:FFT=<2048|8192> -o out.wav sm_bench_harmonizer.csd
; Output stereo: L processed, R dry. Input: 440 Hz sine.
; SCN 1 default(400c chord, 50%) 2 wet100 interval1 +1200 3 stacked 300x4 3 intervals 4 harmonics
; 5 feedback 90 + wet100 6 wet0 (bypass) 7 extreme: all 3 intervals, 10 transp, chord, wet100, fb 99
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_harmonizer.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  iN = $FFT
  p3 = 4
  if iScn == 2 then
    SM_Set $SMP_HARMONIZ_WET_DRY_MIX, 100
    SM_Set $SMP_HARMONIZ_INTERVAL_1, 1200
    SM_Set $SMP_HARMONIZ_TRANSPOSITION_TYPE, 0
  elseif iScn == 3 then
    SM_Set $SMP_HARMONIZ_USE_INTERVAL_2, 1
    SM_Set $SMP_HARMONIZ_USE_INTERVAL_3, 1
    SM_Set $SMP_HARMONIZ_INTERVAL_1, 300
    SM_Set $SMP_HARMONIZ_INTERVAL_2, 700
    SM_Set $SMP_HARMONIZ_INTERVAL_3, -1200
    SM_Set $SMP_HARMONIZ_TRANSPOSITION_TYPE, 0
    SM_Set $SMP_HARMONIZ_TRANSPOSITIONS_PER_INTERVAL, 4
  elseif iScn == 4 then
    SM_Set $SMP_HARMONIZ_TRANSPOSITION_TYPE, 2
    SM_Set $SMP_HARMONIZ_INTERVAL_1, 1200
    SM_Set $SMP_HARMONIZ_TRANSPOSITIONS_PER_INTERVAL, 3
    SM_Set $SMP_HARMONIZ_WET_DRY_MIX, 100
  elseif iScn == 5 then
    SM_Set $SMP_HARMONIZ_FEEDBACK, 90
    SM_Set $SMP_HARMONIZ_WET_DRY_MIX, 100
  elseif iScn == 6 then
    SM_Set $SMP_HARMONIZ_WET_DRY_MIX, 0
  elseif iScn == 7 then
    SM_Set $SMP_HARMONIZ_USE_INTERVAL_2, 1
    SM_Set $SMP_HARMONIZ_USE_INTERVAL_3, 1
    SM_Set $SMP_HARMONIZ_INTERVAL_1, 1200
    SM_Set $SMP_HARMONIZ_INTERVAL_2, -1200
    SM_Set $SMP_HARMONIZ_INTERVAL_3, 1200
    SM_Set $SMP_HARMONIZ_GAIN_3, 0
    SM_Set $SMP_HARMONIZ_TRANSPOSITIONS_PER_INTERVAL, 10
    SM_Set $SMP_HARMONIZ_FEEDBACK, 99
    SM_Set $SMP_HARMONIZ_WET_DRY_MIX, 100
  endif
  aIn = 0.5 * oscili:a(1, 440)
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_Harmonizer kM, kF, $SM_HARMONIZ, kNew, iN
  kM, kF SM_Common kM, kF, $SM_HARMONIZ, kNew, iN
  fOut tab2pvs kM, kF, iN/4, iN, 1
  aOut pvsynth fOut
  aOut dcblock2 aOut
  outs limit(aOut, -0.99, 0.99), aIn
endin
</CsInstruments>
<CsScore>
i1 0 4
</CsScore>
</CsoundSynthesizer>
