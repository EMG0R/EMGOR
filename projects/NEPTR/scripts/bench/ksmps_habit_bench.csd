<CsoundSynthesizer>
<CsOptions>
-W -f -m0
</CsOptions>
<CsInstruments>
; HABIT block-size bench + grain-duration-floor probe.
;   csound ksmps_habit_bench.csd -o x.wav --omacro:KSMPS=64
;   csound ksmps_habit_bench.csd -o x.wav --omacro:KSMPS=32
;
; instr 90 prints the two candidate floors so the value of iMinDur is
; observable directly at whatever block size is in force.
#ifndef KSMPS
#define KSMPS #64#
#endif
sr = 48000
ksmps = $KSMPS
nchnls = 2
0dbfs = 1
seed 11

giHabitMem ftgen 0, 0, 8388608, 2, 0
giHanning  ftgen 0, 0, 1024, 20, 2, 1

gaHabitSendL init 0
gaHabitSendR init 0
gaHabitWetL  init 0
gaHabitWetR  init 0
gkHabitBypass  init 0
gkHabitMix     init 1
gkHabitSize    init 0.35
gkHabitRepeats init 0.45
gkHabitScan    init 0
gkHabitBlend   init 1

#include "../../csound/effects/habit.orc"

instr 1
  kT times
  ; broadband-ish, fully deterministic source so grain playback is exercised
  a1 poscil 0.20, 220
  a2 poscil 0.12, 337
  a3 poscil 0.08, 911
  aIn = a1 + a2 + a3
  gaHabitSendL = aIn
  gaHabitSendR = aIn
  gkHab_rec = 1
  outs gaHabitWetL, gaHabitWetR
  clear gaHabitWetL, gaHabitWetR
endin

; grain-duration floor probe — prints once
instr 90
  iTimeFloor  = 128 / sr            ; the block-size-independent term
  iBlockFloor = 2 * ksmps / sr      ; the >= 2 k-cycle term
  prints "KSMPS %d  timeFloor %.6f s  blockFloor %.6f s  iMinDur %.6f s\n", ksmps, iTimeFloor, iBlockFloor, (iTimeFloor > iBlockFloor ? iTimeFloor : iBlockFloor)
  turnoff
endin
</CsInstruments>
<CsScore>
i90 0 0.1
i1  0 20
i64 0 20
</CsScore>
</CsoundSynthesizer>
