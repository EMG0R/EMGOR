<CsoundSynthesizer>
<CsOptions>
-d -W -f
</CsOptions>
<CsInstruments>
; Phase-3 time-domain rack bench (offline). csound --omacro:SCN=<n> -o /tmp/x.wav
; SCN 40 = fx 19+21 on, both off at t=3 s (L==R expected after ~4 s) | SCN 0 all off (dry check) | 19..22 single fx | 30 all four + FREEZE/BLUR/PITCH spectral
; Output: L = rack-processed chain, R = pure dry (same input) for comparison; the
; chain bus is emulated exactly like signal_chain.orc (read last cycle, publish this one).
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
giSMP      ftgen 0, 0, -768, -2, 0
giSMon     ftgen 0, 0, -24, -2, 0
giSMres    ftgen 0, 0, -24, -2, 2
gkSMActive     init 0
gkSMDefaultFFT init 2
gaSMChainL init 0
gaSMChainR init 0
#include "../../csound/spectral/sm_params.orc"
#include "../../csound/spectral/sm_core.orc"
#include "../../csound/spectral/sm_freezing.orc"
#include "../../csound/spectral/sm_blurring.orc"
#include "../../csound/spectral/sm_averaging.orc"
#include "../../csound/spectral/sm_binshift.orc"
#include "../../csound/spectral/sm_pitchshift.orc"
#include "../../csound/spectral/sm_shuffle.orc"
#include "../../csound/spectral/sm_dronemaker.orc"
#include "../../csound/spectral/sm_gateandhold.orc"
#include "../../csound/spectral/sm_weave.orc"
#include "../../csound/spectral/sm_shimmer.orc"
#include "../../csound/spectral/sm_pulsing.orc"
#include "../../csound/spectral/sm_harmonizer.orc"
#include "../../csound/spectral/sm_stretch.orc"
#include "../../csound/spectral/sm_tracing.orc"
#include "../../csound/spectral/sm_filterbank.orc"
#include "../../csound/spectral/sm_glidingfilters.orc"
#include "../../csound/spectral/sm_granulation.orc"
#include "../../csound/spectral/sm_emergence.orc"
#include "../../csound/spectral/sm_partialglide.orc"
#include "../../csound/spectral/sm_chorus.orc"
#include "../../csound/spectral/sm_grainstreamer.orc"
#include "../../csound/spectral/sm_ideefixer.orc"
#include "../../csound/spectral/sm_superfilterbank.orc"
#include "../../csound/spectral/sm_rack.orc"
tableicopy giSMP, giSMPdef
instr 1
  iScn = $SCN
  p3 = 10
  if iScn >= 19 && iScn <= 22 then
    tabw_i 1, iScn, giSMon
  elseif iScn == 30 then
    tabw_i 1, 19, giSMon
    tabw_i 1, 20, giSMon
    tabw_i 1, 21, giSMon
    tabw_i 1, 22, giSMon
    tabw_i 1, 3, giSMon
    tabw_i 1, 1, giSMon
    tabw_i 1, 11, giSMon
  endif
  if iScn == 40 then
    tabw_i 1, 21, giSMon
    tabw_i 1, 19, giSMon
  endif
  kt = timeinsts()
  aIn = 0.3 * (oscili:a(1, 220) + 0.5 * oscili:a(1, 330 + 20*kt) + 0.4 * rnd31:a(1, 0, 0))
  aIn = aIn * interp(kt % 1.1 < 0.5 ? 1 : 0.2)
  aPrev = aIn
  if (gkSMActive > 0.5) then
    aO = gaSMChainL
    gaSMChainL = aPrev
    gaSMChainR = aPrev
    aPrev = aO
  else
    gaSMChainL = 0
    gaSMChainR = 0
  endif
  outs aPrev, aIn
endin
instr 2
  tabw_i 0, 21, giSMon
  tabw_i 0, 19, giSMon
endin
</CsInstruments>
<CsScore>
i249 0 12
i1 0 10
i2 3 0.1
e
</CsScore>
</CsoundSynthesizer>
