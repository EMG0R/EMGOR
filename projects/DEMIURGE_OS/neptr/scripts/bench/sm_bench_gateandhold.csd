<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; GateAndHold bench. csound --omacro:SCN=<n> --omacro:FFT=<N> -o out.wav sm_bench_gateandhold.csd
; stereo out: L processed, R dry. Input: 330/990/2200 Hz tones, burst for first 1.5 s then silence.
; SCN 0 defaults; 1 hold 0.3 no silence/fades (gated hold); 2 type1 n=3; 3 type3 wait; 4 shift+pulse+delay-vs-freq;
;     5 normalize + decay 0; 6 zero off; 7 extremes (long fades, threshold -60, brightness 100)
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_gateandhold.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  iN = $FFT
  p3 = 6
  SM_Set $SMP_GATEHOLD_RANDOMIZE_PHASES, 0
  if iScn == 1 then
    SM_Set $SMP_GATEHOLD_FADE_IN_LENGTH, 0.05
    SM_Set $SMP_GATEHOLD_FADE_OUT_LENGTH, 0.05
    SM_Set $SMP_GATEHOLD_HOLD_LENGTH, 0.3
    SM_Set $SMP_GATEHOLD_HOLD_VARIANCE, 0
    SM_Set $SMP_GATEHOLD_SILENCE_LENGTH, 0
    SM_Set $SMP_GATEHOLD_SILENCE_LENGTH_VAR, 0
    SM_Set $SMP_GATEHOLD_DECAY_FACTOR, 100
  elseif iScn == 2 then
    SM_Set $SMP_GATEHOLD_GATE_TYPE, 1
    SM_Set $SMP_GATEHOLD_NUM_PARTIALS, 3
    SM_Set $SMP_GATEHOLD_FADE_IN_LENGTH, 0.1
    SM_Set $SMP_GATEHOLD_FADE_OUT_LENGTH, 0.1
    SM_Set $SMP_GATEHOLD_SILENCE_LENGTH, 0
    SM_Set $SMP_GATEHOLD_SILENCE_LENGTH_VAR, 0
  elseif iScn == 3 then
    SM_Set $SMP_GATEHOLD_GATE_TYPE, 4
    SM_Set $SMP_GATEHOLD_NUM_PARTIALS, 2
    SM_Set $SMP_GATEHOLD_FADE_IN_LENGTH, 0.1
    SM_Set $SMP_GATEHOLD_FADE_OUT_LENGTH, 0.1
  elseif iScn == 4 then
    SM_Set $SMP_GATEHOLD_BIN_SHIFT, 20
    SM_Set $SMP_GATEHOLD_BIN_SHIFT_VAR, 10
    SM_Set $SMP_GATEHOLD_DELAY_VS_FREQ, 40
    SM_Set $SMP_GATEHOLD_PULSING_RATE, 3
    SM_Set $SMP_GATEHOLD_PULSING_RATE_VAR, 2
    SM_Set $SMP_GATEHOLD_FADE_IN_LENGTH, 0.2
  elseif iScn == 5 then
    SM_Set $SMP_GATEHOLD_NORMALIZE_HELD_PARTIALS, 1
    SM_Set $SMP_GATEHOLD_NORMALIZE_LEVEL, -20
    SM_Set $SMP_GATEHOLD_DECAY_FACTOR, 0
    SM_Set $SMP_GATEHOLD_SILENCE_LENGTH, 0
    SM_Set $SMP_GATEHOLD_SILENCE_LENGTH_VAR, 0
  elseif iScn == 6 then
    SM_Set $SMP_GATEHOLD_ZERO_PARTIALS_UNDER_THRESHOLD, 0
    SM_Set $SMP_GATEHOLD_THRESHOLD, 0
  elseif iScn == 7 then
    SM_Set $SMP_GATEHOLD_THRESHOLD, -60
    SM_Set $SMP_GATEHOLD_THRESHOLD_BRIGHTNESS, 100
    SM_Set $SMP_GATEHOLD_FADE_IN_LENGTH, 10
    SM_Set $SMP_GATEHOLD_HOLD_LENGTH, 10
    SM_Set $SMP_GATEHOLD_FADE_OUT_LENGTH, 10
    SM_Set $SMP_GATEHOLD_SILENCE_LENGTH, 20
    SM_Set $SMP_GATEHOLD_DELAY_VS_FREQ, 200
    SM_Set $SMP_GATEHOLD_BIN_SHIFT, 100
    SM_Set $SMP_GATEHOLD_BIN_SHIFT_VAR, 100
    SM_Set $SMP_GATEHOLD_RANDOMIZE_PHASES, 1
    SM_Set $SMP_GATEHOLD_GAIN, 40
  endif
  aIn = 0.15 * (oscili:a(1, 330) + oscili:a(1, 990) + oscili:a(1, 2200)) * linseg:a(1, 1.5, 1, 0.01, 0, 10, 0)
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_GateAndHold kM, kF, $SM_GATEHOLD, kNew, iN
  kM, kF SM_Common kM, kF, $SM_GATEHOLD, kNew, iN
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
