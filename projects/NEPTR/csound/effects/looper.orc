;==============================================================
; LOOPER — 5-channel loop recorder/player (menu 10)
; Instruments 210-214, all started alwayson from score.
;
; State machine per channel:
;   0 = idle      (silence, buffer empty or cleared)
;   1 = recording (writing gaLpInL/R into buffer; read pos irrelevant)
;   2 = playing   (reading from buffer, looping at gkLpLen)
;   3 = overdubbing (reading AND writing; input+existing mixed at write pos)
;   4 = stopped   (loop kept, silent)
;   5 = sync-closing (CLOCKED, tap landed early; still recording until the
;       quantize boundary gkLpTarget, then auto-flips to play — see
;       handler/handler_looper.orc LP_QCLOSE)
;   6 = trim-closing (UNCLOCKED, tap landed; bounded backward scan for
;       trailing silence before flipping to play — see LP_QCLOSE / the
;       TRIM-CLOSING block in each channel instrument below)
;
; Gesture model (handled in midi_handler.orc menu 10 dispatch):
;   tap   (<0.4 s):       idle→rec, rec→play, overdub→play
;   hold  (≥0.4 s):       play→overdub, overdub→play
;   double-tap:           any→idle (clear)
;
; Grid-true capture (2026-07-29 Pocket OpGorator integration, spec §4):
;   The FIRST loop closed anywhere (any channel — whichever gets there
;   first) still defines gkLpMasterLen, but no longer just bakes in raw
;   press timing:
;     CLOCKED (gkLpGridAvail==1 — a real external MIDI clock has been seen,
;     or a loop-derived grid already exists): snaps to the nearest
;     whole-beat multiple of gkLpBeatSamples (sr*60/gkGLITCH_bpm). Early
;     click-off defers (state 5) until the boundary is actually reached, so
;     the tail is never cut; late/exact click-off rounds back immediately.
;     UNCLOCKED (no grid yet): free capture, but trims trailing silence via
;     state 6 (scans backward for the last sample above ~-60 dB) instead of
;     baking press-timing silence into the loop; start is untouched.
;   ch2-5 (once a master exists) snap to nearest of {M/4, M/2, M, M×2, M×4}
;   where M = gkLpMasterLen — unchanged, and stays grid-consistent because M
;   is now itself either a whole-beat multiple (CLOCKED) or trailing-silence
;   trimmed (UNCLOCKED).
;
; Varispeed: giLpSpeeds table, index gkLpSpIdx{N}, default 6 = +1.0×
;   Encoder CC22/21/26/25/24 steps the index in menu 10.
;
; Per-sample accuracy via phasor(kr)*ksmps trick (NB: base position
; must be upsamp'd — a() interpolates across the block, which double-
; counts the advance and garbles pitch):
;   phasor at kr completes exactly one cycle per k-block (0..1).
;   Multiply by ksmps → 0..ksmps-1 sample offset within the block.
;   Combined with a k-rate position counter, this gives sample-accurate
;   indexed reads/writes without a per-sample opcode loop.
;   The ramp itself is computed ONCE, in the transport (instr 209), and
;   published as gaLpSampOff — see the note there. Each channel used to
;   run its own copy of the same phasor.
;
; BLOCK SIZE (ksmps) — every `ksmps` in this file is a per-block QUANTITY,
; not a tuning constant, so the looper's behaviour is already independent of
; the block size (verified by render, scripts/bench/ksmps_looper_bench.csd):
;   gkLpClock += ksmps, gkLpWrPos += ksmps, gkLpRdPos += kSpeed*ksmps —
;     sample counters advanced by exactly one block's worth. Same totals.
;   phasor(kr) * ksmps — the ramp completes one cycle per block by
;     construction, so it spans 0..ksmps-1 at whatever the block size is.
;   kLpTrimSteps = 8 * ksmps (state 6) — a per-block WORK BUDGET, not a
;     window. 8*ksmps samples scanned per block x kr blocks per second =
;     8*sr samples per second, i.e. the backward silence scan runs at a
;     fixed 8x real time however the blocks are sized, and it uses the same
;     fraction of each block's deadline. Measured: an identical take closes
;     on the SAME sample (99008) with the SAME trimmed length (48000) at
;     ksmps 64 and ksmps 32.
; The one residual: at speed index 6 the transport lock reads gkLpClock
; AFTER instr 209 has advanced it, so playback runs one block (ksmps
; samples) ahead of the dry input. All five channels share the offset so
; they stay in sync with each other and with their own anchors; only the
; loop-vs-live alignment shifts, by 1.33 ms at ksmps 64 and 0.67 ms at 32.
; Left as-is deliberately: "fixing" it would move today's output by 64
; samples on the user's current setup for no audible gain.
;
; Buffer length is read with ftlen at I-RATE (iBufLen). The looper ftgens
; are fixed-size and are never resized at runtime — looper_persist.orc
; refills their CONTENTS but not their length — so ftlen is a constant and
; re-evaluating it every k-cycle in every channel bought nothing.
;==============================================================

#define LP_HOLD_THRESH #0.4#
#define LP_DBL_WIN     #0.5#

;--------------------------------------------------------------
; Transport — the overarching clock every locked channel derives
; its position from. Runs BEFORE 210-214 each cycle (lower number).
;--------------------------------------------------------------
instr 209
    gkLpClock += ksmps

    ; Shared per-sample block offset (0 .. ksmps-1), computed ONCE here.
    ; Channels 210-214 each used to run their own identical `phasor kr` +
    ; `* ksmps` pair. All five are started from the score at t=0 with the
    ; same (default 0) initial phase and the same kr, so they produced the
    ; bit-identical vector every cycle for the life of the orchestra — five
    ; copies of one number. 209 is the transport and runs BEFORE 210-214
    ; (lower instr number = earlier in the ascending per-cycle order), so
    ; every channel sees this block's value in the same k-cycle it is
    ; written. Output is unchanged sample-for-sample; this only stops
    ; computing the same a-rate ramp five times.
    gaLpSampOff  phasor  kr
    gaLpSampOff  *=      ksmps

    ; Grid-true capture (2026-07-29 Pocket OpGorator integration, spec §4).
    ; A tempo grid is available once either (a) a real external MIDI clock
    ; has been observed (gkGLITCH_extBpmSeen, latched in midi_handler.orc
    ; instr 99 on the first genuine CC119 tempo broadcast) or (b) a master
    ; loop already exists (gkLpMasterLen > 0 — the loop-derived grid
    ; chase_bliss_clock.orc's gkChaseBlissBPM implies). Before either exists
    ; there is nothing to quantize against, so capture stays free-running
    ; (UNCLOCKED — see the LP_QCLOSE macro / state 6 below).
    ; 2026-10-26: gate on a LIVE external clock (gkClkExtLive), not the
    ; process-lifetime latch — the latch stayed 1 with PMOR unplugged and
    ; quantized a free first loop to a phantom grid (see globals.orc).
    gkLpGridAvail = (gkClkExtLive > 0.5 || gkLpMasterLen > 0) ? 1 : 0

    ; Whole-beat length in samples from the arbitrated master tempo
    ; (gkGLITCH_bpm — external CC119 or loop-derived, see
    ; chase_bliss_clock.orc instr 100). Only meaningful while
    ; gkLpGridAvail == 1; instr 209 runs before 210-214 (lower instr number,
    ; ascending execution order) so every channel sees this block's value.
    ; 2026-11-06: while beat lock is live the SAME tempo that integrates gkLpB
    ; (gkLpBpmS) defines the beat, so close quantization and the beat lock agree.
    gkLpBeatSamples = sr * 60 / max((gkLpBLock == 1 && gkLpBpmS > 20 ? gkLpBpmS : gkGLITCH_bpm), 20)   ; bpm<=0 would divide by zero

    ; Master loop length in BEATS (beat-locked channel that holds the master
    ; length). LP_QCLOSE snaps new loops to multiples of this x the CURRENT
    ; beat length, not to the master's length in samples at the old tempo.
    gkLpMasterB = (gkLpMasterLen <= 0 ? 0 : (gkLpLen1 == gkLpMasterLen && gkLpLenB1 > 0 ? gkLpLenB1 : (gkLpLen2 == gkLpMasterLen && gkLpLenB2 > 0 ? gkLpLenB2 : (gkLpLen3 == gkLpMasterLen && gkLpLenB3 > 0 ? gkLpLenB3 : (gkLpLen4 == gkLpMasterLen && gkLpLenB4 > 0 ? gkLpLenB4 : (gkLpLen5 == gkLpMasterLen && gkLpLenB5 > 0 ? gkLpLenB5 : 0))))))
endin

;--------------------------------------------------------------
; Channel 1 — master channel (no grid snap, sets gkLpMasterLen)
;--------------------------------------------------------------
instr 210
    aInL = gaLpInL
    aInR = gaLpInR
    iBufLen = ftlen(giLpBuf1L)

    ; per-sample block offset: shared gaLpSampOff, computed once in instr 209

    ; -- RECORDING (state 1) --
    ; record-start quantization: armed press waits for PMOR's next whole beat
    ; (gkLpArmB1 = that beat in gkLpB); released at once if the lock is lost
    if (gkLpState1 == 1 && gkLpArmB1 > 0) then
        if (gkLpBLock == 0 || gkLpB >= gkLpArmB1) then
            gkLpArmB1   = 0
            gkLpWrPos1  = 0
            gkLpAnchor1 = gkLpClock
        endif
    endif
    if ((gkLpState1 == 1 && gkLpArmB1 == 0) || gkLpState1 == 5) then
        if (gkLpWrPos1 + ksmps <= iBufLen) then
            aBaseW upsamp gkLpWrPos1
            aWrIdx = aBaseW + gaLpSampOff
            tablew aInL, aWrIdx, giLpBuf1L, 0, 0, 0
            tablew aInR, aWrIdx, giLpBuf1R, 0, 0, 0
            gkLpWrPos1 += ksmps
            if (gkLpState1 == 5 && gkLpWrPos1 >= gkLpTarget1) then
                ; sync-close reached the quantize boundary
                gkLpLen1    = gkLpTarget1
                gkLpRdPos1  = gkLpWrPos1 - gkLpTarget1
                gkLpState1  = 2
                gkLpRecBpm1 = gkCommittedBpm
                printks "LP1: SYNCED -> PLAY\n", 0
            endif
        else
            ; Buffer full — auto-close loop
            gkLpLen1      = gkLpWrPos1
            gkLpMasterLen = gkLpWrPos1
            gkLpState1    = 2
            gkLpRdPos1    = 0
            gkLpRecBpm1   = gkCommittedBpm
            printks "LP1: buffer full → PLAY\n", 0
        endif
    endif

    ; -- TRIM-CLOSING (state 6): UNCLOCKED click-off. Bounded backward scan
    ; for the last sample above ~-60 dB (0.001 FS), spread across k-blocks
    ; (not all at once) so a long loop's scan never blows the real-time
    ; deadline in a single block. Start (position 0) is left untouched; if
    ; the whole tail scans as silence, fall back to the raw length
    ; (gkLpTarget1, stashed at click-off) rather than risk erasing audio.
    if (gkLpState1 == 6) then
        kLpTrimSteps1 = 8 * ksmps
        kLpTrimDone1  = 0
        while (kLpTrimSteps1 > 0 && kLpTrimDone1 == 0) do
            if (gkLpTrimPos1 <= 0) then
                gkLpLen1     = gkLpTarget1
                gkLpRdPos1   = 0
                gkLpState1   = 2
                gkLpRecBpm1  = gkCommittedBpm
                kLpTrimDone1 = 1
                printks "LP1: TRIM exhausted -> PLAY (raw kept)\n", 0
            else
                gkLpTrimPos1 -= 1
                kLpTrimSL1 table gkLpTrimPos1, giLpBuf1L, 0, 0, 0
                kLpTrimSR1 table gkLpTrimPos1, giLpBuf1R, 0, 0, 0
                if (abs(kLpTrimSL1) > 0.001 || abs(kLpTrimSR1) > 0.001) then
                    gkLpLen1     = gkLpTrimPos1 + 1
                    gkLpRdPos1   = 0
                    gkLpState1   = 2
                    gkLpRecBpm1  = gkCommittedBpm
                    kLpTrimDone1 = 1
                    printks "LP1: TRIM -> PLAY (%d samples)\n", 0, gkLpLen1
                endif
            endif
            kLpTrimSteps1 -= 1
        od
    endif

    ; Claim the master-transport grid the moment this channel's length is
    ; actually decided — covers the deferred CLOCKED-early finalize (state
    ; 5->2 above) and the UNCLOCKED trim finalize (state 6->2 just above);
    ; a no-op wherever gkLpMasterLen was already set synchronously (immediate
    ; clocked close, or an existing master already claimed by another
    ; channel earlier this k-block).
    if (gkLpState1 == 2 && gkLpMasterLen == 0 && gkLpLen1 > 0) then
        gkLpMasterLen = gkLpLen1

        ; EXACTNESS FIX (2026-10, VARISPEED-WARBLE FIX) — this channel just
        ; DEFINED the master grid for the first time ever. gkLpRecBpm1 was
        ; already stamped above (at the actual close) from gkCommittedBpm
        ; as it stood BEFORE this loop existed; chase_bliss_clock.orc's own
        ; gkCommittedBpm loop-close commit won't see THIS gkLpMasterLen
        ; until NEXT k-block (it reads gkLpMasterLen at the top of its own
        ; block — see that file's "RUNTIME k-block execution order" note).
        ; Left alone, that would mean: THIS block's playback a few lines
        ; below reads gkLpRecBpm1 (old pre-close value) against
        ; gkCommittedBpm (also still old — fine, ratio 1.0) but NEXT block
        ; gkCommittedBpm jumps to the new loop-implied tempo while
        ; gkLpRecBpm1 stays frozen at the old one — a spurious non-1.0
        ; ratio for exactly one commit, the "0.9998" bug this fix exists to
        ; prevent. Pre-empt it: derive the new tempo right now with the same
        ; pure function chase_bliss_clock.orc uses (LoopImpliedBpm,
        ; globals.orc — see its header for why this is a second, deliberately
        ; kept-identical copy rather than one shared call), and stamp BOTH
        ; sides of the eventual ratio in this same k-cycle. chase_bliss's
        ; own commit still fires next block too — same formula, same
        ; length, same answer, a harmless repeat, not a race.
        kLpClaimBpm1, kLpClaimN1 LoopImpliedBpm gkLpMasterLen
        gkCommittedBpm = kLpClaimBpm1
        gkLpRecBpm1    = kLpClaimBpm1
    endif

    ; -- PLAYBACK (states 2 and 3) --
    aOutL = 0
    aOutR = 0
    if ((gkLpState1 == 2 || gkLpState1 == 3) && gkLpLen1 > 0) then
        kSpeed table gkLpSpIdx1, giLpSpeeds
        kLen   = gkLpLen1

        ; VARISPEED-TO-TEMPO (TASK 1, 2026-10; revised 2026-10 VARISPEED-WARBLE
        ; FIX) — composes with (does not replace) the manual speed control
        ; above. gkLpRecBpm1 is the COMMITTED tempo (globals.orc
        ; gkCommittedBpm — a DELIBERATE-decision value, NOT the continuous
        ; PLL-tracked gkGLITCH_bpm) captured at the instant this channel's
        ; loop was closed (see the close paths above + LP_QCLOSE in
        ; handler/handler_looper.orc). The ratio of the LIVE committed tempo
        ; to that captured one is how far this already-recorded audio must
        ; be stretched to land back in tempo — tape-style, pitch rides
        ; along, by design. Using gkCommittedBpm on BOTH sides (not
        ; gkGLITCH_bpm) means an unchanged tempo gives EXACTLY 1.000, not a
        ; near-1.0 value with PLL-noise drift — see globals.orc for why. 0
        ; (never recorded at a known tempo) falls back to 1.0x (no-op).
        ; iLpVarispeedGlide mirrors chase_bliss_clock.orc instr 100's
        ; iChaseBlissGlideTime (0.35 s) so a tempo tap pulls every playing
        ; loop into the new tempo over the same window the master clock
        ; itself glides through on a source handoff. Loop CLOSE
        ; quantization (gkLpBeatSamples, instr 209) is a DIFFERENT consumer
        ; and deliberately still reads the live gkGLITCH_bpm.
        iLpVarispeedGlide = 0.35
        kLpTempoRateTgt1 = (gkLpRecBpm1 > 0 ? gkCommittedBpm / gkLpRecBpm1 : 1)
        ; NaN/inf/absurd-ratio guard (2026-11-02): bad ratio -> 1.0x
        kLpTempoRateTgt1 = (abs(kLpTempoRateTgt1) > 0.1 && abs(kLpTempoRateTgt1) < 10 ? kLpTempoRateTgt1 : 1)
        kLpTempoRate1    portk kLpTempoRateTgt1, iLpVarispeedGlide, 1
        kEffSpeed1 = kSpeed * kLpTempoRate1
        gkLpEffSpd1 = kEffSpeed1   ; read by BLOOPER (true playback rate incl. tempo trim)

        ; TRANSPORT LOCK — at exactly +1x the position is DERIVED from
        ; the clock every cycle (never accumulated): zero inter-channel
        ; drift, and returning from varispeed snaps back onto the grid
        ; exactly where the loop would have been. Only engages while the
        ; varispeed ratio is (within epsilon of) 1.0 too — i.e. this loop's
        ; recorded (committed) tempo still matches the LIVE committed tempo,
        ; so the lock's "position IS exactly clock-derived" invariant stays
        ; true. Since gkCommittedBpm only moves on a deliberate decision (see
        ; above), this is now the NORMAL case — the ratio sits at exactly
        ; 1.0 for as long as nobody has deliberately retuned, not merely
        ; "close to 1.0 modulo PLL noise" as it would with gkGLITCH_bpm on
        ; both sides. Away from 1.0x (a tap retuned since this loop closed)
        ; falls through to the free-running accumulate path below, which IS
        ; the varispeed audio — the whole point of this feature.
        ; ---- BEAT LOCK (2026-11-05) -------------------------------------------
        ; While PMOR's clock is live (gkLpBLock, midi_handler.orc) the read
        ; position is DERIVED from PMOR's beat counter gkLpB, never accumulated
        ; from a tempo ratio: pos = ((gkLpB - recB) mod lenBeats) * len/lenBeats.
        ; A tempo change then only changes the playback RATE (varispeed, as
        ; before) and the loop phase stays on PMOR's beat by construction.
        ; The old path integrated gkCommittedBpm/RecBpm (0.5 BPM deadband, 0.5 s
        ; dwell, 0.35 s glide) and the phase error of every tempo move was
        ; frozen in forever (anchor re-lock only works at exactly 1x).
        ; lenBeats: loop length in beats at the tempo of the moment the lock
        ; engages (quarter-beat snapped when within 0.5 %, else exact).
        if (gkLpBLock == 1 && gkLpSpIdx1 == 6) then
            if (gkLpBHave1 == 0) then
                kBLraw1 = kLen * gkLpBpmS / (60 * sr)
                ; BAR LOCK (2026-11-06): length snapped to a whole beat (min 1) and
                ; recB to an exact integer beat of the ABSOLUTE gkLpB, so sample 0
                ; sits on a real PMOR beat (and bar position, if lenB % 4 == 0).
                ; Sub-beat loops (< 1 beat: M/4, M/2 of a short master) keep the
                ; quarter-beat snap instead of being stretched to a whole beat.
                kBLq1   = round(kBLraw1 * 4) / 4
                gkLpLenB1 = (kBLraw1 >= 1 ? max(1, round(kBLraw1)) : (kBLq1 >= 0.25 && abs(kBLraw1 - kBLq1) < 0.005 * kBLq1 ? kBLq1 : kBLraw1))
                gkLpRecB1 = round(gkLpB - gkLpRdPos1 * gkLpLenB1 / kLen)
                gkLpBHave1 = 1
            endif
            kBLs1 = kLen / gkLpLenB1
            kBLx1 = gkLpB - gkLpRecB1
            kBLx1 = kBLx1 - gkLpLenB1 * floor(kBLx1 / gkLpLenB1)
            gkLpRdPos1 = kBLx1 * kBLs1
            kEffSpeed1 = gkLpBpmS / 60 * kBLs1 / sr
            gkLpEffSpd1 = kEffSpeed1
            gkLpBOn1 = 1
        elseif (gkLpBOn1 == 1) then
            ; lock lost: hand the position back to the clock-derived path in phase
            gkLpAnchor1 = gkLpClock - gkLpRdPos1
            gkLpBOn1 = 0
            gkLpBHave1 = 0
            gkLpLenB1 = 0
        endif
        if (gkLpSpIdx1 == 6 && abs(kLpTempoRate1 - 1) < 0.0005 && gkLpBOn1 == 0) then
            gkLpRdPos1 = (gkLpClock - gkLpAnchor1) % kLen
        endif

        ; NaN/inf read-head guard (a NaN here persists forever: every compare is false)
        gkLpRdPos1 = (abs(gkLpRdPos1) < 1e12 ? gkLpRdPos1 : 0)
        ; A-rate read from current position
        aBaseR upsamp gkLpRdPos1
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed1
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf1L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf1R, 0, 0, 0

        ; NaN/inf safety net: zero this block, heal the poisoned stretch of the
        ; buffer, skip the overdub write. Recording is otherwise left intact.
        kLpBad1 NF_Chk aOutL, aOutR
        if (kLpBad1 == 1) then
            aOutL = 0
            aOutR = 0
            LP_Heal gkLpRdPos1, abs(kEffSpeed1) * ksmps + 4, kLen, giLpBuf1L, giLpBuf1R
        endif
        NF_Log "looper ch1 playback", kLpBad1

        ; Overdub: mix input into buffer at same position
        if (gkLpState1 == 3 && kLpBad1 == 0) then
            ; sum clamped to +-8 (+18 dB): unity-feedback overdub can no longer run away
            tablew limit(aInL + aOutL, -8, 8), aRdPos, giLpBuf1L, 0, 0, 0
            tablew limit(aInR + aOutR, -8, 8), aRdPos, giLpBuf1R, 0, 0, 0
        endif

        ; Advance k-rate read position
        gkLpRdPos1 += kEffSpeed1 * ksmps
        if (gkLpRdPos1 >= kLen) then
            gkLpRdPos1 -= kLen
        elseif (gkLpRdPos1 < 0) then
            gkLpRdPos1 += kLen
        endif
    else
        gkLpBOn1 = 0
        if (gkLpState1 != 4 || gkLpBLock == 0) then
            gkLpBHave1 = 0
            gkLpLenB1 = 0
        endif
    endif

    gaLpOutL += aOutL
    gaLpOutR += aOutR
endin

;--------------------------------------------------------------
; Channel 2 — with grid-snap to ch1 master length
;--------------------------------------------------------------
instr 211
    aInL = gaLpInL
    aInR = gaLpInR
    iBufLen = ftlen(giLpBuf2L)

    ; per-sample block offset: shared gaLpSampOff, computed once in instr 209

    ; record-start quantization: armed press waits for PMOR's next whole beat
    ; (gkLpArmB2 = that beat in gkLpB); released at once if the lock is lost
    if (gkLpState2 == 1 && gkLpArmB2 > 0) then
        if (gkLpBLock == 0 || gkLpB >= gkLpArmB2) then
            gkLpArmB2   = 0
            gkLpWrPos2  = 0
            gkLpAnchor2 = gkLpClock
        endif
    endif
    if ((gkLpState2 == 1 && gkLpArmB2 == 0) || gkLpState2 == 5) then
        if (gkLpWrPos2 + ksmps <= iBufLen) then
            aBaseW upsamp gkLpWrPos2
            aWrIdx = aBaseW + gaLpSampOff
            tablew aInL, aWrIdx, giLpBuf2L, 0, 0, 0
            tablew aInR, aWrIdx, giLpBuf2R, 0, 0, 0
            gkLpWrPos2 += ksmps
            if (gkLpState2 == 5 && gkLpWrPos2 >= gkLpTarget2) then
                ; sync-close reached the quantize boundary
                gkLpLen2    = gkLpTarget2
                gkLpRdPos2  = gkLpWrPos2 - gkLpTarget2
                gkLpState2  = 2
                gkLpRecBpm2 = gkCommittedBpm
                printks "LP2: SYNCED -> PLAY\n", 0
            endif
        else
            gkLpLen2    = gkLpWrPos2
            gkLpState2  = 2
            gkLpRdPos2  = 0
            gkLpRecBpm2 = gkCommittedBpm
        endif
    endif

    ; -- TRIM-CLOSING (state 6): UNCLOCKED click-off. Bounded backward scan
    ; for the last sample above ~-60 dB (0.001 FS), spread across k-blocks
    ; (not all at once) so a long loop's scan never blows the real-time
    ; deadline in a single block. Start (position 0) is left untouched; if
    ; the whole tail scans as silence, fall back to the raw length
    ; (gkLpTarget2, stashed at click-off) rather than risk erasing audio.
    if (gkLpState2 == 6) then
        kLpTrimSteps2 = 8 * ksmps
        kLpTrimDone2  = 0
        while (kLpTrimSteps2 > 0 && kLpTrimDone2 == 0) do
            if (gkLpTrimPos2 <= 0) then
                gkLpLen2     = gkLpTarget2
                gkLpRdPos2   = 0
                gkLpState2   = 2
                gkLpRecBpm2  = gkCommittedBpm
                kLpTrimDone2 = 1
                printks "LP2: TRIM exhausted -> PLAY (raw kept)\n", 0
            else
                gkLpTrimPos2 -= 1
                kLpTrimSL2 table gkLpTrimPos2, giLpBuf2L, 0, 0, 0
                kLpTrimSR2 table gkLpTrimPos2, giLpBuf2R, 0, 0, 0
                if (abs(kLpTrimSL2) > 0.001 || abs(kLpTrimSR2) > 0.001) then
                    gkLpLen2     = gkLpTrimPos2 + 1
                    gkLpRdPos2   = 0
                    gkLpState2   = 2
                    gkLpRecBpm2  = gkCommittedBpm
                    kLpTrimDone2 = 1
                    printks "LP2: TRIM -> PLAY (%d samples)\n", 0, gkLpLen2
                endif
            endif
            kLpTrimSteps2 -= 1
        od
    endif

    ; Claim the master-transport grid the moment this channel's length is
    ; actually decided — covers the deferred CLOCKED-early finalize (state
    ; 5->2 above) and the UNCLOCKED trim finalize (state 6->2 just above);
    ; a no-op wherever gkLpMasterLen was already set synchronously (immediate
    ; clocked close, or an existing master already claimed by another
    ; channel earlier this k-block).
    if (gkLpState2 == 2 && gkLpMasterLen == 0 && gkLpLen2 > 0) then
        gkLpMasterLen = gkLpLen2

        ; EXACTNESS FIX (2026-10, VARISPEED-WARBLE FIX) — see channel 1
        ; (instr 210) for the full rationale; identical pattern here.
        kLpClaimBpm2, kLpClaimN2 LoopImpliedBpm gkLpMasterLen
        gkCommittedBpm = kLpClaimBpm2
        gkLpRecBpm2  = kLpClaimBpm2
    endif

    aOutL = 0
    aOutR = 0
    if ((gkLpState2 == 2 || gkLpState2 == 3) && gkLpLen2 > 0) then
        kSpeed table gkLpSpIdx2, giLpSpeeds
        kLen   = gkLpLen2

        ; VARISPEED-TO-TEMPO (TASK 1, 2026-10) — composes with (does not
        ; replace) the manual speed control above. See channel 1 for the
        ; full rationale comment; identical pattern here.
        iLpVarispeedGlide = 0.35
        kLpTempoRateTgt2 = (gkLpRecBpm2 > 0 ? gkCommittedBpm / gkLpRecBpm2 : 1)
        ; NaN/inf/absurd-ratio guard (2026-11-02): bad ratio -> 1.0x
        kLpTempoRateTgt2 = (abs(kLpTempoRateTgt2) > 0.1 && abs(kLpTempoRateTgt2) < 10 ? kLpTempoRateTgt2 : 1)
        kLpTempoRate2    portk kLpTempoRateTgt2, iLpVarispeedGlide, 1
        kEffSpeed2 = kSpeed * kLpTempoRate2
        gkLpEffSpd2 = kEffSpeed2   ; read by BLOOPER (true playback rate incl. tempo trim)

        ; TRANSPORT LOCK — at exactly +1x the position is DERIVED from
        ; the clock every cycle (never accumulated): zero inter-channel
        ; drift, and returning from varispeed snaps back onto the grid
        ; exactly where the loop would have been. Only engages while the
        ; varispeed ratio is (within epsilon of) 1.0 too — see channel 1.
        ; ---- BEAT LOCK (2026-11-05) -------------------------------------------
        ; While PMOR's clock is live (gkLpBLock, midi_handler.orc) the read
        ; position is DERIVED from PMOR's beat counter gkLpB, never accumulated
        ; from a tempo ratio: pos = ((gkLpB - recB) mod lenBeats) * len/lenBeats.
        ; A tempo change then only changes the playback RATE (varispeed, as
        ; before) and the loop phase stays on PMOR's beat by construction.
        ; The old path integrated gkCommittedBpm/RecBpm (0.5 BPM deadband, 0.5 s
        ; dwell, 0.35 s glide) and the phase error of every tempo move was
        ; frozen in forever (anchor re-lock only works at exactly 1x).
        ; lenBeats: loop length in beats at the tempo of the moment the lock
        ; engages (quarter-beat snapped when within 0.5 %, else exact).
        if (gkLpBLock == 1 && gkLpSpIdx2 == 6) then
            if (gkLpBHave2 == 0) then
                kBLraw2 = kLen * gkLpBpmS / (60 * sr)
                ; BAR LOCK (2026-11-06): length snapped to a whole beat (min 1) and
                ; recB to an exact integer beat of the ABSOLUTE gkLpB, so sample 0
                ; sits on a real PMOR beat (and bar position, if lenB % 4 == 0).
                ; Sub-beat loops (< 1 beat: M/4, M/2 of a short master) keep the
                ; quarter-beat snap instead of being stretched to a whole beat.
                kBLq2   = round(kBLraw2 * 4) / 4
                gkLpLenB2 = (kBLraw2 >= 1 ? max(1, round(kBLraw2)) : (kBLq2 >= 0.25 && abs(kBLraw2 - kBLq2) < 0.005 * kBLq2 ? kBLq2 : kBLraw2))
                gkLpRecB2 = round(gkLpB - gkLpRdPos2 * gkLpLenB2 / kLen)
                gkLpBHave2 = 1
            endif
            kBLs2 = kLen / gkLpLenB2
            kBLx2 = gkLpB - gkLpRecB2
            kBLx2 = kBLx2 - gkLpLenB2 * floor(kBLx2 / gkLpLenB2)
            gkLpRdPos2 = kBLx2 * kBLs2
            kEffSpeed2 = gkLpBpmS / 60 * kBLs2 / sr
            gkLpEffSpd2 = kEffSpeed2
            gkLpBOn2 = 1
        elseif (gkLpBOn2 == 1) then
            ; lock lost: hand the position back to the clock-derived path in phase
            gkLpAnchor2 = gkLpClock - gkLpRdPos2
            gkLpBOn2 = 0
            gkLpBHave2 = 0
            gkLpLenB2 = 0
        endif
        if (gkLpSpIdx2 == 6 && abs(kLpTempoRate2 - 1) < 0.0005 && gkLpBOn2 == 0) then
            gkLpRdPos2 = (gkLpClock - gkLpAnchor2) % kLen
        endif

        aBaseR upsamp gkLpRdPos2
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed2
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf2L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf2R, 0, 0, 0

        ; NaN/inf safety net: zero this block, heal the poisoned stretch of the
        ; buffer, skip the overdub write. Recording is otherwise left intact.
        kLpBad2 NF_Chk aOutL, aOutR
        if (kLpBad2 == 1) then
            aOutL = 0
            aOutR = 0
            LP_Heal gkLpRdPos2, abs(kEffSpeed2) * ksmps + 4, kLen, giLpBuf2L, giLpBuf2R
        endif
        NF_Log "looper ch2 playback", kLpBad2

        if (gkLpState2 == 3 && kLpBad2 == 0) then
            ; sum clamped to +-8 (+18 dB): unity-feedback overdub can no longer run away
            tablew limit(aInL + aOutL, -8, 8), aRdPos, giLpBuf2L, 0, 0, 0
            tablew limit(aInR + aOutR, -8, 8), aRdPos, giLpBuf2R, 0, 0, 0
        endif

        gkLpRdPos2 += kEffSpeed2 * ksmps
        if (gkLpRdPos2 >= kLen) then
            gkLpRdPos2 -= kLen
        elseif (gkLpRdPos2 < 0) then
            gkLpRdPos2 += kLen
        endif
    else
        gkLpBOn2 = 0
        if (gkLpState2 != 4 || gkLpBLock == 0) then
            gkLpBHave2 = 0
            gkLpLenB2 = 0
        endif
    endif

    gaLpOutL += aOutL
    gaLpOutR += aOutR
endin

;--------------------------------------------------------------
; Channel 3
;--------------------------------------------------------------
instr 212
    aInL = gaLpInL
    aInR = gaLpInR
    iBufLen = ftlen(giLpBuf3L)

    ; per-sample block offset: shared gaLpSampOff, computed once in instr 209

    ; record-start quantization: armed press waits for PMOR's next whole beat
    ; (gkLpArmB3 = that beat in gkLpB); released at once if the lock is lost
    if (gkLpState3 == 1 && gkLpArmB3 > 0) then
        if (gkLpBLock == 0 || gkLpB >= gkLpArmB3) then
            gkLpArmB3   = 0
            gkLpWrPos3  = 0
            gkLpAnchor3 = gkLpClock
        endif
    endif
    if ((gkLpState3 == 1 && gkLpArmB3 == 0) || gkLpState3 == 5) then
        if (gkLpWrPos3 + ksmps <= iBufLen) then
            aBaseW upsamp gkLpWrPos3
            aWrIdx = aBaseW + gaLpSampOff
            tablew aInL, aWrIdx, giLpBuf3L, 0, 0, 0
            tablew aInR, aWrIdx, giLpBuf3R, 0, 0, 0
            gkLpWrPos3 += ksmps
            if (gkLpState3 == 5 && gkLpWrPos3 >= gkLpTarget3) then
                ; sync-close reached the quantize boundary
                gkLpLen3    = gkLpTarget3
                gkLpRdPos3  = gkLpWrPos3 - gkLpTarget3
                gkLpState3  = 2
                gkLpRecBpm3 = gkCommittedBpm
                printks "LP3: SYNCED -> PLAY\n", 0
            endif
        else
            gkLpLen3    = gkLpWrPos3
            gkLpState3  = 2
            gkLpRdPos3  = 0
            gkLpRecBpm3 = gkCommittedBpm
        endif
    endif

    ; -- TRIM-CLOSING (state 6): UNCLOCKED click-off. Bounded backward scan
    ; for the last sample above ~-60 dB (0.001 FS), spread across k-blocks
    ; (not all at once) so a long loop's scan never blows the real-time
    ; deadline in a single block. Start (position 0) is left untouched; if
    ; the whole tail scans as silence, fall back to the raw length
    ; (gkLpTarget3, stashed at click-off) rather than risk erasing audio.
    if (gkLpState3 == 6) then
        kLpTrimSteps3 = 8 * ksmps
        kLpTrimDone3  = 0
        while (kLpTrimSteps3 > 0 && kLpTrimDone3 == 0) do
            if (gkLpTrimPos3 <= 0) then
                gkLpLen3     = gkLpTarget3
                gkLpRdPos3   = 0
                gkLpState3   = 2
                gkLpRecBpm3  = gkCommittedBpm
                kLpTrimDone3 = 1
                printks "LP3: TRIM exhausted -> PLAY (raw kept)\n", 0
            else
                gkLpTrimPos3 -= 1
                kLpTrimSL3 table gkLpTrimPos3, giLpBuf3L, 0, 0, 0
                kLpTrimSR3 table gkLpTrimPos3, giLpBuf3R, 0, 0, 0
                if (abs(kLpTrimSL3) > 0.001 || abs(kLpTrimSR3) > 0.001) then
                    gkLpLen3     = gkLpTrimPos3 + 1
                    gkLpRdPos3   = 0
                    gkLpState3   = 2
                    gkLpRecBpm3  = gkCommittedBpm
                    kLpTrimDone3 = 1
                    printks "LP3: TRIM -> PLAY (%d samples)\n", 0, gkLpLen3
                endif
            endif
            kLpTrimSteps3 -= 1
        od
    endif

    ; Claim the master-transport grid the moment this channel's length is
    ; actually decided — covers the deferred CLOCKED-early finalize (state
    ; 5->2 above) and the UNCLOCKED trim finalize (state 6->2 just above);
    ; a no-op wherever gkLpMasterLen was already set synchronously (immediate
    ; clocked close, or an existing master already claimed by another
    ; channel earlier this k-block).
    if (gkLpState3 == 2 && gkLpMasterLen == 0 && gkLpLen3 > 0) then
        gkLpMasterLen = gkLpLen3

        ; EXACTNESS FIX (2026-10, VARISPEED-WARBLE FIX) — see channel 1
        ; (instr 210) for the full rationale; identical pattern here.
        kLpClaimBpm3, kLpClaimN3 LoopImpliedBpm gkLpMasterLen
        gkCommittedBpm = kLpClaimBpm3
        gkLpRecBpm3  = kLpClaimBpm3
    endif

    aOutL = 0
    aOutR = 0
    if ((gkLpState3 == 2 || gkLpState3 == 3) && gkLpLen3 > 0) then
        kSpeed table gkLpSpIdx3, giLpSpeeds
        kLen   = gkLpLen3

        ; VARISPEED-TO-TEMPO (TASK 1, 2026-10) — composes with (does not
        ; replace) the manual speed control above. See channel 1 for the
        ; full rationale comment; identical pattern here.
        iLpVarispeedGlide = 0.35
        kLpTempoRateTgt3 = (gkLpRecBpm3 > 0 ? gkCommittedBpm / gkLpRecBpm3 : 1)
        ; NaN/inf/absurd-ratio guard (2026-11-02): bad ratio -> 1.0x
        kLpTempoRateTgt3 = (abs(kLpTempoRateTgt3) > 0.1 && abs(kLpTempoRateTgt3) < 10 ? kLpTempoRateTgt3 : 1)
        kLpTempoRate3    portk kLpTempoRateTgt3, iLpVarispeedGlide, 1
        kEffSpeed3 = kSpeed * kLpTempoRate3
        gkLpEffSpd3 = kEffSpeed3   ; read by BLOOPER (true playback rate incl. tempo trim)

        ; TRANSPORT LOCK — at exactly +1x the position is DERIVED from
        ; the clock every cycle (never accumulated): zero inter-channel
        ; drift, and returning from varispeed snaps back onto the grid
        ; exactly where the loop would have been. Only engages while the
        ; varispeed ratio is (within epsilon of) 1.0 too — see channel 1.
        ; ---- BEAT LOCK (2026-11-05) -------------------------------------------
        ; While PMOR's clock is live (gkLpBLock, midi_handler.orc) the read
        ; position is DERIVED from PMOR's beat counter gkLpB, never accumulated
        ; from a tempo ratio: pos = ((gkLpB - recB) mod lenBeats) * len/lenBeats.
        ; A tempo change then only changes the playback RATE (varispeed, as
        ; before) and the loop phase stays on PMOR's beat by construction.
        ; The old path integrated gkCommittedBpm/RecBpm (0.5 BPM deadband, 0.5 s
        ; dwell, 0.35 s glide) and the phase error of every tempo move was
        ; frozen in forever (anchor re-lock only works at exactly 1x).
        ; lenBeats: loop length in beats at the tempo of the moment the lock
        ; engages (quarter-beat snapped when within 0.5 %, else exact).
        if (gkLpBLock == 1 && gkLpSpIdx3 == 6) then
            if (gkLpBHave3 == 0) then
                kBLraw3 = kLen * gkLpBpmS / (60 * sr)
                ; BAR LOCK (2026-11-06): length snapped to a whole beat (min 1) and
                ; recB to an exact integer beat of the ABSOLUTE gkLpB, so sample 0
                ; sits on a real PMOR beat (and bar position, if lenB % 4 == 0).
                ; Sub-beat loops (< 1 beat: M/4, M/2 of a short master) keep the
                ; quarter-beat snap instead of being stretched to a whole beat.
                kBLq3   = round(kBLraw3 * 4) / 4
                gkLpLenB3 = (kBLraw3 >= 1 ? max(1, round(kBLraw3)) : (kBLq3 >= 0.25 && abs(kBLraw3 - kBLq3) < 0.005 * kBLq3 ? kBLq3 : kBLraw3))
                gkLpRecB3 = round(gkLpB - gkLpRdPos3 * gkLpLenB3 / kLen)
                gkLpBHave3 = 1
            endif
            kBLs3 = kLen / gkLpLenB3
            kBLx3 = gkLpB - gkLpRecB3
            kBLx3 = kBLx3 - gkLpLenB3 * floor(kBLx3 / gkLpLenB3)
            gkLpRdPos3 = kBLx3 * kBLs3
            kEffSpeed3 = gkLpBpmS / 60 * kBLs3 / sr
            gkLpEffSpd3 = kEffSpeed3
            gkLpBOn3 = 1
        elseif (gkLpBOn3 == 1) then
            ; lock lost: hand the position back to the clock-derived path in phase
            gkLpAnchor3 = gkLpClock - gkLpRdPos3
            gkLpBOn3 = 0
            gkLpBHave3 = 0
            gkLpLenB3 = 0
        endif
        if (gkLpSpIdx3 == 6 && abs(kLpTempoRate3 - 1) < 0.0005 && gkLpBOn3 == 0) then
            gkLpRdPos3 = (gkLpClock - gkLpAnchor3) % kLen
        endif

        aBaseR upsamp gkLpRdPos3
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed3
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf3L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf3R, 0, 0, 0

        ; NaN/inf safety net: zero this block, heal the poisoned stretch of the
        ; buffer, skip the overdub write. Recording is otherwise left intact.
        kLpBad3 NF_Chk aOutL, aOutR
        if (kLpBad3 == 1) then
            aOutL = 0
            aOutR = 0
            LP_Heal gkLpRdPos3, abs(kEffSpeed3) * ksmps + 4, kLen, giLpBuf3L, giLpBuf3R
        endif
        NF_Log "looper ch3 playback", kLpBad3

        if (gkLpState3 == 3 && kLpBad3 == 0) then
            ; sum clamped to +-8 (+18 dB): unity-feedback overdub can no longer run away
            tablew limit(aInL + aOutL, -8, 8), aRdPos, giLpBuf3L, 0, 0, 0
            tablew limit(aInR + aOutR, -8, 8), aRdPos, giLpBuf3R, 0, 0, 0
        endif

        gkLpRdPos3 += kEffSpeed3 * ksmps
        if (gkLpRdPos3 >= kLen) then
            gkLpRdPos3 -= kLen
        elseif (gkLpRdPos3 < 0) then
            gkLpRdPos3 += kLen
        endif
    else
        gkLpBOn3 = 0
        if (gkLpState3 != 4 || gkLpBLock == 0) then
            gkLpBHave3 = 0
            gkLpLenB3 = 0
        endif
    endif

    gaLpOutL += aOutL
    gaLpOutR += aOutR
endin

;--------------------------------------------------------------
; Channel 4
;--------------------------------------------------------------
instr 213
    aInL = gaLpInL
    aInR = gaLpInR
    iBufLen = ftlen(giLpBuf4L)

    ; per-sample block offset: shared gaLpSampOff, computed once in instr 209

    ; record-start quantization: armed press waits for PMOR's next whole beat
    ; (gkLpArmB4 = that beat in gkLpB); released at once if the lock is lost
    if (gkLpState4 == 1 && gkLpArmB4 > 0) then
        if (gkLpBLock == 0 || gkLpB >= gkLpArmB4) then
            gkLpArmB4   = 0
            gkLpWrPos4  = 0
            gkLpAnchor4 = gkLpClock
        endif
    endif
    if ((gkLpState4 == 1 && gkLpArmB4 == 0) || gkLpState4 == 5) then
        if (gkLpWrPos4 + ksmps <= iBufLen) then
            aBaseW upsamp gkLpWrPos4
            aWrIdx = aBaseW + gaLpSampOff
            tablew aInL, aWrIdx, giLpBuf4L, 0, 0, 0
            tablew aInR, aWrIdx, giLpBuf4R, 0, 0, 0
            gkLpWrPos4 += ksmps
            if (gkLpState4 == 5 && gkLpWrPos4 >= gkLpTarget4) then
                ; sync-close reached the quantize boundary
                gkLpLen4    = gkLpTarget4
                gkLpRdPos4  = gkLpWrPos4 - gkLpTarget4
                gkLpState4  = 2
                gkLpRecBpm4 = gkCommittedBpm
                printks "LP4: SYNCED -> PLAY\n", 0
            endif
        else
            gkLpLen4    = gkLpWrPos4
            gkLpState4  = 2
            gkLpRdPos4  = 0
            gkLpRecBpm4 = gkCommittedBpm
        endif
    endif

    ; -- TRIM-CLOSING (state 6): UNCLOCKED click-off. Bounded backward scan
    ; for the last sample above ~-60 dB (0.001 FS), spread across k-blocks
    ; (not all at once) so a long loop's scan never blows the real-time
    ; deadline in a single block. Start (position 0) is left untouched; if
    ; the whole tail scans as silence, fall back to the raw length
    ; (gkLpTarget4, stashed at click-off) rather than risk erasing audio.
    if (gkLpState4 == 6) then
        kLpTrimSteps4 = 8 * ksmps
        kLpTrimDone4  = 0
        while (kLpTrimSteps4 > 0 && kLpTrimDone4 == 0) do
            if (gkLpTrimPos4 <= 0) then
                gkLpLen4     = gkLpTarget4
                gkLpRdPos4   = 0
                gkLpState4   = 2
                gkLpRecBpm4  = gkCommittedBpm
                kLpTrimDone4 = 1
                printks "LP4: TRIM exhausted -> PLAY (raw kept)\n", 0
            else
                gkLpTrimPos4 -= 1
                kLpTrimSL4 table gkLpTrimPos4, giLpBuf4L, 0, 0, 0
                kLpTrimSR4 table gkLpTrimPos4, giLpBuf4R, 0, 0, 0
                if (abs(kLpTrimSL4) > 0.001 || abs(kLpTrimSR4) > 0.001) then
                    gkLpLen4     = gkLpTrimPos4 + 1
                    gkLpRdPos4   = 0
                    gkLpState4   = 2
                    gkLpRecBpm4  = gkCommittedBpm
                    kLpTrimDone4 = 1
                    printks "LP4: TRIM -> PLAY (%d samples)\n", 0, gkLpLen4
                endif
            endif
            kLpTrimSteps4 -= 1
        od
    endif

    ; Claim the master-transport grid the moment this channel's length is
    ; actually decided — covers the deferred CLOCKED-early finalize (state
    ; 5->2 above) and the UNCLOCKED trim finalize (state 6->2 just above);
    ; a no-op wherever gkLpMasterLen was already set synchronously (immediate
    ; clocked close, or an existing master already claimed by another
    ; channel earlier this k-block).
    if (gkLpState4 == 2 && gkLpMasterLen == 0 && gkLpLen4 > 0) then
        gkLpMasterLen = gkLpLen4

        ; EXACTNESS FIX (2026-10, VARISPEED-WARBLE FIX) — see channel 1
        ; (instr 210) for the full rationale; identical pattern here.
        kLpClaimBpm4, kLpClaimN4 LoopImpliedBpm gkLpMasterLen
        gkCommittedBpm = kLpClaimBpm4
        gkLpRecBpm4  = kLpClaimBpm4
    endif

    aOutL = 0
    aOutR = 0
    if ((gkLpState4 == 2 || gkLpState4 == 3) && gkLpLen4 > 0) then
        kSpeed table gkLpSpIdx4, giLpSpeeds
        kLen   = gkLpLen4

        ; VARISPEED-TO-TEMPO (TASK 1, 2026-10) — composes with (does not
        ; replace) the manual speed control above. See channel 1 for the
        ; full rationale comment; identical pattern here.
        iLpVarispeedGlide = 0.35
        kLpTempoRateTgt4 = (gkLpRecBpm4 > 0 ? gkCommittedBpm / gkLpRecBpm4 : 1)
        ; NaN/inf/absurd-ratio guard (2026-11-02): bad ratio -> 1.0x
        kLpTempoRateTgt4 = (abs(kLpTempoRateTgt4) > 0.1 && abs(kLpTempoRateTgt4) < 10 ? kLpTempoRateTgt4 : 1)
        kLpTempoRate4    portk kLpTempoRateTgt4, iLpVarispeedGlide, 1
        kEffSpeed4 = kSpeed * kLpTempoRate4
        gkLpEffSpd4 = kEffSpeed4   ; read by BLOOPER (true playback rate incl. tempo trim)

        ; TRANSPORT LOCK — at exactly +1x the position is DERIVED from
        ; the clock every cycle (never accumulated): zero inter-channel
        ; drift, and returning from varispeed snaps back onto the grid
        ; exactly where the loop would have been. Only engages while the
        ; varispeed ratio is (within epsilon of) 1.0 too — see channel 1.
        ; ---- BEAT LOCK (2026-11-05) -------------------------------------------
        ; While PMOR's clock is live (gkLpBLock, midi_handler.orc) the read
        ; position is DERIVED from PMOR's beat counter gkLpB, never accumulated
        ; from a tempo ratio: pos = ((gkLpB - recB) mod lenBeats) * len/lenBeats.
        ; A tempo change then only changes the playback RATE (varispeed, as
        ; before) and the loop phase stays on PMOR's beat by construction.
        ; The old path integrated gkCommittedBpm/RecBpm (0.5 BPM deadband, 0.5 s
        ; dwell, 0.35 s glide) and the phase error of every tempo move was
        ; frozen in forever (anchor re-lock only works at exactly 1x).
        ; lenBeats: loop length in beats at the tempo of the moment the lock
        ; engages (quarter-beat snapped when within 0.5 %, else exact).
        if (gkLpBLock == 1 && gkLpSpIdx4 == 6) then
            if (gkLpBHave4 == 0) then
                kBLraw4 = kLen * gkLpBpmS / (60 * sr)
                ; BAR LOCK (2026-11-06): length snapped to a whole beat (min 1) and
                ; recB to an exact integer beat of the ABSOLUTE gkLpB, so sample 0
                ; sits on a real PMOR beat (and bar position, if lenB % 4 == 0).
                ; Sub-beat loops (< 1 beat: M/4, M/2 of a short master) keep the
                ; quarter-beat snap instead of being stretched to a whole beat.
                kBLq4   = round(kBLraw4 * 4) / 4
                gkLpLenB4 = (kBLraw4 >= 1 ? max(1, round(kBLraw4)) : (kBLq4 >= 0.25 && abs(kBLraw4 - kBLq4) < 0.005 * kBLq4 ? kBLq4 : kBLraw4))
                gkLpRecB4 = round(gkLpB - gkLpRdPos4 * gkLpLenB4 / kLen)
                gkLpBHave4 = 1
            endif
            kBLs4 = kLen / gkLpLenB4
            kBLx4 = gkLpB - gkLpRecB4
            kBLx4 = kBLx4 - gkLpLenB4 * floor(kBLx4 / gkLpLenB4)
            gkLpRdPos4 = kBLx4 * kBLs4
            kEffSpeed4 = gkLpBpmS / 60 * kBLs4 / sr
            gkLpEffSpd4 = kEffSpeed4
            gkLpBOn4 = 1
        elseif (gkLpBOn4 == 1) then
            ; lock lost: hand the position back to the clock-derived path in phase
            gkLpAnchor4 = gkLpClock - gkLpRdPos4
            gkLpBOn4 = 0
            gkLpBHave4 = 0
            gkLpLenB4 = 0
        endif
        if (gkLpSpIdx4 == 6 && abs(kLpTempoRate4 - 1) < 0.0005 && gkLpBOn4 == 0) then
            gkLpRdPos4 = (gkLpClock - gkLpAnchor4) % kLen
        endif

        aBaseR upsamp gkLpRdPos4
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed4
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf4L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf4R, 0, 0, 0

        ; NaN/inf safety net: zero this block, heal the poisoned stretch of the
        ; buffer, skip the overdub write. Recording is otherwise left intact.
        kLpBad4 NF_Chk aOutL, aOutR
        if (kLpBad4 == 1) then
            aOutL = 0
            aOutR = 0
            LP_Heal gkLpRdPos4, abs(kEffSpeed4) * ksmps + 4, kLen, giLpBuf4L, giLpBuf4R
        endif
        NF_Log "looper ch4 playback", kLpBad4

        if (gkLpState4 == 3 && kLpBad4 == 0) then
            ; sum clamped to +-8 (+18 dB): unity-feedback overdub can no longer run away
            tablew limit(aInL + aOutL, -8, 8), aRdPos, giLpBuf4L, 0, 0, 0
            tablew limit(aInR + aOutR, -8, 8), aRdPos, giLpBuf4R, 0, 0, 0
        endif

        gkLpRdPos4 += kEffSpeed4 * ksmps
        if (gkLpRdPos4 >= kLen) then
            gkLpRdPos4 -= kLen
        elseif (gkLpRdPos4 < 0) then
            gkLpRdPos4 += kLen
        endif
    else
        gkLpBOn4 = 0
        if (gkLpState4 != 4 || gkLpBLock == 0) then
            gkLpBHave4 = 0
            gkLpLenB4 = 0
        endif
    endif

    gaLpOutL += aOutL
    gaLpOutR += aOutR
endin

;--------------------------------------------------------------
; Channel 5
;--------------------------------------------------------------
instr 214
    aInL = gaLpInL
    aInR = gaLpInR
    iBufLen = ftlen(giLpBuf5L)

    ; per-sample block offset: shared gaLpSampOff, computed once in instr 209

    ; record-start quantization: armed press waits for PMOR's next whole beat
    ; (gkLpArmB5 = that beat in gkLpB); released at once if the lock is lost
    if (gkLpState5 == 1 && gkLpArmB5 > 0) then
        if (gkLpBLock == 0 || gkLpB >= gkLpArmB5) then
            gkLpArmB5   = 0
            gkLpWrPos5  = 0
            gkLpAnchor5 = gkLpClock
        endif
    endif
    if ((gkLpState5 == 1 && gkLpArmB5 == 0) || gkLpState5 == 5) then
        if (gkLpWrPos5 + ksmps <= iBufLen) then
            aBaseW upsamp gkLpWrPos5
            aWrIdx = aBaseW + gaLpSampOff
            tablew aInL, aWrIdx, giLpBuf5L, 0, 0, 0
            tablew aInR, aWrIdx, giLpBuf5R, 0, 0, 0
            gkLpWrPos5 += ksmps
            if (gkLpState5 == 5 && gkLpWrPos5 >= gkLpTarget5) then
                ; sync-close reached the quantize boundary
                gkLpLen5    = gkLpTarget5
                gkLpRdPos5  = gkLpWrPos5 - gkLpTarget5
                gkLpState5  = 2
                gkLpRecBpm5 = gkCommittedBpm
                printks "LP5: SYNCED -> PLAY\n", 0
            endif
        else
            gkLpLen5    = gkLpWrPos5
            gkLpState5  = 2
            gkLpRdPos5  = 0
            gkLpRecBpm5 = gkCommittedBpm
        endif
    endif

    ; -- TRIM-CLOSING (state 6): UNCLOCKED click-off. Bounded backward scan
    ; for the last sample above ~-60 dB (0.001 FS), spread across k-blocks
    ; (not all at once) so a long loop's scan never blows the real-time
    ; deadline in a single block. Start (position 0) is left untouched; if
    ; the whole tail scans as silence, fall back to the raw length
    ; (gkLpTarget5, stashed at click-off) rather than risk erasing audio.
    if (gkLpState5 == 6) then
        kLpTrimSteps5 = 8 * ksmps
        kLpTrimDone5  = 0
        while (kLpTrimSteps5 > 0 && kLpTrimDone5 == 0) do
            if (gkLpTrimPos5 <= 0) then
                gkLpLen5     = gkLpTarget5
                gkLpRdPos5   = 0
                gkLpState5   = 2
                gkLpRecBpm5  = gkCommittedBpm
                kLpTrimDone5 = 1
                printks "LP5: TRIM exhausted -> PLAY (raw kept)\n", 0
            else
                gkLpTrimPos5 -= 1
                kLpTrimSL5 table gkLpTrimPos5, giLpBuf5L, 0, 0, 0
                kLpTrimSR5 table gkLpTrimPos5, giLpBuf5R, 0, 0, 0
                if (abs(kLpTrimSL5) > 0.001 || abs(kLpTrimSR5) > 0.001) then
                    gkLpLen5     = gkLpTrimPos5 + 1
                    gkLpRdPos5   = 0
                    gkLpState5   = 2
                    gkLpRecBpm5  = gkCommittedBpm
                    kLpTrimDone5 = 1
                    printks "LP5: TRIM -> PLAY (%d samples)\n", 0, gkLpLen5
                endif
            endif
            kLpTrimSteps5 -= 1
        od
    endif

    ; Claim the master-transport grid the moment this channel's length is
    ; actually decided — covers the deferred CLOCKED-early finalize (state
    ; 5->2 above) and the UNCLOCKED trim finalize (state 6->2 just above);
    ; a no-op wherever gkLpMasterLen was already set synchronously (immediate
    ; clocked close, or an existing master already claimed by another
    ; channel earlier this k-block).
    if (gkLpState5 == 2 && gkLpMasterLen == 0 && gkLpLen5 > 0) then
        gkLpMasterLen = gkLpLen5

        ; EXACTNESS FIX (2026-10, VARISPEED-WARBLE FIX) — see channel 1
        ; (instr 210) for the full rationale; identical pattern here.
        kLpClaimBpm5, kLpClaimN5 LoopImpliedBpm gkLpMasterLen
        gkCommittedBpm = kLpClaimBpm5
        gkLpRecBpm5  = kLpClaimBpm5
    endif

    aOutL = 0
    aOutR = 0
    if ((gkLpState5 == 2 || gkLpState5 == 3) && gkLpLen5 > 0) then
        kSpeed table gkLpSpIdx5, giLpSpeeds
        kLen   = gkLpLen5

        ; VARISPEED-TO-TEMPO (TASK 1, 2026-10) — composes with (does not
        ; replace) the manual speed control above. See channel 1 for the
        ; full rationale comment; identical pattern here.
        iLpVarispeedGlide = 0.35
        kLpTempoRateTgt5 = (gkLpRecBpm5 > 0 ? gkCommittedBpm / gkLpRecBpm5 : 1)
        ; NaN/inf/absurd-ratio guard (2026-11-02): bad ratio -> 1.0x
        kLpTempoRateTgt5 = (abs(kLpTempoRateTgt5) > 0.1 && abs(kLpTempoRateTgt5) < 10 ? kLpTempoRateTgt5 : 1)
        kLpTempoRate5    portk kLpTempoRateTgt5, iLpVarispeedGlide, 1
        kEffSpeed5 = kSpeed * kLpTempoRate5
        gkLpEffSpd5 = kEffSpeed5   ; read by BLOOPER (true playback rate incl. tempo trim)

        ; TRANSPORT LOCK — at exactly +1x the position is DERIVED from
        ; the clock every cycle (never accumulated): zero inter-channel
        ; drift, and returning from varispeed snaps back onto the grid
        ; exactly where the loop would have been. Only engages while the
        ; varispeed ratio is (within epsilon of) 1.0 too — see channel 1.
        ; ---- BEAT LOCK (2026-11-05) -------------------------------------------
        ; While PMOR's clock is live (gkLpBLock, midi_handler.orc) the read
        ; position is DERIVED from PMOR's beat counter gkLpB, never accumulated
        ; from a tempo ratio: pos = ((gkLpB - recB) mod lenBeats) * len/lenBeats.
        ; A tempo change then only changes the playback RATE (varispeed, as
        ; before) and the loop phase stays on PMOR's beat by construction.
        ; The old path integrated gkCommittedBpm/RecBpm (0.5 BPM deadband, 0.5 s
        ; dwell, 0.35 s glide) and the phase error of every tempo move was
        ; frozen in forever (anchor re-lock only works at exactly 1x).
        ; lenBeats: loop length in beats at the tempo of the moment the lock
        ; engages (quarter-beat snapped when within 0.5 %, else exact).
        if (gkLpBLock == 1 && gkLpSpIdx5 == 6) then
            if (gkLpBHave5 == 0) then
                kBLraw5 = kLen * gkLpBpmS / (60 * sr)
                ; BAR LOCK (2026-11-06): length snapped to a whole beat (min 1) and
                ; recB to an exact integer beat of the ABSOLUTE gkLpB, so sample 0
                ; sits on a real PMOR beat (and bar position, if lenB % 4 == 0).
                ; Sub-beat loops (< 1 beat: M/4, M/2 of a short master) keep the
                ; quarter-beat snap instead of being stretched to a whole beat.
                kBLq5   = round(kBLraw5 * 4) / 4
                gkLpLenB5 = (kBLraw5 >= 1 ? max(1, round(kBLraw5)) : (kBLq5 >= 0.25 && abs(kBLraw5 - kBLq5) < 0.005 * kBLq5 ? kBLq5 : kBLraw5))
                gkLpRecB5 = round(gkLpB - gkLpRdPos5 * gkLpLenB5 / kLen)
                gkLpBHave5 = 1
            endif
            kBLs5 = kLen / gkLpLenB5
            kBLx5 = gkLpB - gkLpRecB5
            kBLx5 = kBLx5 - gkLpLenB5 * floor(kBLx5 / gkLpLenB5)
            gkLpRdPos5 = kBLx5 * kBLs5
            kEffSpeed5 = gkLpBpmS / 60 * kBLs5 / sr
            gkLpEffSpd5 = kEffSpeed5
            gkLpBOn5 = 1
        elseif (gkLpBOn5 == 1) then
            ; lock lost: hand the position back to the clock-derived path in phase
            gkLpAnchor5 = gkLpClock - gkLpRdPos5
            gkLpBOn5 = 0
            gkLpBHave5 = 0
            gkLpLenB5 = 0
        endif
        if (gkLpSpIdx5 == 6 && abs(kLpTempoRate5 - 1) < 0.0005 && gkLpBOn5 == 0) then
            gkLpRdPos5 = (gkLpClock - gkLpAnchor5) % kLen
        endif

        aBaseR upsamp gkLpRdPos5
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed5
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf5L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf5R, 0, 0, 0

        ; NaN/inf safety net: zero this block, heal the poisoned stretch of the
        ; buffer, skip the overdub write. Recording is otherwise left intact.
        kLpBad5 NF_Chk aOutL, aOutR
        if (kLpBad5 == 1) then
            aOutL = 0
            aOutR = 0
            LP_Heal gkLpRdPos5, abs(kEffSpeed5) * ksmps + 4, kLen, giLpBuf5L, giLpBuf5R
        endif
        NF_Log "looper ch5 playback", kLpBad5

        if (gkLpState5 == 3 && kLpBad5 == 0) then
            ; sum clamped to +-8 (+18 dB): unity-feedback overdub can no longer run away
            tablew limit(aInL + aOutL, -8, 8), aRdPos, giLpBuf5L, 0, 0, 0
            tablew limit(aInR + aOutR, -8, 8), aRdPos, giLpBuf5R, 0, 0, 0
        endif

        gkLpRdPos5 += kEffSpeed5 * ksmps
        if (gkLpRdPos5 >= kLen) then
            gkLpRdPos5 -= kLen
        elseif (gkLpRdPos5 < 0) then
            gkLpRdPos5 += kLen
        endif
    else
        gkLpBOn5 = 0
        if (gkLpState5 != 4 || gkLpBLock == 0) then
            gkLpBHave5 = 0
            gkLpLenB5 = 0
        endif
    endif

    gaLpOutL += aOutL
    gaLpOutR += aOutR
endin
