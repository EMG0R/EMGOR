<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; Tracing bench. csound --omacro:TYP=<0..7> --omacro:FFT=<iN> -o out.wav sm_bench_tracing.csd
; Input: 8 harmonics of 200 Hz with falling amplitude + a little noise. L = processed, R = dry.
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "../../csound/spectral/sm_params.orc"
giSMP ftgen 0, 0, -768, -2, 0
tableicopy giSMP, giSMPdef
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_tracing.orc"
#ifndef TYP
#define TYP #0#
#end
#ifndef FFT
#define FFT #2048#
#end
#ifndef NB1
#define NB1 #16#
#end
#ifndef NB2
#define NB2 #50#
#end
#ifndef THR
#define THR #-20#
#end
instr 1
  iN = $FFT
  p3 = 4
  tabw_i $TYP, $SMP_TRACING_TRACING_TYPE, giSMP
  tabw_i $NB1, $SMP_TRACING_NUMBER_OF_BINS_1, giSMP
  tabw_i $NB2, $SMP_TRACING_NUMBER_OF_BINS_2, giSMP
  tabw_i $THR, $SMP_TRACING_THRESHOLD, giSMP
  aIn = 0.4*oscili:a(1,200) + 0.2*oscili:a(1,400) + 0.133*oscili:a(1,600) + 0.1*oscili:a(1,800) + 0.08*oscili:a(1,1000) + 0.067*oscili:a(1,1200) + 0.057*oscili:a(1,1400) + 0.05*oscili:a(1,1600)
  aIn += 0.01 * rnd31:a(1, 0)
  aIn *= 0.5
  fIn pvsanal aIn, iN, iN/4, iN, 1
  iNb = iN/2 + 1
  kM[] init iNb
  kF[] init iNb
  kFr pvs2tab kM, kF, fIn
  kNew = changed:k(kFr)
  kM, kF SM_Tracing kM, kF, $SM_TRACING, kNew, iN
  kM, kF SM_Common kM, kF, $SM_TRACING, kNew, iN
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
