; 2026-07-29 Pocket OpGorator integration (spec §4) — grid-true capture.
; Click-off (any channel's REC->tap) used to bake the human press-timing
; error straight into the loop: SET_LENGTH captured the raw write position
; unconditionally. Now:
;   - CLOCKED (gkLpGridAvail==1, see looper.orc instr 209): the FIRST loop
;     closed anywhere (still the one that defines gkLpMasterLen) quantizes
;     to the nearest whole-beat multiple of gkLpBeatSamples (derived from
;     gkGLITCH_bpm — external CC119 or loop-derived, whichever is
;     authoritative). Early click-off (raw < the nearest boundary) defers to
;     state 5 so the tail is never cut; late/exact click-off rounds back
;     immediately, same as the existing ch2-5 candidate logic below (which
;     is untouched — it already snaps to nearest-pow2-of-master and already
;     defers early closes via state 5, and since the master is now itself a
;     whole-beat multiple, its pow2 fractions/multiples stay grid-consistent).
;   - UNCLOCKED (no grid yet — no external clock has ever been seen AND no
;     loop-derived master exists): free capture, but trim trailing silence
;     instead of baking it in. Deferred to state 6 (see looper.orc), which
;     scans backward from the write head in bounded per-k-block chunks for
;     the last sample above ~-60 dB (0.001 FS); if nothing is found (the
;     whole take is silence) it falls back to the raw length rather than
;     risk erasing real audio. Start (position 0) is never touched.
; $gkRecBpm (TASK 1, 2026-10): stamped with the AUTHORITATIVE tempo
; (gkGLITCH_bpm) at every point this macro closes a loop IMMEDIATELY (state
; -> 2 synchronously, here). The two paths that instead DEFER to state 5
; (sync-close) do NOT stamp here — looper.orc's own state-5 finalize stamps
; it at the moment the deferred close actually lands, which is the correct
; "instant the length became authoritative" moment for those. See
; globals.orc's gkLpRecBpm declaration for why this can never feed back into
; the tempo derivation (it mirrors the gkLpMasterLen capture-once pattern).
#define LP_QCLOSE(kRaw'gkLen'gkState'gkRdPos'gkTgt'iBufN'gkTrimPos'gkRecBpm)#
  if (gkLpMasterLen == 0) then
    ; First loop closed anywhere = master transport
    if (gkLpGridAvail == 1 && gkLpBeatSamples > 0) then
      ; CLOCKED — quantize to the nearest whole-beat grid line.
      kBeats_$gkLen = round($kRaw / gkLpBeatSamples)
      if (kBeats_$gkLen < 1) then
        kBeats_$gkLen = 1
      endif
      kTgt_$gkLen   = kBeats_$gkLen * gkLpBeatSamples
      kBufSz_$gkLen = ftlen($iBufN)
      if (kTgt_$gkLen > kBufSz_$gkLen) then
        kTgt_$gkLen = kBufSz_$gkLen
      endif
      if ($kRaw >= kTgt_$gkLen) then
        ; late (or exact) — the grid line has already been reached; round
        ; back and wrap any overshoot into the read phase.
        $gkLen        = kTgt_$gkLen
        gkLpMasterLen = kTgt_$gkLen
        $gkState      = 2
        $gkRdPos      = $kRaw % kTgt_$gkLen
        $gkRecBpm     = gkGLITCH_bpm
      else
        ; early — keep recording until the boundary is actually reached,
        ; then finalize (looper.orc instr sets gkLpMasterLen once state
        ; flips to 2, since this channel is the master-definer).
        $gkTgt   = kTgt_$gkLen
        $gkState = 5
      endif
    else
      ; UNCLOCKED — free capture, trim trailing silence on the way out.
      $gkTrimPos = $kRaw
      $gkTgt     = $kRaw   ; fallback length if the whole tail is silence
      $gkState   = 6
    endif
  else
    ; Snap to nearest of {M/4, M/2, M, 2M, 4M}
    kM_$gkLen  = gkLpMasterLen
    kC0_$gkLen = kM_$gkLen / 4
    kC1_$gkLen = kM_$gkLen / 2
    kC2_$gkLen = kM_$gkLen
    kC3_$gkLen = kM_$gkLen * 2
    kC4_$gkLen = kM_$gkLen * 4
    kBst_$gkLen = kC0_$gkLen
    kBsD_$gkLen = abs($kRaw - kC0_$gkLen)
    if (abs($kRaw - kC1_$gkLen) < kBsD_$gkLen) then
      kBst_$gkLen = kC1_$gkLen
      kBsD_$gkLen = abs($kRaw - kC1_$gkLen)
    endif
    if (abs($kRaw - kC2_$gkLen) < kBsD_$gkLen) then
      kBst_$gkLen = kC2_$gkLen
      kBsD_$gkLen = abs($kRaw - kC2_$gkLen)
    endif
    if (abs($kRaw - kC3_$gkLen) < kBsD_$gkLen) then
      kBst_$gkLen = kC3_$gkLen
      kBsD_$gkLen = abs($kRaw - kC3_$gkLen)
    endif
    if (abs($kRaw - kC4_$gkLen) < kBsD_$gkLen) then
      kBst_$gkLen = kC4_$gkLen
    endif
    kBufSz_$gkLen = ftlen($iBufN)
    if (kBst_$gkLen > kBufSz_$gkLen) then
      kBst_$gkLen = kBufSz_$gkLen
    endif
    if ($kRaw >= kBst_$gkLen) then
      ; Closed at/after the boundary: wrap the overshoot so playback
      ; lands exactly where the player's timing put it.
      $gkLen    = kBst_$gkLen
      $gkState  = 2
      $gkRdPos  = $kRaw % kBst_$gkLen
      $gkRecBpm = gkGLITCH_bpm
    else
      ; Closed early: keep recording until the boundary (state 5),
      ; then looper.orc flips to play, phase-aligned.
      $gkTgt   = kBst_$gkLen
      $gkState = 5
    endif
  endif
#

;============ LOOPER GESTURE MACROS — each body is expanded VERBATIM at its $LP_* call site (handler_menus.orc / midi_handler.orc) ============
; MENU INDEX: the looper lives on menu 12 since the 2026-07-15 GLITCH reorder (was 13, before that 10;
; tail order is now LOOPER=12, CHASE BLISS=13, GLITCH=14, NAM=15, IR VERB=16). The ten
; `gkCurrentMenu == 12` checks below are the ONLY index-coupled lines in this
; file — if the LOOPER menu ever moves again, these ten literals must move with
; it (grep this file for gkCurrentMenu).

; LOOPER ch1 press gesture (menu 14, CC74) — moved verbatim from midi_handler.orc
#define LP_CH1_PRESS #
      elseif (gkCurrentMenu == 14) then
        ; LOOPER ch1 — press-fired for tight timing (rec/play/resume/clear
        ; all land on finger-down; release only classifies tap-vs-hold)
        kLpNow74 times
        if (kLpNow74 - gkLpLastTap1 < 0.5) then
          gkLpDblFired1 = 1
          gkLpState1  = 0
          gkLpLen1    = 0
          gkLpWrPos1  = 0
          gkLpRdPos1  = 0
          gkLpSpIdx1  = 6
          if (gkDrumChan == 1) then
            gkDrumChan = -1
            gkDrumIdx  = -1
          endif
          if (gkLpLen2 == 0 && gkLpLen3 == 0 && gkLpLen4 == 0 && gkLpLen5 == 0) then
            gkLpMasterLen = 0
          endif
          printks "LP1: CLEAR\n", 0
        elseif (gkLpState1 == 0) then
          gkLpState1  = 1
          gkLpWrPos1  = 0
          gkLpRdPos1  = 0
          gkLpSpIdx1  = 6
          gkLpAnchor1 = gkLpClock
          gkLpConsumed1 = 1
          printks "LP1: REC\n", 0
        elseif (gkLpState1 == 1) then
          $LP_QCLOSE(gkLpWrPos1'gkLpLen1'gkLpState1'gkLpRdPos1'gkLpTarget1'giLpBuf1L'gkLpTrimPos1'gkLpRecBpm1)
          gkLpConsumed1 = 1
          printks "LP1: CLOSE\n", 0
        elseif (gkLpState1 == 4) then
          gkLpState1 = 2
          ; relock: re-enter exactly in time with the transport
          gkLpRdPos1 = (gkLpClock - gkLpAnchor1) % gkLpLen1
          gkLpConsumed1 = 1
          printks "LP1: RESUME\n", 0
        endif
        gkLpLastTap1 = kLpNow74
        gkLpPressT1  = kLpNow74
#

; LOOPER ch2 press gesture (menu 14, CC75) — moved verbatim from midi_handler.orc
#define LP_CH2_PRESS #
      elseif (gkCurrentMenu == 14) then
        ; LOOPER ch2 — press-fired for tight timing (rec/play/resume/clear
        ; all land on finger-down; release only classifies tap-vs-hold)
        kLpNow75 times
        if (kLpNow75 - gkLpLastTap2 < 0.5) then
          gkLpDblFired2 = 1
          gkLpState2  = 0
          gkLpLen2    = 0
          gkLpWrPos2  = 0
          gkLpRdPos2  = 0
          gkLpSpIdx2  = 6
          if (gkDrumChan == 2) then
            gkDrumChan = -1
            gkDrumIdx  = -1
          endif
          if (gkLpLen1 == 0 && gkLpLen3 == 0 && gkLpLen4 == 0 && gkLpLen5 == 0) then
            gkLpMasterLen = 0
          endif
          printks "LP2: CLEAR\n", 0
        elseif (gkLpState2 == 0) then
          gkLpState2  = 1
          gkLpWrPos2  = 0
          gkLpRdPos2  = 0
          gkLpSpIdx2  = 6
          gkLpAnchor2 = gkLpClock
          gkLpConsumed2 = 1
          printks "LP2: REC\n", 0
        elseif (gkLpState2 == 1) then
          $LP_QCLOSE(gkLpWrPos2'gkLpLen2'gkLpState2'gkLpRdPos2'gkLpTarget2'giLpBuf2L'gkLpTrimPos2'gkLpRecBpm2)
          gkLpConsumed2 = 1
          printks "LP2: CLOSE\n", 0
        elseif (gkLpState2 == 4) then
          gkLpState2 = 2
          ; relock: re-enter exactly in time with the transport
          gkLpRdPos2 = (gkLpClock - gkLpAnchor2) % gkLpLen2
          gkLpConsumed2 = 1
          printks "LP2: RESUME\n", 0
        endif
        gkLpLastTap2 = kLpNow75
        gkLpPressT2  = kLpNow75
#

; LOOPER ch3 press gesture (menu 14, CC76) — moved verbatim from midi_handler.orc
#define LP_CH3_PRESS #
      elseif (gkCurrentMenu == 14) then
        ; LOOPER ch3 — press-fired for tight timing (rec/play/resume/clear
        ; all land on finger-down; release only classifies tap-vs-hold)
        kLpNow76 times
        if (kLpNow76 - gkLpLastTap3 < 0.5) then
          gkLpDblFired3 = 1
          gkLpState3  = 0
          gkLpLen3    = 0
          gkLpWrPos3  = 0
          gkLpRdPos3  = 0
          gkLpSpIdx3  = 6
          if (gkDrumChan == 3) then
            gkDrumChan = -1
            gkDrumIdx  = -1
          endif
          if (gkLpLen1 == 0 && gkLpLen2 == 0 && gkLpLen4 == 0 && gkLpLen5 == 0) then
            gkLpMasterLen = 0
          endif
          printks "LP3: CLEAR\n", 0
        elseif (gkLpState3 == 0) then
          gkLpState3  = 1
          gkLpWrPos3  = 0
          gkLpRdPos3  = 0
          gkLpSpIdx3  = 6
          gkLpAnchor3 = gkLpClock
          gkLpConsumed3 = 1
          printks "LP3: REC\n", 0
        elseif (gkLpState3 == 1) then
          $LP_QCLOSE(gkLpWrPos3'gkLpLen3'gkLpState3'gkLpRdPos3'gkLpTarget3'giLpBuf3L'gkLpTrimPos3'gkLpRecBpm3)
          gkLpConsumed3 = 1
          printks "LP3: CLOSE\n", 0
        elseif (gkLpState3 == 4) then
          gkLpState3 = 2
          ; relock: re-enter exactly in time with the transport
          gkLpRdPos3 = (gkLpClock - gkLpAnchor3) % gkLpLen3
          gkLpConsumed3 = 1
          printks "LP3: RESUME\n", 0
        endif
        gkLpLastTap3 = kLpNow76
        gkLpPressT3  = kLpNow76
#

; LOOPER ch4 press gesture (menu 14, CC71) — moved verbatim from midi_handler.orc
#define LP_CH4_PRESS #
      elseif (gkCurrentMenu == 14) then
        ; LOOPER ch4 — press-fired for tight timing (rec/play/resume/clear
        ; all land on finger-down; release only classifies tap-vs-hold)
        kLpNow71 times
        if (kLpNow71 - gkLpLastTap4 < 0.5) then
          gkLpDblFired4 = 1
          gkLpState4  = 0
          gkLpLen4    = 0
          gkLpWrPos4  = 0
          gkLpRdPos4  = 0
          gkLpSpIdx4  = 6
          if (gkDrumChan == 4) then
            gkDrumChan = -1
            gkDrumIdx  = -1
          endif
          if (gkLpLen1 == 0 && gkLpLen2 == 0 && gkLpLen3 == 0 && gkLpLen5 == 0) then
            gkLpMasterLen = 0
          endif
          printks "LP4: CLEAR\n", 0
        elseif (gkLpState4 == 0) then
          gkLpState4  = 1
          gkLpWrPos4  = 0
          gkLpRdPos4  = 0
          gkLpSpIdx4  = 6
          gkLpAnchor4 = gkLpClock
          gkLpConsumed4 = 1
          printks "LP4: REC\n", 0
        elseif (gkLpState4 == 1) then
          $LP_QCLOSE(gkLpWrPos4'gkLpLen4'gkLpState4'gkLpRdPos4'gkLpTarget4'giLpBuf4L'gkLpTrimPos4'gkLpRecBpm4)
          gkLpConsumed4 = 1
          printks "LP4: CLOSE\n", 0
        elseif (gkLpState4 == 4) then
          gkLpState4 = 2
          ; relock: re-enter exactly in time with the transport
          gkLpRdPos4 = (gkLpClock - gkLpAnchor4) % gkLpLen4
          gkLpConsumed4 = 1
          printks "LP4: RESUME\n", 0
        endif
        gkLpLastTap4 = kLpNow71
        gkLpPressT4  = kLpNow71
#

; LOOPER ch5 press gesture (menu 14, CC72) — moved verbatim from midi_handler.orc
#define LP_CH5_PRESS #
      elseif (gkCurrentMenu == 14) then
        ; LOOPER ch5 — press-fired for tight timing (rec/play/resume/clear
        ; all land on finger-down; release only classifies tap-vs-hold)
        kLpNow72 times
        if (kLpNow72 - gkLpLastTap5 < 0.5) then
          gkLpDblFired5 = 1
          gkLpState5  = 0
          gkLpLen5    = 0
          gkLpWrPos5  = 0
          gkLpRdPos5  = 0
          gkLpSpIdx5  = 6
          if (gkDrumChan == 5) then
            gkDrumChan = -1
            gkDrumIdx  = -1
          endif
          if (gkLpLen1 == 0 && gkLpLen2 == 0 && gkLpLen3 == 0 && gkLpLen4 == 0) then
            gkLpMasterLen = 0
          endif
          printks "LP5: CLEAR\n", 0
        elseif (gkLpState5 == 0) then
          gkLpState5  = 1
          gkLpWrPos5  = 0
          gkLpRdPos5  = 0
          gkLpSpIdx5  = 6
          gkLpAnchor5 = gkLpClock
          gkLpConsumed5 = 1
          printks "LP5: REC\n", 0
        elseif (gkLpState5 == 1) then
          $LP_QCLOSE(gkLpWrPos5'gkLpLen5'gkLpState5'gkLpRdPos5'gkLpTarget5'giLpBuf5L'gkLpTrimPos5'gkLpRecBpm5)
          gkLpConsumed5 = 1
          printks "LP5: CLOSE\n", 0
        elseif (gkLpState5 == 4) then
          gkLpState5 = 2
          ; relock: re-enter exactly in time with the transport
          gkLpRdPos5 = (gkLpClock - gkLpAnchor5) % gkLpLen5
          gkLpConsumed5 = 1
          printks "LP5: RESUME\n", 0
        endif
        gkLpLastTap5 = kLpNow72
        gkLpPressT5  = kLpNow72
#

; LOOPER release handlers ch1-ch5 (tap vs hold classify) — moved verbatim from midi_handler.orc
#define LP_RELEASE_ALL #
    ;==========================================================
    ; LOOPER RELEASE HANDLERS (CC74/75/76/71/72, kVal==0, gkLpPressT>=0)
    ; Press does the tight-timing work (rec/play/resume/clear fire on
    ; finger-down). Release only classifies tap-vs-hold for play/overdub:
    ;   hold(>=0.4s): play->overdub, overdub->play
    ;   tap:          play->stop,    overdub->play
    ;==========================================================
    ; --- CH1 release (CC74) ---
    elseif (kCC == 74 && kVal == 0 && gkLpPressT1 >= 0) then
      kLpRel1 times
      kLpDt1  = kLpRel1 - gkLpPressT1
      gkLpPressT1 = -1
      if (gkLpDblFired1 >= 0.5) then
        gkLpDblFired1 = 0
      elseif (gkLpConsumed1 >= 0.5) then
        gkLpConsumed1 = 0
      else
        if (gkLpState1 == 2) then
          gkLpState1 = 4
          printks "LP1: STOP\n", 0
        elseif (gkLpState1 == 3) then
          gkLpState1 = 2
          printks "LP1: OVD->PLAY\n", 0
        endif
      endif

    ; --- CH2 release (CC75) ---
    elseif (kCC == 75 && kVal == 0 && gkLpPressT2 >= 0) then
      kLpRel2 times
      kLpDt2  = kLpRel2 - gkLpPressT2
      gkLpPressT2 = -1
      if (gkLpDblFired2 >= 0.5) then
        gkLpDblFired2 = 0
      elseif (gkLpConsumed2 >= 0.5) then
        gkLpConsumed2 = 0
      else
        if (gkLpState2 == 2) then
          gkLpState2 = 4
          printks "LP2: STOP\n", 0
        elseif (gkLpState2 == 3) then
          gkLpState2 = 2
          printks "LP2: OVD->PLAY\n", 0
        endif
      endif

    ; --- CH3 release (CC76) ---
    elseif (kCC == 76 && kVal == 0 && gkLpPressT3 >= 0) then
      kLpRel3 times
      kLpDt3  = kLpRel3 - gkLpPressT3
      gkLpPressT3 = -1
      if (gkLpDblFired3 >= 0.5) then
        gkLpDblFired3 = 0
      elseif (gkLpConsumed3 >= 0.5) then
        gkLpConsumed3 = 0
      else
        if (gkLpState3 == 2) then
          gkLpState3 = 4
          printks "LP3: STOP\n", 0
        elseif (gkLpState3 == 3) then
          gkLpState3 = 2
          printks "LP3: OVD->PLAY\n", 0
        endif
      endif

    ; --- CH4 release (CC71) ---
    elseif (kCC == 71 && kVal == 0 && gkLpPressT4 >= 0) then
      kLpRel4 times
      kLpDt4  = kLpRel4 - gkLpPressT4
      gkLpPressT4 = -1
      if (gkLpDblFired4 >= 0.5) then
        gkLpDblFired4 = 0
      elseif (gkLpConsumed4 >= 0.5) then
        gkLpConsumed4 = 0
      else
        if (gkLpState4 == 2) then
          gkLpState4 = 4
          printks "LP4: STOP\n", 0
        elseif (gkLpState4 == 3) then
          gkLpState4 = 2
          printks "LP4: OVD->PLAY\n", 0
        endif
      endif

    ; --- CH5 release (CC72) ---
    elseif (kCC == 72 && kVal == 0 && gkLpPressT5 >= 0) then
      kLpRel5 times
      kLpDt5  = kLpRel5 - gkLpPressT5
      gkLpPressT5 = -1
      if (gkLpDblFired5 >= 0.5) then
        gkLpDblFired5 = 0
      elseif (gkLpConsumed5 >= 0.5) then
        gkLpConsumed5 = 0
      else
        if (gkLpState5 == 2) then
          gkLpState5 = 4
          printks "LP5: STOP\n", 0
        elseif (gkLpState5 == 3) then
          gkLpState5 = 2
          printks "LP5: OVD->PLAY\n", 0
        endif
      endif
#

; LOOPER ch1 speed (menu 10, slot 0 rotation CC24) — moved verbatim from midi_handler.orc
#define LP_CH1_SPEED #
      if (gkCurrentMenu == 14) then
        if (giDrumBrowse == 1 && (gkDrumChan == 1 || (gkLpState1 == 0 && gkLpLen1 == 0))) then
          ; DRUM BROWSE request — drumloops.orc instr 216 honors it
          ; only when the drum loop would be the only looper content
          gkDrumReqCh = 1
          gkDrumStep += 1
        elseif (gkLpLen1 > 0) then
          ; LOOPER ch1 speed (slot 0 encoder) — only once a loop exists
          if (kVal > 64) then
            gkLpSpIdx1 = min(gkLpSpIdx1 + 1, 7)
          elseif (kVal < 64) then
            gkLpSpIdx1 = max(gkLpSpIdx1 - 1, 0)
          endif
        endif
#

; LOOPER ch2 speed (menu 10, slot 1 rotation CC25) — moved verbatim from midi_handler.orc
#define LP_CH2_SPEED #
      if (gkCurrentMenu == 14) then
        if (giDrumBrowse == 1 && (gkDrumChan == 2 || (gkLpState2 == 0 && gkLpLen2 == 0))) then
          ; DRUM BROWSE request (see ch1)
          gkDrumReqCh = 2
          gkDrumStep += 1
        elseif (gkLpLen2 > 0) then
          ; LOOPER ch2 speed (slot 1 encoder) — only once a loop exists
          if (kVal > 64) then
            gkLpSpIdx2 = min(gkLpSpIdx2 + 1, 7)
          elseif (kVal < 64) then
            gkLpSpIdx2 = max(gkLpSpIdx2 - 1, 0)
          endif
        endif
#

; LOOPER ch3 speed (menu 10, slot 2 rotation CC26) — moved verbatim from midi_handler.orc
#define LP_CH3_SPEED #
      if (gkCurrentMenu == 14) then
        if (giDrumBrowse == 1 && (gkDrumChan == 3 || (gkLpState3 == 0 && gkLpLen3 == 0))) then
          ; DRUM BROWSE request (see ch1)
          gkDrumReqCh = 3
          gkDrumStep += 1
        elseif (gkLpLen3 > 0) then
          ; LOOPER ch3 speed (slot 2 encoder) — only once a loop exists
          if (kVal > 64) then
            gkLpSpIdx3 = min(gkLpSpIdx3 + 1, 7)
          elseif (kVal < 64) then
            gkLpSpIdx3 = max(gkLpSpIdx3 - 1, 0)
          endif
        endif
#

; LOOPER ch4 speed (menu 10, slot 3 rotation CC21) — moved verbatim from midi_handler.orc
#define LP_CH4_SPEED #
      if (gkCurrentMenu == 14) then
        if (giDrumBrowse == 1 && (gkDrumChan == 4 || (gkLpState4 == 0 && gkLpLen4 == 0))) then
          ; DRUM BROWSE request (see ch1)
          gkDrumReqCh = 4
          gkDrumStep += 1
        elseif (gkLpLen4 > 0) then
          ; LOOPER ch4 speed (slot 3 encoder) — only once a loop exists
          if (kVal > 64) then
            gkLpSpIdx4 = min(gkLpSpIdx4 + 1, 7)
          elseif (kVal < 64) then
            gkLpSpIdx4 = max(gkLpSpIdx4 - 1, 0)
          endif
        endif
#

; LOOPER ch5 speed (menu 10, slot 4 rotation CC22) — moved verbatim from midi_handler.orc
#define LP_CH5_SPEED #
      if (gkCurrentMenu == 14) then
        if (giDrumBrowse == 1 && (gkDrumChan == 5 || (gkLpState5 == 0 && gkLpLen5 == 0))) then
          ; DRUM BROWSE request (see ch1)
          gkDrumReqCh = 5
          gkDrumStep += 1
        elseif (gkLpLen5 > 0) then
          ; LOOPER ch5 speed (slot 4/leftmost encoder) — only once a loop exists
          if (kVal > 64) then
            gkLpSpIdx5 = min(gkLpSpIdx5 + 1, 7)
          elseif (kVal < 64) then
            gkLpSpIdx5 = max(gkLpSpIdx5 - 1, 0)
          endif
        endif
#

; LOOPER hold watcher (every k-cycle, overdub threshold) — moved verbatim from midi_handler.orc
#define LP_HOLD_WATCHER #
  ;==============================================================
  ; LOOPER HOLD WATCHER — runs every k-cycle (not just on MIDI
  ; events) so overdub engages the moment the 0.4 s hold threshold
  ; passes, while the finger is still down. Everything played during
  ; the hold lands in the loop.
  ;==============================================================
  kLpNowW times
  if (gkLpPressT1 >= 0 && gkLpConsumed1 < 0.5 && gkLpDblFired1 < 0.5 && kLpNowW - gkLpPressT1 >= 0.4) then
    if (gkLpState1 == 2) then
      gkLpState1 = 3
      gkLpConsumed1 = 1
      printks "LP1: OVERDUB (hold)\n", 0
    elseif (gkLpState1 == 3) then
      gkLpState1 = 2
      gkLpConsumed1 = 1
      printks "LP1: OVD->PLAY (hold)\n", 0
    endif
  endif
  if (gkLpPressT2 >= 0 && gkLpConsumed2 < 0.5 && gkLpDblFired2 < 0.5 && kLpNowW - gkLpPressT2 >= 0.4) then
    if (gkLpState2 == 2) then
      gkLpState2 = 3
      gkLpConsumed2 = 1
      printks "LP2: OVERDUB (hold)\n", 0
    elseif (gkLpState2 == 3) then
      gkLpState2 = 2
      gkLpConsumed2 = 1
      printks "LP2: OVD->PLAY (hold)\n", 0
    endif
  endif
  if (gkLpPressT3 >= 0 && gkLpConsumed3 < 0.5 && gkLpDblFired3 < 0.5 && kLpNowW - gkLpPressT3 >= 0.4) then
    if (gkLpState3 == 2) then
      gkLpState3 = 3
      gkLpConsumed3 = 1
      printks "LP3: OVERDUB (hold)\n", 0
    elseif (gkLpState3 == 3) then
      gkLpState3 = 2
      gkLpConsumed3 = 1
      printks "LP3: OVD->PLAY (hold)\n", 0
    endif
  endif
  if (gkLpPressT4 >= 0 && gkLpConsumed4 < 0.5 && gkLpDblFired4 < 0.5 && kLpNowW - gkLpPressT4 >= 0.4) then
    if (gkLpState4 == 2) then
      gkLpState4 = 3
      gkLpConsumed4 = 1
      printks "LP4: OVERDUB (hold)\n", 0
    elseif (gkLpState4 == 3) then
      gkLpState4 = 2
      gkLpConsumed4 = 1
      printks "LP4: OVD->PLAY (hold)\n", 0
    endif
  endif
  if (gkLpPressT5 >= 0 && gkLpConsumed5 < 0.5 && gkLpDblFired5 < 0.5 && kLpNowW - gkLpPressT5 >= 0.4) then
    if (gkLpState5 == 2) then
      gkLpState5 = 3
      gkLpConsumed5 = 1
      printks "LP5: OVERDUB (hold)\n", 0
    elseif (gkLpState5 == 3) then
      gkLpState5 = 2
      gkLpConsumed5 = 1
      printks "LP5: OVD->PLAY (hold)\n", 0
    endif
  endif
#

; LOOPER state/speed/position OSC -> UI — moved verbatim from midi_handler.orc
#define LP_OSC_FEEDBACK #
  ; LOOPER state → UI
  $OSC_I(gkLpState1'/lp1_state'lp1st)
  $OSC_I(gkLpState2'/lp2_state'lp2st)
  $OSC_I(gkLpState3'/lp3_state'lp3st)
  $OSC_I(gkLpState4'/lp4_state'lp4st)
  $OSC_I(gkLpState5'/lp5_state'lp5st)
  $OSC_I(gkLpSpIdx1'/lp1_speed'lp1sp)
  $OSC_I(gkLpSpIdx2'/lp2_speed'lp2sp)
  $OSC_I(gkLpSpIdx3'/lp3_speed'lp3sp)
  $OSC_I(gkLpSpIdx4'/lp4_speed'lp4sp)
  $OSC_I(gkLpSpIdx5'/lp5_speed'lp5sp)
  ; Drum sample mode: which channel holds the drum loop (-1 = none),
  ; loaded file index, file count
  $OSC_I(gkDrumChan'/lp_drum_ch'lpdch)
  $OSC_I(gkDrumIdx'/lp_drum_idx'lpdix)
  $OSC_I(gkDrumCount'/lp_drum_n'lpdcn)
  ; Playback position as 0..1 fraction (for progress display).
  ; Metro-gated: raw RdPos changes every k-cycle while playing, which would
  ; flood the UI with thousands of OSC messages per second.
  kLpUiTick metro 12
  if (kLpUiTick == 1) then
    kLpPos1Fr = (gkLpLen1 > 0 ? gkLpRdPos1 / gkLpLen1 : 0)
    kLpPos2Fr = (gkLpLen2 > 0 ? gkLpRdPos2 / gkLpLen2 : 0)
    kLpPos3Fr = (gkLpLen3 > 0 ? gkLpRdPos3 / gkLpLen3 : 0)
    kLpPos4Fr = (gkLpLen4 > 0 ? gkLpRdPos4 / gkLpLen4 : 0)
    kLpPos5Fr = (gkLpLen5 > 0 ? gkLpRdPos5 / gkLpLen5 : 0)
    $OSC_F(kLpPos1Fr'/lp1_pos'lp1po)
    $OSC_F(kLpPos2Fr'/lp2_pos'lp2po)
    $OSC_F(kLpPos3Fr'/lp3_pos'lp3po)
    $OSC_F(kLpPos4Fr'/lp4_pos'lp4po)
    $OSC_F(kLpPos5Fr'/lp5_pos'lp5po)
  endif
#
