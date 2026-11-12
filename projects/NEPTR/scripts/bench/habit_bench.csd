<CsoundSynthesizer>
<CsOptions>
-o /tmp/habit_bench.wav -W
</CsOptions>
<CsInstruments>
#include "../../csound/globals.orc"
#include "../../csound/effects/habit.orc"

; test driver: 2 s sine-blip phrase into the send buses, scenario via p4
instr 1
  ; p4 scenarios: 1 = plain echo + size hop, 2 = infinite repeats,
  ;               3 = scanning,              4 = (re-engage; see score)
  iScn = p4

  ; a recognizable phrase: 0.2 s blips stepping 220/330/440 Hz, input stops at t=2
  kPhr  = phasor:k(0.5)
  kStep = int(phasor:k(1/6) * 3)
  kFrq  = 220 * (kStep == 0 ? 1 : (kStep == 1 ? 1.5 : 2))
  aOsc  poscil 0.3, kFrq
  iInDur = (iScn == 3 ? 6 : 2)      ; scan test needs more recorded material
  kGate = (kPhr < 0.4 && timeinsts() < iInDur ? 1 : 0)
  aSig  = aOsc * interp(kGate)
  gaHabitSendL = aSig
  gaHabitSendR = aSig

  if (iScn == 1) then
    gkHabitScan = 0
    gkHabitRepeats = 0.65
    ; size hops at t=4 (listen: echoes shorten, NO pitch bend)
    gkHabitSize = (timeinsts() < 4 ? 0.42 : 0.30)
  elseif (iScn == 2) then
    gkHabitScan = 0
    gkHabitSize = 0.30
    gkHabitRepeats = 1.0      ; must settle stable, not run away or die
  elseif (iScn == 3) then
    gkHabitSize = 0.30
    gkHabitRepeats = 0.3
    gkHabitScan = (timeinsts() < 9 ? 0 : 0.8)   ; ghosts after t=9 (reach [1.8..now])
  endif

  ; NOTE: do NOT zero the buses here — instr 1 runs BEFORE instr 64 each cycle,
  ; so the sends must survive until 64 reads them. They're reassigned anyway.
  outs gaHabitSendL * 0.5 + gaHabitWetL, gaHabitSendR * 0.5 + gaHabitWetR
endin
</CsInstruments>
<CsScore>
; scenario chosen by editing these lines (see plan Task 5)
i 64 0 14
i 64 16 14
i 1  0 30 3
</CsScore>
</CsoundSynthesizer>
