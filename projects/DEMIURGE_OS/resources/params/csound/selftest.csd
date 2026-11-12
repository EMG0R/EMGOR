<CsoundSynthesizer>
<CsOptions>
; null rtaudio: real-time pacing, opens NO audio device. (-n alone runs the score
; in microseconds, before the OSC packet can arrive.)
-+rtaudio=null -o dac -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 64
nchnls = 2
0dbfs = 1
#include "demiurge_params.udo"
instr 1
  dparam_init "test", 0
  kv dparam "/test/ping", 0.1
  k2 dparam "/test/other", 0.2      ; second listener on the same handle must not steal /p
  dpout "/test/ping", kv
endin
</CsInstruments>
<CsScore>
i1 0 60
</CsScore>
</CsoundSynthesizer>
