<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; GlidingFilters bench: csound --omacro:SCN=n --omacro:N=2048 -o out.wav
; (analyze_sm_kernels.py does this for every scenario). Output = stereo: L processed, R dry.
; (scn 14 writes the kernel-domain magnitude-energy ratio sqrt(E_out/E_in) to R instead of dry)
; SCN: 1..6 neutral (AVG BLUR FRZ SHUF BSHIFT PITCH), 11..16 characteristic (same order),
;      21 freeze factor 0 control, 99 all six kernels chained (CPU).
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1

#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_glidingfilters.orc"

opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop

instr 1
  iScn = $SCN
  iN = $N
  p3 = 8
  SM_Set $SMP_GLIDEFLT_RANDOMIZE_PHASES, 0
  if iScn == 1 then ; defaults
  elseif iScn == 2 then ; one narrow filter, static, long life, 1 kHz
    SM_Set $SMP_GLIDEFLT_NUMBER_OF_FILTERS, 1
    SM_Set $SMP_GLIDEFLT_FILTER_WIDTH, 3
    SM_Set $SMP_GLIDEFLT_START_FREQUENCY, 1000
    SM_Set $SMP_GLIDEFLT_START_FREQUENCY_VARIATION, 1
    SM_Set $SMP_GLIDEFLT_FILTER_DURATION, 20
    SM_Set $SMP_GLIDEFLT_FILTER_DURATION_VAR, 0
    SM_Set $SMP_GLIDEFLT_SILENCE_DURATION, 0
    SM_Set $SMP_GLIDEFLT_SILENCE_DURATION_VAR, 0
    SM_Set $SMP_GLIDEFLT_PITCH_GLIDE_VAR, 0
  elseif iScn == 3 then ; same, glide +1200 c/s
    SM_Set $SMP_GLIDEFLT_NUMBER_OF_FILTERS, 1
    SM_Set $SMP_GLIDEFLT_FILTER_WIDTH, 3
    SM_Set $SMP_GLIDEFLT_START_FREQUENCY, 500
    SM_Set $SMP_GLIDEFLT_START_FREQUENCY_VARIATION, 1
    SM_Set $SMP_GLIDEFLT_FILTER_DURATION, 20
    SM_Set $SMP_GLIDEFLT_FILTER_DURATION_VAR, 0
    SM_Set $SMP_GLIDEFLT_SILENCE_DURATION, 0
    SM_Set $SMP_GLIDEFLT_SILENCE_DURATION_VAR, 0
    SM_Set $SMP_GLIDEFLT_PITCH_GLIDE_PER_SECOND, 1200
    SM_Set $SMP_GLIDEFLT_PITCH_GLIDE_VAR, 0
  elseif iScn == 4 then ; extremes: 100 filters, width 50, short lives
    SM_Set $SMP_GLIDEFLT_NUMBER_OF_FILTERS, 100
    SM_Set $SMP_GLIDEFLT_FILTER_WIDTH, 50
    SM_Set $SMP_GLIDEFLT_FILTER_DURATION, 0
    SM_Set $SMP_GLIDEFLT_FILTER_DURATION_VAR, 10
    SM_Set $SMP_GLIDEFLT_SILENCE_DURATION, 0
    SM_Set $SMP_GLIDEFLT_SILENCE_DURATION_VAR, 0
    SM_Set $SMP_GLIDEFLT_PITCH_GLIDE_PER_SECOND, -1200
    SM_Set $SMP_GLIDEFLT_PITCH_GLIDE_VAR, 1200
  elseif iScn == 5 then ; wet 0 = transparent
    SM_Set $SMP_GLIDEFLT_WET_DRY_MIX, 0
  elseif iScn == 6 then ; extreme low/high start, max glide, rand phase, +40 dB
    SM_Set $SMP_GLIDEFLT_START_FREQUENCY, 22050
    SM_Set $SMP_GLIDEFLT_START_FREQUENCY_VARIATION, 11025
    SM_Set $SMP_GLIDEFLT_PITCH_GLIDE_PER_SECOND, 1200
    SM_Set $SMP_GLIDEFLT_RANDOMIZE_PHASES, 1
    SM_Set $SMP_GLIDEFLT_GAIN, 40
  endif
  aIn = 0.3 * rnd31:a(1, 0)
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_GlidingFilters kM, kF, $SM_GLIDEFLT, kNew, iN
  kM, kF SM_Common kM, kF, $SM_GLIDEFLT, kNew, iN
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
