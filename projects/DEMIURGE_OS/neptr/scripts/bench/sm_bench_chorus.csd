<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; Chorus bench. Render: csound --omacro:SCN=<n> -o out.wav sm_bench_chorus.csd  (stereo: L,R processed)
; SCN 1 defaults (440 Hz sine in, +-delay) 2 LFO var 10ms @2Hz fb 0 wet 100 3 extremes (delay 0, fb 100, rate 200, var 100)
; 4 random var 5 link amp 6 link inv 7 param sweep (click test) 8 CPU: 1 call, noise  9 dry control (wet 0)
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_chorus.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  p3 = 4
  if iScn == 2 then
    SM_Set $SMP_CHORUS_DELAY_VARIANCE_TYPE, 2
    SM_Set $SMP_CHORUS_DELAY_VARIANCE, 10
    SM_Set $SMP_CHORUS_LFO_RATE, 2
    SM_Set $SMP_CHORUS_FEEDBACK, 0
    SM_Set $SMP_CHORUS_WET_DRY_MIX, 100
  elseif iScn == 3 then
    SM_Set $SMP_CHORUS_DELAY, 0
    SM_Set $SMP_CHORUS_DELAY_VARIANCE_TYPE, 2
    SM_Set $SMP_CHORUS_DELAY_VARIANCE, 100
    SM_Set $SMP_CHORUS_LFO_RATE, 200
    SM_Set $SMP_CHORUS_FEEDBACK, 100
    SM_Set $SMP_CHORUS_WET_DRY_MIX, 100
    SM_Set $SMP_CHORUS_GAIN, 30
  elseif iScn == 4 then
    SM_Set $SMP_CHORUS_DELAY_VARIANCE_TYPE, 1
  elseif iScn == 5 then
    SM_Set $SMP_CHORUS_DELAY_VARIANCE_TYPE, 3
  elseif iScn == 6 then
    SM_Set $SMP_CHORUS_DELAY_VARIANCE_TYPE, 4
  elseif iScn == 9 then
    SM_Set $SMP_CHORUS_WET_DRY_MIX, 0
  endif
  if iScn == 8 then
    aIn = noise(0.3, 0)
  else
    aIn = oscili(0.5, 440) * (iScn >= 5 && iScn <= 6 ? (0.2 + 0.8 * (oscili:k(0.5, 1) + 0.5)) : 1)
  endif
  if iScn == 7 then
    ; sweep params every k-cycle (stress): delay 1..100, fb 0..100, mix, gain
    kS = oscili:k(0.5, 0.7) + 0.5
    tablew kS * 100, $SMP_CHORUS_DELAY, giSMP
    tablew kS * 100, $SMP_CHORUS_FEEDBACK, giSMP
    tablew 2, $SMP_CHORUS_DELAY_VARIANCE_TYPE, giSMP
    tablew kS * 50, $SMP_CHORUS_DELAY_VARIANCE, giSMP
    tablew kS * 100, $SMP_CHORUS_WET_DRY_MIX, giSMP
    tablew (kS - 0.5) * 20, $SMP_CHORUS_GAIN, giSMP
  endif
  aL, aR SM_Chorus aIn, aIn * 0.9, $SM_CHORUS
  outs aL, aR
endin
</CsInstruments>
<CsScore>
i1 0 4
</CsScore>
</CsoundSynthesizer>
