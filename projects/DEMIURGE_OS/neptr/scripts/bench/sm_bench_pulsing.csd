<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; Pulsing bench. csound --omacro:SCN=<n> --omacro:FFT=<idx> -o out.wav sm_bench_pulsing.csd
; SCN 0 default, 1 fast short (on/off .05), 2 extreme long+mults 5, 3 wrap mode, 4 zero durations, 5 gain/bright extremes
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_pulsing.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  iN = 512 * (2 ^ $FFT)
  p3 = 6
  if iScn == 1 then
    SM_Set $SMP_PULSING_ON_DURATION, 0.05
    SM_Set $SMP_PULSING_OFF_DURATION, 0.05
    SM_Set $SMP_PULSING_MIN_ON_DURATION, 0.02
    SM_Set $SMP_PULSING_MAX_ON_DURATION, 0.1
    SM_Set $SMP_PULSING_MIN_OFF_DURATION, 0.02
    SM_Set $SMP_PULSING_MAX_OFF_DURATION, 0.1
  elseif iScn == 2 then
    SM_Set $SMP_PULSING_ON_DURATION, 5
    SM_Set $SMP_PULSING_OFF_DURATION, 5
    SM_Set $SMP_PULSING_ON_MULTIPLIER_FREQ, 5
    SM_Set $SMP_PULSING_OFF_MULTIPLIER_FREQ, 5
    SM_Set $SMP_PULSING_ON_MULTIPLIER_TIME, 5
    SM_Set $SMP_PULSING_OFF_MULTIPLIER_TIME, 5
  elseif iScn == 3 then
    SM_Set $SMP_PULSING_MODULO_TYPE, 2
    SM_Set $SMP_PULSING_ON_MULTIPLIER_TIME, 3
    SM_Set $SMP_PULSING_OFF_MULTIPLIER_TIME, 0.3
  elseif iScn == 4 then
    SM_Set $SMP_PULSING_ON_DURATION, 0
    SM_Set $SMP_PULSING_OFF_DURATION, 0
    SM_Set $SMP_PULSING_MIN_ON_DURATION, 0
    SM_Set $SMP_PULSING_MAX_ON_DURATION, 0
    SM_Set $SMP_PULSING_MIN_OFF_DURATION, 0
    SM_Set $SMP_PULSING_MAX_OFF_DURATION, 0
  elseif iScn == 5 then
    SM_Set $SMP_PULSING_GAIN, 40
    SM_Set $SMP_PULSING_BRIGHTNESS, 100
  endif
  aIn = 0.1 * (oscili:a(1, 220) + oscili:a(1, 440) + oscili:a(1, 880) + oscili:a(1, 1760) + oscili:a(1, 3520) + oscili:a(1, 7040)) + 0.02 * rnd31:a(1, 0)
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_Pulsing kM, kF, $SM_PULSING, kNew, iN
  kM, kF SM_Common kM, kF, $SM_PULSING, kNew, iN
  fOut tab2pvs kM, kF, iN/4, iN, 1
  aOut pvsynth fOut
  aOut dcblock2 aOut
  outs limit(aOut, -0.99, 0.99), aIn
endin
</CsInstruments>
<CsScore>
i1 0 6
</CsScore>
</CsoundSynthesizer>
