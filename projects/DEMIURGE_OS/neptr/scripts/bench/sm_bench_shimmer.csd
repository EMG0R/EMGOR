<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; SM_Shimmer bench. csound --omacro:SCN=<n> --omacro:FFT=<N> -o out.wav sm_bench_shimmer.csd
; Output stereo: L processed, R dry. SCN 0 default, 1 neutral(all off), 2 jitter max, 3 pulse sq, 4 pulse sin,
; 5 delay LFO, 6 delay ascent+random, 7 feedback 90.
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#ifndef FFT
#define FFT #2048#
#endif
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_shimmer.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  iN = $FFT
  p3 = 6
  if iScn >= 1 then
    SM_Set $SMP_SHIMMER_JITTER_AMOUNT, 0
    SM_Set $SMP_SHIMMER_AMPLITUDE_PULSE_TYPE, 0
    SM_Set $SMP_SHIMMER_DELAY_VARIANCE_TYPE, 0
    SM_Set $SMP_SHIMMER_BRIGHTNESS, 0
  endif
  if iScn == 2 then
    SM_Set $SMP_SHIMMER_JITTER_AMOUNT, 100
    SM_Set $SMP_SHIMMER_LOW_FREQUENCY_JITTER, 20
    SM_Set $SMP_SHIMMER_HIGH_FREQUENCY_JITTER, 30
    SM_Set $SMP_SHIMMER_JITTER_HOLD_WINDOWS, 3
    SM_Set $SMP_SHIMMER_JITTER_HOLD_VAR, 50
  elseif iScn == 3 then
    SM_Set $SMP_SHIMMER_AMPLITUDE_PULSE_TYPE, 1
    SM_Set $SMP_SHIMMER_PULSE_LEVEL_MIN, -60
    SM_Set $SMP_SHIMMER_PULSE_WIDTH, 20
  elseif iScn == 4 then
    SM_Set $SMP_SHIMMER_AMPLITUDE_PULSE_TYPE, 2
    SM_Set $SMP_SHIMMER_PULSE_RATE, 5
  elseif iScn == 5 then
    SM_Set $SMP_SHIMMER_DELAY_VARIANCE_TYPE, 3
    SM_Set $SMP_SHIMMER_LFO_RATE_DELAY_VAR, 0.5
    SM_Set $SMP_SHIMMER_DELAY_VARIANCE, 250
  elseif iScn == 6 then
    SM_Set $SMP_SHIMMER_DELAY_VARIANCE_TYPE, 2
    SM_Set $SMP_SHIMMER_DELAY_VARIANCE, 40
  elseif iScn == 7 then
    SM_Set $SMP_SHIMMER_FEEDBACK, 90
    SM_Set $SMP_SHIMMER_DELAY_VARIANCE_TYPE, 5
    SM_Set $SMP_SHIMMER_DELAY_VARIANCE, 250
    SM_Set $SMP_SHIMMER_JITTER_AMOUNT, 100
    SM_Set $SMP_SHIMMER_HIGH_FREQUENCY_JITTER, 30
  endif
  SM_Set $SMP_SHIMMER_GAIN, 0
  aIn = 0.2 * (oscili:a(1, 220) + oscili:a(1, 660) + oscili:a(1, 1980) + oscili:a(1, 4400)) + 0.05*rnd31:a(1,0)
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_Shimmer kM, kF, $SM_SHIMMER, kNew, iN
  kM, kF SM_Common kM, kF, $SM_SHIMMER, kNew, iN
  fO tab2pvs kM, kF, iN/4, iN, 1
  aO pvsynth fO
  outs aO, aIn
endin
</CsInstruments>
<CsScore>
i1 0 6
e
</CsScore>
</CsoundSynthesizer>
