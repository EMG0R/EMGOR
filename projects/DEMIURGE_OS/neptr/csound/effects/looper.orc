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
    gkLpGridAvail = (gkGLITCH_extBpmSeen > 0.5 || gkLpMasterLen > 0) ? 1 : 0

    ; Whole-beat length in samples from the arbitrated master tempo
    ; (gkGLITCH_bpm — external CC119 or loop-derived, see
    ; chase_bliss_clock.orc instr 100). Only meaningful while
    ; gkLpGridAvail == 1; instr 209 runs before 210-214 (lower instr number,
    ; ascending execution order) so every channel sees this block's value.
    gkLpBeatSamples = sr * 60 / gkGLITCH_bpm
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
    if (gkLpState1 == 1 || gkLpState1 == 5) then
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
        kLpTempoRate1    portk kLpTempoRateTgt1, iLpVarispeedGlide, 1
        kEffSpeed1 = kSpeed * kLpTempoRate1

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
        if (gkLpSpIdx1 == 6 && abs(kLpTempoRate1 - 1) < 0.0005) then
            gkLpRdPos1 = (gkLpClock - gkLpAnchor1) % kLen
        endif

        ; A-rate read from current position
        aBaseR upsamp gkLpRdPos1
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed1
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf1L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf1R, 0, 0, 0

        ; Overdub: mix input into buffer at same position
        if (gkLpState1 == 3) then
            tablew aInL + aOutL, aRdPos, giLpBuf1L, 0, 0, 0
            tablew aInR + aOutR, aRdPos, giLpBuf1R, 0, 0, 0
        endif

        ; Advance k-rate read position
        gkLpRdPos1 += kEffSpeed1 * ksmps
        if (gkLpRdPos1 >= kLen) then
            gkLpRdPos1 -= kLen
        elseif (gkLpRdPos1 < 0) then
            gkLpRdPos1 += kLen
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

    if (gkLpState2 == 1 || gkLpState2 == 5) then
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
        kLpTempoRate2    portk kLpTempoRateTgt2, iLpVarispeedGlide, 1
        kEffSpeed2 = kSpeed * kLpTempoRate2

        ; TRANSPORT LOCK — at exactly +1x the position is DERIVED from
        ; the clock every cycle (never accumulated): zero inter-channel
        ; drift, and returning from varispeed snaps back onto the grid
        ; exactly where the loop would have been. Only engages while the
        ; varispeed ratio is (within epsilon of) 1.0 too — see channel 1.
        if (gkLpSpIdx2 == 6 && abs(kLpTempoRate2 - 1) < 0.0005) then
            gkLpRdPos2 = (gkLpClock - gkLpAnchor2) % kLen
        endif

        aBaseR upsamp gkLpRdPos2
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed2
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf2L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf2R, 0, 0, 0

        if (gkLpState2 == 3) then
            tablew aInL + aOutL, aRdPos, giLpBuf2L, 0, 0, 0
            tablew aInR + aOutR, aRdPos, giLpBuf2R, 0, 0, 0
        endif

        gkLpRdPos2 += kEffSpeed2 * ksmps
        if (gkLpRdPos2 >= kLen) then
            gkLpRdPos2 -= kLen
        elseif (gkLpRdPos2 < 0) then
            gkLpRdPos2 += kLen
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

    if (gkLpState3 == 1 || gkLpState3 == 5) then
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
        kLpTempoRate3    portk kLpTempoRateTgt3, iLpVarispeedGlide, 1
        kEffSpeed3 = kSpeed * kLpTempoRate3

        ; TRANSPORT LOCK — at exactly +1x the position is DERIVED from
        ; the clock every cycle (never accumulated): zero inter-channel
        ; drift, and returning from varispeed snaps back onto the grid
        ; exactly where the loop would have been. Only engages while the
        ; varispeed ratio is (within epsilon of) 1.0 too — see channel 1.
        if (gkLpSpIdx3 == 6 && abs(kLpTempoRate3 - 1) < 0.0005) then
            gkLpRdPos3 = (gkLpClock - gkLpAnchor3) % kLen
        endif

        aBaseR upsamp gkLpRdPos3
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed3
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf3L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf3R, 0, 0, 0

        if (gkLpState3 == 3) then
            tablew aInL + aOutL, aRdPos, giLpBuf3L, 0, 0, 0
            tablew aInR + aOutR, aRdPos, giLpBuf3R, 0, 0, 0
        endif

        gkLpRdPos3 += kEffSpeed3 * ksmps
        if (gkLpRdPos3 >= kLen) then
            gkLpRdPos3 -= kLen
        elseif (gkLpRdPos3 < 0) then
            gkLpRdPos3 += kLen
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

    if (gkLpState4 == 1 || gkLpState4 == 5) then
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
        kLpTempoRate4    portk kLpTempoRateTgt4, iLpVarispeedGlide, 1
        kEffSpeed4 = kSpeed * kLpTempoRate4

        ; TRANSPORT LOCK — at exactly +1x the position is DERIVED from
        ; the clock every cycle (never accumulated): zero inter-channel
        ; drift, and returning from varispeed snaps back onto the grid
        ; exactly where the loop would have been. Only engages while the
        ; varispeed ratio is (within epsilon of) 1.0 too — see channel 1.
        if (gkLpSpIdx4 == 6 && abs(kLpTempoRate4 - 1) < 0.0005) then
            gkLpRdPos4 = (gkLpClock - gkLpAnchor4) % kLen
        endif

        aBaseR upsamp gkLpRdPos4
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed4
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf4L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf4R, 0, 0, 0

        if (gkLpState4 == 3) then
            tablew aInL + aOutL, aRdPos, giLpBuf4L, 0, 0, 0
            tablew aInR + aOutR, aRdPos, giLpBuf4R, 0, 0, 0
        endif

        gkLpRdPos4 += kEffSpeed4 * ksmps
        if (gkLpRdPos4 >= kLen) then
            gkLpRdPos4 -= kLen
        elseif (gkLpRdPos4 < 0) then
            gkLpRdPos4 += kLen
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

    if (gkLpState5 == 1 || gkLpState5 == 5) then
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
        kLpTempoRate5    portk kLpTempoRateTgt5, iLpVarispeedGlide, 1
        kEffSpeed5 = kSpeed * kLpTempoRate5

        ; TRANSPORT LOCK — at exactly +1x the position is DERIVED from
        ; the clock every cycle (never accumulated): zero inter-channel
        ; drift, and returning from varispeed snaps back onto the grid
        ; exactly where the loop would have been. Only engages while the
        ; varispeed ratio is (within epsilon of) 1.0 too — see channel 1.
        if (gkLpSpIdx5 == 6 && abs(kLpTempoRate5 - 1) < 0.0005) then
            gkLpRdPos5 = (gkLpClock - gkLpAnchor5) % kLen
        endif

        aBaseR upsamp gkLpRdPos5
        aRdPos  = aBaseR + gaLpSampOff * kEffSpeed5
        aRdPos  wrap aRdPos, 0, kLen
        aOutL   tablei aRdPos, giLpBuf5L, 0, 0, 0
        aOutR   tablei aRdPos, giLpBuf5R, 0, 0, 0

        if (gkLpState5 == 3) then
            tablew aInL + aOutL, aRdPos, giLpBuf5L, 0, 0, 0
            tablew aInR + aOutR, aRdPos, giLpBuf5R, 0, 0, 0
        endif

        gkLpRdPos5 += kEffSpeed5 * ksmps
        if (gkLpRdPos5 >= kLen) then
            gkLpRdPos5 -= kLen
        elseif (gkLpRdPos5 < 0) then
            gkLpRdPos5 += kLen
        endif
    endif

    gaLpOutL += aOutL
    gaLpOutR += aOutR
endin
