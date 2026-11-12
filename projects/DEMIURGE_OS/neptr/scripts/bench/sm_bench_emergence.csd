<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; Emergence bench. csound --omacro:SCN=<n> --omacro:FFT=<N> -o /tmp/x.wav sm_bench_emergence.csd
; SCN 1 defaults, 2 fast env (in2 hold2 out2 recycle), 3 no recycle short, 4 max bins 1, 5 max bins 50 fuzz 10, 6 all zero
; out: L processed, R dry
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_emergence.orc"
opcode SM_Set, 0, ii
  ii, iv xin
  tabw_i iv, ii, giSMP
endop
instr 1
  iScn = $SCN
  iN = $FFT
  p3 = 5
  if iScn == 2 then
    SM_Set $SMP_EMERGE_FADE_IN_WINDOWS, 2
    SM_Set $SMP_EMERGE_HOLD_WINDOWS, 2
    SM_Set $SMP_EMERGE_FADE_OUT_WINDOWS, 2
    SM_Set $SMP_EMERGE_RECYCLE_FADED_PEAKS, 1
  elseif iScn == 3 then
    SM_Set $SMP_EMERGE_FADE_IN_WINDOWS, 2
    SM_Set $SMP_EMERGE_HOLD_WINDOWS, 2
    SM_Set $SMP_EMERGE_FADE_OUT_WINDOWS, 2
  elseif iScn == 4 then
    SM_Set $SMP_EMERGE_MAX_BINS_PER_PARTIAL, 1
  elseif iScn == 5 then
    SM_Set $SMP_EMERGE_MAX_BINS_PER_PARTIAL, 50
    SM_Set $SMP_EMERGE_FUZZINESS, 10
  elseif iScn == 6 then
    SM_Set $SMP_EMERGE_FADE_IN_WINDOWS, 0
    SM_Set $SMP_EMERGE_HOLD_WINDOWS, 0
    SM_Set $SMP_EMERGE_FADE_OUT_WINDOWS, 0
  endif
  aIn = 0.15 * (oscili:a(1, 220) + oscili:a(1, 440) + oscili:a(1, 880) + oscili:a(1, 1760)) + 0.02*rnd31:a(1,0)
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_Emergence kM, kF, $SM_EMERGE, kNew, iN
  kM, kF SM_Common kM, kF, $SM_EMERGE, kNew, iN
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
