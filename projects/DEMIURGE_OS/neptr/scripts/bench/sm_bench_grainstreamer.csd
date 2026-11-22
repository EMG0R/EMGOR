<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; Grain Streamer bench. csound --omacro:SCN=<n> -o out.wav  (L = processed, R = dry). 4 s, input = 220 Hz sine.
; SCN: 1 defaults, 2 prob 100 reps 1 len 50 (pure repeat), 3 prob 0 (passthrough), 4 extremes (len 1/200, reps 100, var 100),
;      5 zero non-frozen + prob 30, 6 fade 0 / prob 100, 7 param sweeps mid-run, 8 two instances (CPU, x4 chained via 99)
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_grainstreamer.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  p3 = 4
  if iScn == 2 then
    SM_Set $SMP_GRAINSTR_FREEZE_PROBABILITY, 100
    SM_Set $SMP_GRAINSTR_GRAIN_REPETITIONS, 1
    SM_Set $SMP_GRAINSTR_REPETITIONS_VARIATION, 0
    SM_Set $SMP_GRAINSTR_GRAIN_LENGTH_AT_START, 50
    SM_Set $SMP_GRAINSTR_GRAIN_LENGTH_START_VAR, 0
    SM_Set $SMP_GRAINSTR_GRAIN_LENGTH_AT_END, 50
    SM_Set $SMP_GRAINSTR_GRAIN_LENGTH_END_VAR, 0
  elseif iScn == 3 then
    SM_Set $SMP_GRAINSTR_FREEZE_PROBABILITY, 0
  elseif iScn == 4 then
    SM_Set $SMP_GRAINSTR_FREEZE_PROBABILITY, 100
    SM_Set $SMP_GRAINSTR_GRAIN_REPETITIONS, 100
    SM_Set $SMP_GRAINSTR_REPETITIONS_VARIATION, 100
    SM_Set $SMP_GRAINSTR_GRAIN_LENGTH_AT_START, 1
    SM_Set $SMP_GRAINSTR_GRAIN_LENGTH_START_VAR, 200
    SM_Set $SMP_GRAINSTR_GRAIN_LENGTH_AT_END, 200
    SM_Set $SMP_GRAINSTR_GRAIN_LENGTH_END_VAR, 200
    SM_Set $SMP_GRAINSTR_GAIN, 20
  elseif iScn == 5 then
    SM_Set $SMP_GRAINSTR_FREEZE_PROBABILITY, 30
    SM_Set $SMP_GRAINSTR_ZERO_NON_FROZEN_GRAINS, 1
  elseif iScn == 6 then
    SM_Set $SMP_GRAINSTR_FREEZE_PROBABILITY, 100
    SM_Set $SMP_GRAINSTR_FADE_AMOUNT, 0
  endif
  aIn = 0.3 * poscil(1, 220)
  if iScn == 7 then
    kT = timeinsts()
    tablew 100 * (0.5 + 0.5 * sin(kT * 9)), $SMP_GRAINSTR_FREEZE_PROBABILITY, giSMP
    tablew 1 + 99 * (0.5 + 0.5 * sin(kT * 7)), $SMP_GRAINSTR_GRAIN_REPETITIONS, giSMP
    tablew 1 + 199 * (0.5 + 0.5 * sin(kT * 5)), $SMP_GRAINSTR_GRAIN_LENGTH_AT_START, giSMP
    tablew 20 * sin(kT * 3), $SMP_GRAINSTR_GAIN, giSMP
    tablew (sin(kT * 11) > 0 ? 1 : 0), $SMP_GRAINSTR_ZERO_NON_FROZEN_GRAINS, giSMP
  endif
  aL, aR SM_GrainStreamer aIn, aIn, $SM_GRAINSTR
  if iScn == 8 then
    aL2, aR2 SM_GrainStreamer aL, aR, $SM_GRAINSTR
    aL3, aR3 SM_GrainStreamer aL2, aR2, $SM_GRAINSTR
    aL, aR SM_GrainStreamer aL3, aR3, $SM_GRAINSTR
  endif
  outs aL, aIn
endin
</CsInstruments>
<CsScore>
i1 0 4
</CsScore>
</CsoundSynthesizer>
