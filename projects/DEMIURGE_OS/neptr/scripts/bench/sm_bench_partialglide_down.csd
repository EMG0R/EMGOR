<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; SoundMagic kernels bench. Render:  csound --omacro:SCN=<n> -o <out.wav> sm_bench_partialglide.csd
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
#include "../../csound/spectral/sm_partialglide.orc"

opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop

instr 1
  iScn = $SCN
  iN = $N
  p3 = 4
  SM_Set $SMP_PARTGLD_RANDOMIZE_PHASES, 0
  if iScn == 1 then
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, 0
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_VARIANCE, 0
  elseif iScn == 2 then
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, 600
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_VARIANCE, 0
  elseif iScn == 3 then
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, 0
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_VARIANCE, 1200
  elseif iScn == 5 then
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, 400
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_VARIANCE, 0
    SM_Set $SMP_PARTGLD_FADE_IN_WINDOWS, 20
  elseif iScn == 6 then
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, 400
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_VARIANCE, 0
    SM_Set $SMP_PARTGLD_ZERO_TRANSIENTS, 1
  elseif iScn == 7 then
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, 400
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_VARIANCE, 0
    SM_Set $SMP_PARTGLD_MIRROR_BIN, 2
  elseif iScn == 8 then
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, 400
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_VARIANCE, 0
    SM_Set $SMP_PARTGLD_MIRROR_BIN, 100
  elseif iScn == 9 then
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, -400
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_VARIANCE, 0
  elseif iScn == 4 then
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_PER_SECOND, 400
    SM_Set $SMP_PARTGLD_PITCH_SHIFT_VARIANCE, 0
    SM_Set $SMP_PARTGLD_MIRROR_BIN, 2
    SM_Set $SMP_PARTGLD_FADE_IN_WINDOWS, 20
    SM_Set $SMP_PARTGLD_ZERO_TRANSIENTS, 1
  endif
  aIn = 0.15 * (oscili:a(1, 220) + oscili:a(1, 440) + oscili:a(1, 880) + oscili:a(1, 1760))
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_PartialGlide kM, kF, $SM_PARTGLD, kNew, iN
  kM, kF SM_Common kM, kF, $SM_PARTGLD, kNew, iN
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
