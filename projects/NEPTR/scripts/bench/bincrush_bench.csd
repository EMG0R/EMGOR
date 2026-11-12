<CsoundSynthesizer>
<CsOptions>
-o /tmp/bincrush_bench.wav -W
</CsOptions>
<CsInstruments>
; globals.orc references $PRESET_BANK (defined in neptrPhase4.csd's preamble in
; the real build). Mirror that default here so the bench compiles standalone.
#ifndef PRESET_BANK
#define PRESET_BANK #0#
#endif

#include "../../csound/globals.orc"
#include "../../csound/effects/bincrush.orc"

; Scenario = pedal position, via --omacro:SCN=N  (0..4 -> pedal 0,.25,.5,.75,1).
; Renders the REAL instr 121 fed a steady rich-harmonic source so the surviving
; bin count is easy to measure. Override -o per scenario on the command line.
#ifndef SCN
#define SCN #0#
#endif

; driver (instr 1 < 121, so the send survives until 121 reads it; not zeroed)
instr 1
  iScn = $SCN
  gkBinBypass = 0
  gkBinPedal  = iScn * 0.25
  ; rich harmonic input (sawtooth = many bins to crush), guitar-ish G3
  aSaw vco2 0.3, 196
  gaBinSendL = aSaw
  gaBinSendR = aSaw
  ; wet is 1 block late here (instr 1 runs before 121) — fine for spectral analysis
  outs gaBinWetL, gaBinWetR
endin
</CsInstruments>
<CsScore>
i 121 0 3
i 1   0 3
</CsScore>
</CsoundSynthesizer>
