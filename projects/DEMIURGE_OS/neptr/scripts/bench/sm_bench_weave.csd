<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; SM_Weave bench. csound --omacro:SCN=<n> --omacro:FFT=<N> -o out.wav  (L processed, R dry)
; SCN 1 default-ish (short times), 2 extremes pulse+norm+fade0, 3 wet=1 silence long
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
#include "../../csound/spectral/sm_weave.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  iN = $FFT
  p3 = 8
  SM_Set $SMP_WEAVE_RANDOMIZE_PHASES, 0
  SM_Set $SMP_WEAVE_THRESHOLD, -40
  SM_Set $SMP_WEAVE_MIN_STABILITY_LENGTH, 0.3
  SM_Set $SMP_WEAVE_HOLD_LENGTH, 2
  SM_Set $SMP_WEAVE_SILENCE_LENGTH, 1
  SM_Set $SMP_WEAVE_SILENCE_LENGTH_VAR, 0.2
  SM_Set $SMP_WEAVE_HOLD_OUT_LENGTH, 1
  SM_Set $SMP_WEAVE_WET_DRY, 1
  if iScn == 2 then
    SM_Set $SMP_WEAVE_FADE_IN_OUT, 0
    SM_Set $SMP_WEAVE_PULSE_FREQUENCY, 5
    SM_Set $SMP_WEAVE_PULSE_GAIN, -60
    SM_Set $SMP_WEAVE_NORMALIZE, 1
    SM_Set $SMP_WEAVE_NORMALIZE_LEVEL, -6
    SM_Set $SMP_WEAVE_NUM_THREADS, 15
    SM_Set $SMP_WEAVE_HOLD_OUT_LENGTH, 0
  elseif iScn == 3 then
    SM_Set $SMP_WEAVE_SILENCE_LENGTH, 20
    SM_Set $SMP_WEAVE_NUM_THREADS, 1
    SM_Set $SMP_WEAVE_THREAD_LIFE, 3
  endif
  ; input: steady tone 0..3s, then silence, then different tone 5..8s
  aIn = 0.3 * oscili:a(1, 440) * linseg:a(1, 3, 1, 0.01, 0, 1.99, 0, 0.01, 1, 3, 1)
  if iScn == 2 then
    aIn = 0.3 * (oscili:a(1, 330) + oscili:a(1, 500)) * linseg:a(1, 3, 1, 0.01, 0, 1.99, 0, 0.01, 1, 3, 1)
  endif
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_Weave kM, kF, $SM_WEAVE, kNew, iN
  kM, kF SM_Common kM, kF, $SM_WEAVE, kNew, iN
  fO tab2pvs kM, kF, iN/4, iN, 1
  aO pvsynth fO
  outs aO, aIn
endin
</CsInstruments>
<CsScore>
i1 0 8
</CsScore>
</CsoundSynthesizer>
