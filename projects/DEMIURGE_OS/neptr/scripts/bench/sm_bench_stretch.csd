<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1

#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_stretch.orc"



opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
; Render: csound --omacro:SCN=n --omacro:FFT=2048 -o out.wav ; L processed, R dry
; SCN 1 default, 2 alpha .01 beta 1 (near identity), 3 alpha 5 beta 3, 4 LFO var, 5 random var quant 100, 6 link amp, 7 beta .5 alpha .01
instr 1
  iScn = $SCN
  iN = $FFT
  p3 = 5
  SM_Set $SMP_STRETCH_RANDOMIZE_PHASES, 0
  if iScn == 2 then
    SM_Set $SMP_STRETCH_ALPHA_STRETCH, 0.01
  elseif iScn == 3 then
    SM_Set $SMP_STRETCH_ALPHA_STRETCH, 5
    SM_Set $SMP_STRETCH_BETA_STRETCH, 3
  elseif iScn == 4 then
    SM_Set $SMP_STRETCH_VARIANCE_TYPE, 2
    SM_Set $SMP_STRETCH_BETA_STRETCH_VARIANCE, 1
    SM_Set $SMP_STRETCH_LFO_RATE, 2
  elseif iScn == 5 then
    SM_Set $SMP_STRETCH_VARIANCE_TYPE, 1
    SM_Set $SMP_STRETCH_VARIANCE_QUANTIZATION, 100
  elseif iScn == 6 then
    SM_Set $SMP_STRETCH_VARIANCE_TYPE, 3
    SM_Set $SMP_STRETCH_BETA_STRETCH_VARIANCE, 2
  elseif iScn == 7 then
    SM_Set $SMP_STRETCH_ALPHA_STRETCH, 0.01
    SM_Set $SMP_STRETCH_BETA_STRETCH, 0.5
  endif
  aIn = 0.15 * (oscili:a(1, 220) + oscili:a(1, 440) + oscili:a(1, 880) + oscili:a(1, 1760))
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_Stretch kM, kF, $SM_STRETCH, kNew, iN
  kM, kF SM_Common kM, kF, $SM_STRETCH, kNew, iN
  fOut tab2pvs kM, kF, iN/4, iN, 1
  aOut pvsynth fOut
  aOut dcblock2 aOut
  outs limit(aOut, -0.99, 0.99), aIn
endin
</CsInstruments>
<CsScore>
i1 0 5
</CsScore>
</CsoundSynthesizer>
