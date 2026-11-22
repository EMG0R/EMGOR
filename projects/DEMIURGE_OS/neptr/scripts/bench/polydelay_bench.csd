<CsoundSynthesizer>
<CsOptions>
-o /tmp/claude-1000/-home-server/78875ea7-61b8-4684-aec1-9f8fa15a30cd/scratchpad/polydelay_bench.wav -W -3 -m0
</CsOptions>
<CsInstruments>
#ifndef PRESET_BANK
#define PRESET_BANK #0#
#end
#include "../../csound/globals.orc"
#include "../../csound/effects/movedelay.orc"

; POLY DLY v5 bench. Override with --omacro: (all optional):
;   ATT (0) DEC (0) REPS (4) INTV (7) DUR (6) ; output path via -o
; Input: BURST s (default 0.2; use --omacro:BURST=1 for the 1 s burst) 220 Hz sine, 10 ms fades, then silence. NumL1 DenL2 NumR1 DenR3,
; 120 BPM, no loop. Output: L = dry+L stack, R = dry+R stack.
#ifndef ATT
#define ATT #0#
#end
#ifndef DEC
#define DEC #0#
#end
#ifndef BURST
#define BURST #0.2#
#end
#ifndef REPS
#define REPS #4#
#end
#ifndef INTV
#define INTV #7#
#end

gaRefDry init 0
instr 1
  gkChaseBlissBPM = 120
  gkLpMasterLen = 0
  gkMvdBypass = 0
  gkMvdNumL = 1
  gkMvdDenL = 2
  gkMvdNumR = 1
  gkMvdDenR = 3
  gkMvdAtt = $ATT
  gkMvdDec = $DEC
  gkMvdReps = $REPS
  gkMvdIntv = $INTV
  kEnv linseg 0, 0.01, 1, $BURST-0.02, 1, 0.01, 0, 0.01, 0
  aSig poscil 0.5 * kEnv, 220
  gaRefDry = aSig
  gaMvdInL = aSig
  gaMvdInR = aSig
endin
; effect instr must run after the feeder (higher number, same start)
instr 193
  ; output = wet only (dry subtracted) so overlapping repeats are measurable
  outs (gaMvdWetL - gaRefDry)*0.25, (gaMvdWetR - gaRefDry)*0.25
  gaMvdInL = 0
  gaMvdInR = 0
endin
</CsInstruments>
<CsScore>
i 1   0 6
i 191 0 6
i 193 0 6
</CsScore>
</CsoundSynthesizer>
