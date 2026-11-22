<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; Granulation bench. csound --omacro:SCN=<n> --omacro:FFT=<idx> -o out.wav sm_bench_granulation.csd
; L = processed, R = dry. SCN 1 default, 2 density 0, 3 shift +20 delay .1, 4 fb 90 + fade 100,
; 5 bins/grain 1 + extremes, 6 max var, 7 delay .1 len .1 Bins 150 (near transparent after delay)
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_granulation.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  iFft = $FFT
  iN = 512 * 2^iFft
  p3 = 8
  SM_Set $SMP_GRANUL_RANDOMIZE_PHASES, 0
  if iScn == 2 then
    SM_Set $SMP_GRANUL_DENSITY, 0
  elseif iScn == 3 then
    SM_Set $SMP_GRANUL_BIN_SHIFT, 20
    SM_Set $SMP_GRANUL_GRAIN_DELAY, 0.1
    SM_Set $SMP_GRANUL_GRAIN_DELAY_VARIANCE, 0
  elseif iScn == 4 then
    SM_Set $SMP_GRANUL_FEEDBACK, 90
    SM_Set $SMP_GRANUL_FADE_AMOUNT, 100
  elseif iScn == 5 then
    SM_Set $SMP_GRANUL_BINS_PER_GRAIN, 1
    SM_Set $SMP_GRANUL_GRAIN_LENGTH, 0.1
    SM_Set $SMP_GRANUL_DENSITY, 50
    SM_Set $SMP_GRANUL_BIN_SHIFT, -200
    SM_Set $SMP_GRANUL_BIN_SHIFT_VARIANCE, 200
    SM_Set $SMP_GRANUL_GRAIN_DELAY, 5
    SM_Set $SMP_GRANUL_GRAIN_DELAY_VARIANCE, 2
  elseif iScn == 6 then
    SM_Set $SMP_GRANUL_GRAIN_LENGTH, 1
    SM_Set $SMP_GRANUL_GRAIN_LENGTH_VARIANCE, 1
    SM_Set $SMP_GRANUL_GRAIN_DELAY_VARIANCE, 2
    SM_Set $SMP_GRANUL_BINS_PER_GRAIN, 150
    SM_Set $SMP_GRANUL_RANDOMIZE_PHASES, 1
    SM_Set $SMP_GRANUL_FEEDBACK, 99
    SM_Set $SMP_GRANUL_GAIN, 40
  elseif iScn == 7 then
    SM_Set $SMP_GRANUL_GRAIN_DELAY, 0.1
    SM_Set $SMP_GRANUL_GRAIN_DELAY_VARIANCE, 0
    SM_Set $SMP_GRANUL_BINS_PER_GRAIN, 150
    SM_Set $SMP_GRANUL_GRAIN_LENGTH_VARIANCE, 0
  endif
  aIn = 0.3 * (oscili:a(1, 220) + oscili:a(1, 660) * 0.7 + oscili:a(1, 1770) * 0.5) * linseg:a(1, 3, 1, 0.01, 0.2, 5, 0.2)
  aIn += 0.05 * rnd31:a(1, 0)
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_Granulation kM, kF, $SM_GRANUL, kNew, iN
  kM, kF SM_Common kM, kF, $SM_GRANUL, kNew, iN
  fOut tab2pvs kM, kF, iN/4, iN, 1
  aOut pvsynth fOut
  aOut dcblock2 aOut
  outs limit(aOut, -0.99, 0.99), aIn
endin
</CsInstruments>
<CsScore>
i1 0 8
</CsScore>
</CsoundSynthesizer>
