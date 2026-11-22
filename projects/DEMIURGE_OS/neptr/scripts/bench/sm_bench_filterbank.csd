<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; SoundMagic kernels bench. Render:  csound --omacro:SCN=<n> -o <out.wav> sm_bench_filterbank.csd
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
#include "../../csound/spectral/sm_filterbank.orc"

opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop

instr 1
  iScn = $SCN
  iN = $N
  p3 = 4
  SM_Set $SMP_FILTBANK_RANDOMIZE_PHASES, 0
  if iScn == 2 then
    SM_Set $SMP_FILTBANK_NUMBER_OF_FILTERS, 10
    SM_Set $SMP_FILTBANK_BASE_FREQUENCY, 440
    SM_Set $SMP_FILTBANK_INTERVAL, 1200
  elseif iScn == 3 then
    SM_Set $SMP_FILTBANK_NUMBER_OF_FILTERS, 400
    SM_Set $SMP_FILTBANK_BASE_FREQUENCY, 20
    SM_Set $SMP_FILTBANK_INTERVAL, 2400
    SM_Set $SMP_FILTBANK_TRANSPOSITION_TYPE, 2
    SM_Set $SMP_FILTBANK_HARMONIC_GAIN, 1
    SM_Set $SMP_FILTBANK_SIDE_BINS, 40
    SM_Set $SMP_FILTBANK_SIDE_BIN_GAIN, 0
    SM_Set $SMP_FILTBANK_FEEDBACK, 99
  elseif iScn == 4 then
    SM_Set $SMP_FILTBANK_NUMBER_OF_FILTERS, 8
    SM_Set $SMP_FILTBANK_BASE_FREQUENCY, 110
    SM_Set $SMP_FILTBANK_INTERVAL, 700
    SM_Set $SMP_FILTBANK_TRANSPOSITION_TYPE, 1
    SM_Set $SMP_FILTBANK_SIDE_BINS, 2
    SM_Set $SMP_FILTBANK_FEEDBACK, 80
    SM_Set $SMP_FILTBANK_WET_DRY_MIX, 70
  elseif iScn == 5 then
    SM_Set $SMP_FILTBANK_NUMBER_OF_FILTERS, 1
    SM_Set $SMP_FILTBANK_BASE_FREQUENCY, 22050
    SM_Set $SMP_FILTBANK_WET_DRY_MIX, 0
  endif
  aIn = 0.3 * rnd31:a(1, 0)
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_Filterbank kM, kF, $SM_FILTBANK, kNew, iN
  kM, kF SM_Common kM, kF, $SM_FILTBANK, kNew, iN
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
