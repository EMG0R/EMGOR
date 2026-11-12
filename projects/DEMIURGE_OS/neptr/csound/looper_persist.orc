;==============================================================
; LOOPER PERSISTENCE — save/restore loop audio across restarts
; (instr 96, always-on, self-scheduled)
;
; *** DISABLED 2026-08-12 — guarded on LOOPER_PERSIST_AUDIO,
; *** which nothing defines. The code below is intact and
; *** compiles to nothing. Do not re-enable without reading this.
;
; WHY IT IS OFF. It was written to be a convenience: die
; mid-set, come back, your loops are still there. In practice
; it made the looper un-performable. A restart is the gesture a
; player reaches for precisely when the loop is WRONG, and the
; engine kept handing the wrong loop back — restored audio in
; ch1-5, restored gkLpLen<N>, and worst of all a restored
; gkLpMasterLen, which is the master grid length every later
; loop snaps to. With a master length already set, the first
; loop you record after a restart cannot define a new length:
; LP_QCLOSE's `gkLpMasterLen == 0` branch never fires and the
; new loop is quantized to a bar length from a previous
; session. That is the "it keeps remembering length / timing"
; failure, and it is not fixable by clearing files alone —
; whatever is on disk at boot becomes the grid.
;
; The correct behaviour for this instrument, and what you now
; get, is: every start is a blank slate. globals.orc inits
; gkLpState/Len/SpIdx to 0 and gkLpMasterLen to 0, and the
; giLpBuf* ftgens come up zeroed, so with this file compiled
; out there is nothing to reset — there is simply nothing to
; restore. The first loop closed after any boot defines a
; brand-new master length.
;
; Turning it off also drops the save side, and that is a bonus,
; not a loss: see KNOWN COST below — every loop close wrote
; ~36 MB from the audio thread and could audibly hiccup a live
; take. Nothing else in the tree reads lp<N>_*.ftab /
; lp_meta.ftab; stale copies in $STATE_DIR are inert and safe
; to delete.
;
; NOT affected: state_persist.orc (settings/presets, its own
; #ifdef STATE_PERSIST, writes neptr_settings.ftab). Settings
; persistence is a separate and still-wanted feature. It does
; restore gkLpSpIdx1..5 (loop playback speed, state slots
; 297-301) — that is a knob position, not audio or length, and
; is deliberately left alone.
;
; Everything from here down is the original description of the
; feature as it behaved when enabled.
;
; To resurrect it: pass --omacro:LOOPER_PERSIST_AUDIO=1 (and
; understand that you are re-introducing the restored-grid bug
; above unless you also stop restoring gkLpMasterLen).
;
; WHAT IT DOES
;   Watches gkLpState1..5 (declared/init'd in globals.orc, driven
;   by handler/handler_looper.orc macros + effects/looper.orc):
;     * transition {1 rec, 3 overdub, 5 sync-close} -> 2 (play):
;       the loop content/length may have changed -> save channel.
;       Saves are debounced to >= 2 s per channel (a save request
;       inside the window is latched and flushed when it expires,
;       so the LAST state always lands on disk).
;     * transition (nonzero) -> 0 (double-tap CLEAR): mark the
;       channel invalid in the meta table and re-save ONLY the
;       meta file. Audio files are left on disk; valid=0 is the
;       deletion (cheap, portable, no rm needed).
;   At startup it restores every channel whose meta valid flag is
;   1: buffers via ftload, then gkLpLen/gkLpSpIdx from meta, and
;   gkLpState = 4 (STOPPED). Restored loops never auto-play; the
;   normal resume gesture (single press: 4 -> 2) starts them.
;
; FILE FORMATS (all binary, ftsave iflag 0, in $STATE_DIR):
;   lp<N>_L.ftab / lp<N>_R.ftab  N = 1..5 (matches gkLpState<N>
;       channel numbering) — one full 2,304,000-sample raw table
;       per file (giLpBuf<N>L / giLpBuf<N>R, ftgen GEN -2).
;   lp_meta.ftab — the giLpPersistMeta table (16-slot raw -2
;       table; 15 used): [valid, len, spIdx] x 5 channels at
;       slots (N-1)*3 + {0,1,2}.
;
; INCLUDE ORDER (required):
;   globals.orc  ->  effects/looper.orc  ->  THIS FILE
;   It references gkLpState/Len/SpIdx/MasterLen and giLpBuf* which
;   are all created in globals.orc; including after looper.orc
;   also guarantees the whole looper is in place first. In
;   neptrPhase4.csd add, directly after the looper include:
;       #include "looper_persist.orc"        ; (line ~65)
;   and nothing else — instr 96 self-schedules below. To enable,
;   pass --omacro:STATE_PERSIST=1 (and optionally
;   --omacro:STATE_DIR=/some/dir) or #define it in the csd.
;
; RESTORE/HANDLER RACE — why the first-k-cycle write is safe:
;   gk assignments from instrument i-time don't stick for k-vars,
;   so the restore writes happen in the FIRST k-cycle (kDone init
;   0 pattern). At t=0 no MIDI has arrived: handler_looper only
;   mutates gkLpState<N> on CC press/release events, and the hold
;   watcher only acts when gkLpPressT<N> >= 0 (init -1). So the
;   states are still at their init 0 during our one-shot write;
;   nothing else can race it.
;
; ALSO RESTORED: gkLpMasterLen (from ch1 if ch1 is valid). Without
;   it, the first NEW loop closed after a restore would grab
;   master-transport authority (LP_QCLOSE's gkLpMasterLen==0
;   branch) and break grid-snap against the restored loops.
;
; OPCODE SEMANTICS (Csound 6.18, from memory — verify on compile):
;   ftsavek Sfile, ktrig, iflag, ifn [, ifn2 ...]   iflag 0=binary
;   ftload  Sfile, iflag, ifn [, ifn2 ...]          i-time; table
;       sizes in the file must match the existing tables, else
;       csound errors out. filevalid (i-time) only checks that the
;       file exists/opens — it does NOT validate contents, so a
;       truncated file could still abort startup. Uncertainty:
;       whether ftload of a size-mismatched file errors or aborts
;       performance; treated here as "don't let it happen" (files
;       are only ever written whole by ftsavek).
;
; KNOWN COST: each channel save writes 2 x ~18 MB from the audio
;   thread (k-rate ftsavek). On the Pi this can audibly hiccup;
;   it only ever happens on a loop-close/overdub-end gesture and
;   is debounced, which is the accepted trade-off for zero
;   changes to looper.orc / handler_looper.orc.
;==============================================================

; NOTE the guard: LOOPER_PERSIST_AUDIO, *not* STATE_PERSIST. The
; csd defines STATE_PERSIST for state_persist.orc (settings), and
; this file used to ride along on it — which is how looper-audio
; persistence ended up permanently on. Disabling it is now a
; matter of not defining a macro nobody defines.
#ifdef LOOPER_PERSIST_AUDIO

; NOTE: the literal below is a static fallback only — a normal
; launch (neptr/scripts/launch_phase4.sh) always overrides this
; macro with --omacro:STATE_DIR=$HOME/... so it matches whatever Pi
; user is actually running. This default only fires on a manual,
; direct `csound` invocation that bypasses that script.
#ifndef STATE_DIR
#define STATE_DIR #/home/gorpi7/demiurge/state#
#endif

; meta table: [valid,len,spIdx] x 5, raw (GEN -2), 16 slots
giLpPersistMeta ftgen 0, 0, -16, -2, 0

instr 96
    ;----------------------------------------------------------
    ; i-time: ensure dir, build filenames, load files if present
    ;----------------------------------------------------------
    SDir  = "$STATE_DIR"
    SCmd  sprintf "mkdir -p %s", SDir
    iRet system_i 1, SCmd

    SMeta sprintf "%s/lp_meta.ftab", SDir
    S1L sprintf "%s/lp1_L.ftab", SDir
    S1R sprintf "%s/lp1_R.ftab", SDir
    S2L sprintf "%s/lp2_L.ftab", SDir
    S2R sprintf "%s/lp2_R.ftab", SDir
    S3L sprintf "%s/lp3_L.ftab", SDir
    S3R sprintf "%s/lp3_R.ftab", SDir
    S4L sprintf "%s/lp4_L.ftab", SDir
    S4R sprintf "%s/lp4_R.ftab", SDir
    S5L sprintf "%s/lp5_L.ftab", SDir
    S5R sprintf "%s/lp5_R.ftab", SDir

    iV1 = 0
    iV2 = 0
    iV3 = 0
    iV4 = 0
    iV5 = 0
    iLen1 = 0
    iLen2 = 0
    iLen3 = 0
    iLen4 = 0
    iLen5 = 0
    iSp1 = 6
    iSp2 = 6
    iSp3 = 6
    iSp4 = 6
    iSp5 = 6

    iMetaOk filevalid SMeta
    if (iMetaOk == 1) then
        ftload SMeta, 0, giLpPersistMeta

        ; --- channel 1 ---
        iV1   tab_i 0, giLpPersistMeta
        iLen1 tab_i 1, giLpPersistMeta
        iSp1  tab_i 2, giLpPersistMeta
        if (iV1 == 1 && iLen1 > 0) then
            iOk1L filevalid S1L
            iOk1R filevalid S1R
            if (iOk1L == 1 && iOk1R == 1) then
                ftload S1L, 0, giLpBuf1L
                ftload S1R, 0, giLpBuf1R
                prints "LP-PERSIST: ch1 restored (len %d)\n", iLen1
            else
                iV1 = 0
            endif
        else
            iV1 = 0
        endif

        ; --- channel 2 ---
        iV2   tab_i 3, giLpPersistMeta
        iLen2 tab_i 4, giLpPersistMeta
        iSp2  tab_i 5, giLpPersistMeta
        if (iV2 == 1 && iLen2 > 0) then
            iOk2L filevalid S2L
            iOk2R filevalid S2R
            if (iOk2L == 1 && iOk2R == 1) then
                ftload S2L, 0, giLpBuf2L
                ftload S2R, 0, giLpBuf2R
                prints "LP-PERSIST: ch2 restored (len %d)\n", iLen2
            else
                iV2 = 0
            endif
        else
            iV2 = 0
        endif

        ; --- channel 3 ---
        iV3   tab_i 6, giLpPersistMeta
        iLen3 tab_i 7, giLpPersistMeta
        iSp3  tab_i 8, giLpPersistMeta
        if (iV3 == 1 && iLen3 > 0) then
            iOk3L filevalid S3L
            iOk3R filevalid S3R
            if (iOk3L == 1 && iOk3R == 1) then
                ftload S3L, 0, giLpBuf3L
                ftload S3R, 0, giLpBuf3R
                prints "LP-PERSIST: ch3 restored (len %d)\n", iLen3
            else
                iV3 = 0
            endif
        else
            iV3 = 0
        endif

        ; --- channel 4 ---
        iV4   tab_i 9, giLpPersistMeta
        iLen4 tab_i 10, giLpPersistMeta
        iSp4  tab_i 11, giLpPersistMeta
        if (iV4 == 1 && iLen4 > 0) then
            iOk4L filevalid S4L
            iOk4R filevalid S4R
            if (iOk4L == 1 && iOk4R == 1) then
                ftload S4L, 0, giLpBuf4L
                ftload S4R, 0, giLpBuf4R
                prints "LP-PERSIST: ch4 restored (len %d)\n", iLen4
            else
                iV4 = 0
            endif
        else
            iV4 = 0
        endif

        ; --- channel 5 ---
        iV5   tab_i 12, giLpPersistMeta
        iLen5 tab_i 13, giLpPersistMeta
        iSp5  tab_i 14, giLpPersistMeta
        if (iV5 == 1 && iLen5 > 0) then
            iOk5L filevalid S5L
            iOk5R filevalid S5R
            if (iOk5L == 1 && iOk5R == 1) then
                ftload S5L, 0, giLpBuf5L
                ftload S5R, 0, giLpBuf5R
                prints "LP-PERSIST: ch5 restored (len %d)\n", iLen5
            else
                iV5 = 0
            endif
        else
            iV5 = 0
        endif
    endif

    ; clamp restored speed indices to the giLpSpeeds range 0..7
    iSp1 = (iSp1 < 0 ? 0 : (iSp1 > 7 ? 7 : iSp1))
    iSp2 = (iSp2 < 0 ? 0 : (iSp2 > 7 ? 7 : iSp2))
    iSp3 = (iSp3 < 0 ? 0 : (iSp3 > 7 ? 7 : iSp3))
    iSp4 = (iSp4 < 0 ? 0 : (iSp4 > 7 ? 7 : iSp4))
    iSp5 = (iSp5 < 0 ? 0 : (iSp5 > 7 ? 7 : iSp5))

    ;----------------------------------------------------------
    ; k-rate: one-shot restore of gk vars on the FIRST k-cycle
    ; (safe: no MIDI yet, handler leaves all states at init 0 —
    ; see RESTORE/HANDLER RACE note in the header)
    ;----------------------------------------------------------
    kDone init 0
    if (kDone == 0) then
        kDone = 1
        if (iV1 == 1) then
            gkLpLen1   = iLen1
            gkLpSpIdx1 = iSp1
            gkLpState1 = 4
            gkLpMasterLen = iLen1
        endif
        if (iV2 == 1) then
            gkLpLen2   = iLen2
            gkLpSpIdx2 = iSp2
            gkLpState2 = 4
        endif
        if (iV3 == 1) then
            gkLpLen3   = iLen3
            gkLpSpIdx3 = iSp3
            gkLpState3 = 4
        endif
        if (iV4 == 1) then
            gkLpLen4   = iLen4
            gkLpSpIdx4 = iSp4
            gkLpState4 = 4
        endif
        if (iV5 == 1) then
            gkLpLen5   = iLen5
            gkLpSpIdx5 = iSp5
            gkLpState5 = 4
        endif
    endif

    ;----------------------------------------------------------
    ; k-rate: watch state transitions, save with 2 s debounce
    ;----------------------------------------------------------
    kNow times
    kMetaTrig = 0

    ; per-channel: previous state, pending-save latch, last-save time
    kPrev1 init 0
    kPrev2 init 0
    kPrev3 init 0
    kPrev4 init 0
    kPrev5 init 0
    kPend1 init 0
    kPend2 init 0
    kPend3 init 0
    kPend4 init 0
    kPend5 init 0
    kLast1 init -2
    kLast2 init -2
    kLast3 init -2
    kLast4 init -2
    kLast5 init -2
    kTrig1 = 0
    kTrig2 = 0
    kTrig3 = 0
    kTrig4 = 0
    kTrig5 = 0

    ; --- channel 1 ---
    if (gkLpState1 == 2 && (kPrev1 == 1 || kPrev1 == 3 || kPrev1 == 5)) then
        kPend1 = 1              ; loop closed / overdub ended -> content dirty
    elseif (gkLpState1 == 0 && kPrev1 != 0) then
        kPend1 = 0              ; CLEAR: invalidate, re-save meta only
        tablew 0, 0, giLpPersistMeta
        kMetaTrig = 1
    endif
    if (kPend1 == 1 && kNow - kLast1 >= 2) then
        kPend1 = 0
        kLast1 = kNow
        kTrig1 = 1
        tablew 1,          0, giLpPersistMeta
        tablew gkLpLen1,   1, giLpPersistMeta
        tablew gkLpSpIdx1, 2, giLpPersistMeta
        kMetaTrig = 1
        printks "LP-PERSIST: ch1 saved\n", 0
    endif
    ftsavek S1L, kTrig1, 0, giLpBuf1L
    ftsavek S1R, kTrig1, 0, giLpBuf1R

    ; --- channel 2 ---
    if (gkLpState2 == 2 && (kPrev2 == 1 || kPrev2 == 3 || kPrev2 == 5)) then
        kPend2 = 1
    elseif (gkLpState2 == 0 && kPrev2 != 0) then
        kPend2 = 0
        tablew 0, 3, giLpPersistMeta
        kMetaTrig = 1
    endif
    if (kPend2 == 1 && kNow - kLast2 >= 2) then
        kPend2 = 0
        kLast2 = kNow
        kTrig2 = 1
        tablew 1,          3, giLpPersistMeta
        tablew gkLpLen2,   4, giLpPersistMeta
        tablew gkLpSpIdx2, 5, giLpPersistMeta
        kMetaTrig = 1
        printks "LP-PERSIST: ch2 saved\n", 0
    endif
    ftsavek S2L, kTrig2, 0, giLpBuf2L
    ftsavek S2R, kTrig2, 0, giLpBuf2R

    ; --- channel 3 ---
    if (gkLpState3 == 2 && (kPrev3 == 1 || kPrev3 == 3 || kPrev3 == 5)) then
        kPend3 = 1
    elseif (gkLpState3 == 0 && kPrev3 != 0) then
        kPend3 = 0
        tablew 0, 6, giLpPersistMeta
        kMetaTrig = 1
    endif
    if (kPend3 == 1 && kNow - kLast3 >= 2) then
        kPend3 = 0
        kLast3 = kNow
        kTrig3 = 1
        tablew 1,          6, giLpPersistMeta
        tablew gkLpLen3,   7, giLpPersistMeta
        tablew gkLpSpIdx3, 8, giLpPersistMeta
        kMetaTrig = 1
        printks "LP-PERSIST: ch3 saved\n", 0
    endif
    ftsavek S3L, kTrig3, 0, giLpBuf3L
    ftsavek S3R, kTrig3, 0, giLpBuf3R

    ; --- channel 4 ---
    if (gkLpState4 == 2 && (kPrev4 == 1 || kPrev4 == 3 || kPrev4 == 5)) then
        kPend4 = 1
    elseif (gkLpState4 == 0 && kPrev4 != 0) then
        kPend4 = 0
        tablew 0, 9, giLpPersistMeta
        kMetaTrig = 1
    endif
    if (kPend4 == 1 && kNow - kLast4 >= 2) then
        kPend4 = 0
        kLast4 = kNow
        kTrig4 = 1
        tablew 1,           9, giLpPersistMeta
        tablew gkLpLen4,   10, giLpPersistMeta
        tablew gkLpSpIdx4, 11, giLpPersistMeta
        kMetaTrig = 1
        printks "LP-PERSIST: ch4 saved\n", 0
    endif
    ftsavek S4L, kTrig4, 0, giLpBuf4L
    ftsavek S4R, kTrig4, 0, giLpBuf4R

    ; --- channel 5 ---
    if (gkLpState5 == 2 && (kPrev5 == 1 || kPrev5 == 3 || kPrev5 == 5)) then
        kPend5 = 1
    elseif (gkLpState5 == 0 && kPrev5 != 0) then
        kPend5 = 0
        tablew 0, 12, giLpPersistMeta
        kMetaTrig = 1
    endif
    if (kPend5 == 1 && kNow - kLast5 >= 2) then
        kPend5 = 0
        kLast5 = kNow
        kTrig5 = 1
        tablew 1,          12, giLpPersistMeta
        tablew gkLpLen5,   13, giLpPersistMeta
        tablew gkLpSpIdx5, 14, giLpPersistMeta
        kMetaTrig = 1
        printks "LP-PERSIST: ch5 saved\n", 0
    endif
    ftsavek S5L, kTrig5, 0, giLpBuf5L
    ftsavek S5R, kTrig5, 0, giLpBuf5R

    ; meta file — saved whenever any channel saved or cleared
    ftsavek SMeta, kMetaTrig, 0, giLpPersistMeta

    kPrev1 = gkLpState1
    kPrev2 = gkLpState2
    kPrev3 = gkLpState3
    kPrev4 = gkLpState4
    kPrev5 = gkLpState5
endin

schedule 96, 0, -1

#endif  ; LOOPER_PERSIST_AUDIO
