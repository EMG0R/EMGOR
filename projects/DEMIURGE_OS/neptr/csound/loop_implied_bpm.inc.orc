; neptr/csound/loop_implied_bpm.inc.orc
;
; LoopImpliedBpm — pure function, loop length in samples -> implied tempo
; (and the winning power-of-two N), by the successive-halving 8th-note search
; documented in full in chase_bliss_clock.orc's ALGORITHM header.
;
; THE SINGLE DEFINITION. 2026-10-01: this search briefly existed twice — as a
; UDO in globals.orc (for effects/looper.orc) and verbatim inline inside
; chase_bliss_clock.orc instr 100 — cross-commented "must stay identical",
; which is how two copies of an algorithm begin to drift. The reason for the
; duplication was real: testing/neptr-clock/test_arbiter.csd #includes
; chase_bliss_clock.orc standalone, deliberately without globals.orc, so that
; file could not depend on anything globals.orc declared.
;
; The fix is a third includer rather than a second copy. This file is included
; by globals.orc, by chase_bliss_clock.orc, and by test_arbiter.csd. Guarded
; below so multiple includes in one orchestra are harmless.
;
; Precondition: kLenSamples > 0. The no-loop case is handled by each caller.

#ifndef LOOP_IMPLIED_BPM_INCLUDED
#define LOOP_IMPLIED_BPM_INCLUDED ##

opcode LoopImpliedBpm, kk, k
  kLenSamples xin
  kLoopSec = kLenSamples / sr

  kBestN       = 2
  kBestBpm     = 30 * 2 / kLoopSec
  kBestDist    = abs(kBestBpm - 120)
  kBestInRange = (kBestBpm >= 80 && kBestBpm <= 160 ? 1 : 0)

  kN = 4
  while kN <= 4096 do
    kBpm  = 30 * kN / kLoopSec
    kInR  = (kBpm >= 80 && kBpm <= 160 ? 1 : 0)
    kDist = abs(kBpm - 120)

    kTake = 0
    if (kInR == 1 && kBestInRange == 0) then
      kTake = 1
    elseif (kInR == kBestInRange && kDist < kBestDist) then
      kTake = 1
    endif

    if (kTake == 1) then
      kBestN       = kN
      kBestBpm     = kBpm
      kBestDist    = kDist
      kBestInRange = kInR
    endif

    kN = kN * 2
  od

  xout kBestBpm, kBestN
endop

#end

