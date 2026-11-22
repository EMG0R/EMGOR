<CsoundSynthesizer>
<CsOptions>
-W -f -m0
</CsOptions>
<CsInstruments>
; ONWARD minimum-segment-floor probe.
;   csound ksmps_onward_floor_probe.csd -o x.wav --omacro:KSMPS=64
;   csound ksmps_onward_floor_probe.csd -o x.wav --omacro:KSMPS=32
;
; Calls OnwGlitch directly with parameters PINNED to the floor (SIZE at its
; 0.02 s clamp, err=1, PLAYBACK error type => |rate| up to 4, harm 4 =>
; |rate*harm| up to 16, so round(0.02/|r| * kr) lands under the floor).
;
; The source table is a LINEAR RAMP, so the segment read is a sawtooth whose
; PERIOD IS the segment duration — measure the period in the wav and you have
; measured the floor, in samples, without touching the opcode's internals.
;
; New-event triggers come off a sample counter with thresholds that are
; multiples of 64, so ksmps 64 and 32 make their decisions on the same sample.
#ifndef PRESET_BANK
#define PRESET_BANK #0#
#endif
#include "../../csound/globals.orc"
#include "../../csound/onset_pick.orc"
#include "../../csound/effects/onward.orc"

seed 20260713

giRamp ftgen 0, 0, 65536, 7, 0, 65536, 1

instr 1
  kN init 0
  kCnt init 0
  kCnt += 1

  kRecS  = 1
  kSizeS = 0.001            ; clamped up to the 0.02 s minimum inside OnwGlitch
  kHarm  = 4
  kErr   = 1
  kType  = 2                ; PLAYBACK errors: random 0.25/0.5/2/4 x speed
  kOct   = 0
  kAdopt = (kCnt == 1 ? 1 : 0)
  ; fresh decision every 12000 samples (0.25 s) — same sample at any ksmps
  kNewEvt = (kN % 12000 == 0 && kN > 0 ? 1 : 0)
  kIsActive = 1

  aL, aR, kB, kRt, kLs, kMu, kDc  OnwGlitch kSizeS, kRecS, kHarm, kNewEvt, kIsActive, kErr, kType, kOct, kAdopt, giRamp
  outs aL, aL
  kN += ksmps
endin
</CsInstruments>
<CsScore>
i1 0 4
</CsScore>
</CsoundSynthesizer>
