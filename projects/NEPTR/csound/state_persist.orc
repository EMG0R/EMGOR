;==============================================================
; NEPTR SETTINGS PERSISTENCE — HAND-MAINTAINED since 2026-09 (was generated
; by vst/generate_vst.py; the generator is STALE for this file: running it
; rewrites the file and DROPS slots 302+ — menu, toggles, master out, BPM,
; mvd (POLY DLY v5: slots 145-151,307,308), hyperGuitar lanes, instr 96 preset bank. Do not run it without
; porting those first). Edit the write side here and the matching
; state_unpack.inc.orc lines in lockstep.
; (original note: the registry lived in the generator's FX table.)
;
; instr 97, always-on: every 2 s compares each registered global
; against the state table; on any change rewrites the table and
; ftsavek's it to $STATE_DIR/neptr_settings.ftab. At startup the
; file (if present) is loaded and, on non-VST builds (the Pi),
; every global is restored on the first k-cycle. VST builds
; define STATE_VST and restore through the WIDGETS instead (see
; the generated csd), so GUI and engine stay consistent.
;
; Include AFTER globals.orc (any position after it is fine).
; No-op unless STATE_PERSIST is defined.
;==============================================================

#ifdef STATE_PERSIST

; NOTE: the literal below is a static fallback only — a normal
; launch (neptr/scripts/launch_phase4.sh) always overrides this
; macro with --omacro:PRESET_DIR=$HOME/... so it matches whatever Pi
; user is actually running. This default only fires on a manual,
; direct `csound` invocation that bypasses that script.
#ifndef PRESET_DIR
#define PRESET_DIR #/home/gorpi7/demiurge/presets#
#endif
; NOTE: same fallback story as PRESET_DIR above — overridden by
; --omacro:STATE_DIR=$HOME/... in launch_phase4.sh on a normal launch.
#ifndef STATE_DIR
#define STATE_DIR #/home/gorpi7/demiurge/state#
#endif

giNeptrState ftgen 0, 0, -512, -2, 0
gkNeptrStLoaded init 0   ; 0 pending, 1 loaded, -1 no file

; PRESET BANK (2026-09-09) — 17 full-state slots in one table, persisted per
; context. Layout: slot N (1..17) occupies indices (N-1)*512 .. N*512-1 (a full
; giNeptrState snapshot); occupancy flags live in the tail at PB_OCC+(N-1); a
; layout stamp at PB_STAMP_I validates a loaded file (else the bank is treated
; empty). 17*512 = 8704 data + 17 occ + pad + stamp -> size 8736.
giPresetBank ftgen 0, 0, -8736, -2, 0
#define PB_SLOTW   #512#
#define PB_OCC     #8704#
#define PB_STAMP_I #8735#
#define PB_STAMP   #771217#
; SAVE GENERATION (2026-09-25): monotonic counter written into the table before
; every bank save. The dual-file scheme alternates A/B so a truncated write can
; never destroy the only copy - but the loader took A whenever its stamp was
; valid, and the stamp is valid in BOTH files, so whenever the last capture
; landed in B it was invisible after a reboot ("I made presets last night and
; they're gone": slot 11 lived only in bank.b.ftab). Each save dumps the WHOLE
; in-memory bank, so the file with the higher generation is the complete
; current bank; the loader now picks that one. Files from before this change
; read 0 here and still load.
#define PB_GEN_I   #8730#
gkPresetBankLoaded init 0

; ACTIVATION SEQUENCES (2026-11-02): tables, ring, per-id apply UDOs. See preset_seq.orc.
#include "preset_seq.orc"

instr 97  ; SETTINGS STATE ENGINE
    SDir = "$STATE_DIR"
    SCmd sprintf "mkdir -p \"%s\"", SDir
    iRet system_i 1, SCmd
    SFile sprintf "%s/neptr_settings.ftab", SDir
    ; 2026-09-01: dual-file save. ftsavek writes IN PLACE with the layout
    ; stamp at index 511 — the END of the file — so a power cut mid-write
    ; truncates the stamp off while filevalid still passes, and the next
    ; boot silently discards EVERY saved setting ("it forgot everything").
    ; Saves now alternate between two files; boot restores whichever one
    ; carries a valid stamp. Worst case after a crash: lose one save tick.
    SFileB sprintf "%s/neptr_settings.b.ftab", SDir
    ; PRESET BANK paths — per context, derived from the PRESET_BANK launch macro
    ; (0=surf, 1=bass, 2=gorcore) so surf's bank file stays separate from
    ; gorcore's automatically (a set change is a reload/restart, so i-time is
    ; fine). Lives beside neptr_settings under ~/demiurge/presets/<ctx>/.
    SPreCtx strcpy "surf"
    if ($PRESET_BANK == 1) then
        SPreCtx strcpy "bass"
    elseif ($PRESET_BANK == 2) then
        SPreCtx strcpy "gorcore"
    endif
    ; 2026-09-25: the preset bank lives on SD, INDEPENDENT of STATE_DIR.
    ; It used to be derived as STATE_DIR/../presets, so moving the settings
    ; file to tmpfs (a real-time fix - see demiurge-state-ram) silently moved
    ; the BANK into RAM too: empty after every boot, and captures lost on a
    ; power cut ("all my presets are gone"). The bank is written only on a
    ; capture/delete, never on the audio thread's 2 s cadence, so it has no
    ; business being anywhere but persistent storage.
    SPreDir sprintf "%s/%s", "$PRESET_DIR", SPreCtx
    SPreCmd sprintf "mkdir -p \"%s\"", SPreDir
    iRetP system_i 1, SPreCmd
    SBankA sprintf "%s/bank.ftab", SPreDir
    SBankB sprintf "%s/bank.b.ftab", SPreDir
    SSeqA sprintf "%s/seq.ftab", SPreDir
    SSeqB sprintf "%s/seq.b.ftab", SPreDir
    ; 2026-11-02: pick the NEWER valid copy, not "A if valid". Saves alternate
    ; A/B, so A is routinely one save OLDER than B; "A first" threw away the
    ; last change (preset selection) after a power cut. Slot 348 = save
    ; generation (bumped on every save); the higher valid generation wins.
    iOk = 0
    iStamp = 0
    iOkA filevalid SFile
    iOkB filevalid SFileB
    iGenA = -1
    iGenB = -1
    if (iOkA == 1) then
        ftload SFile, 0, giNeptrState
        if (tab_i(511, giNeptrState) == 77000302) then
            iGenA = tab_i(348, giNeptrState)
        endif
    endif
    if (iOkB == 1) then
        ftload SFileB, 0, giNeptrState
        if (tab_i(511, giNeptrState) == 77000302) then
            iGenB = tab_i(348, giNeptrState)
        endif
    endif
    iAwins = (iGenA >= 0 && iGenA >= iGenB) ? 1 : 0
    if (iAwins == 1) then
        ftload SFile, 0, giNeptrState      ; table currently holds B: reload A
        iOk = 1
        iStamp = 77000302
    elseif (iGenB >= 0) then
        iOk = 1                            ; table already holds B
        iStamp = 77000302
        prints "[state] restored from alternate copy (newer or primary bad)\n"
    elseif (iOkA == 1 || iOkB == 1) then
        iOk = 1                            ; files exist but no valid stamp
    endif
    ; A file whose stamp does not match THIS layout is not restored. It
    ; is not enough that the file exists and parses: `filevalid` says
    ; nothing about whether index 220 still means what it meant when the
    ; file was written. Restoring an unstamped file is how every effect
    ; came back ON at boot — a bypass global reads 0 for 'active', so an
    ; unwritten or shifted region silently ENABLES the whole chain, and
    ; on 2026-08-11 that switched on CHAINFB (a deliberately
    ; self-oscillating feedback loop) at every single boot. Declining to
    ; restore costs the user their saved settings once; restoring
    ; garbage costs them an instrument that screams.
    iGood = ((iOk == 1 && iStamp == 77000302) ? 1 : 0)
    ; Stamp the live table so every save from here on identifies itself.
    tableiw 77000302, 511, giNeptrState
    kFlag init 0
    if (kFlag == 0) then
        kFlag = 1
        gkNeptrStLoaded = (iGood == 1 ? 1 : -1)
        if (iOk == 1 && iGood == 0) then
            prints "[state] %s ignored: layout stamp mismatch (want %d) — defaults kept, effects stay bypassed\n", SFile, 77000302
        endif
#ifndef STATE_VST
        ; ---- direct global restore (Pi build) ----
        if (iGood == 1) then
            #include "state_unpack.inc.orc"
            ; ONE-TIME VOCAL LEVEL MIGRATION (2026-11-01). The persisted vocal
            ; values at the time (pre +24 dB x level 7x = +41 dB, reverb wet 1.0,
            ; level 0.0 left over from fighting it) are the "ridiculous gain" bug.
            ; Table slot 343 is the stamp (0 = not migrated). Boot-only: it is NOT
            ; in state_unpack.inc.orc, so preset recalls never run it. The new
            ; values differ from the persisted ones, so the change-scan below
            ; rewrites 319..342 and the stamp lands in the next settings save.
            if (tab:k(343, giNeptrState) < 1) then
                chnset 15, "voc_pre_db"
                chnset 1, "voc_level"
                chnset 0.5, "voc_dly_wet"
                chnset 0, "voc_rev_wet"
                tablew 1, 343, giNeptrState
            endif
            ; SHOW-READY boot restore (2026-11-02), see the write side (344..347).
            ; Read the table BEFORE the change-scan can overwrite it. 0 = absent.
            kShFx = tab:k(344, giNeptrState)
            if (kShFx > 50) then
                gkDuoFx = kShFx - 100
            endif
            kShNam = tab:k(345, giNeptrState)
            if (kShNam > 50) then
                gkDuoNam = kShNam - 100
            endif
            kShPr = tab:k(346, giNeptrState)
            if (kShPr > 50) then
                gkPrLoaded = kShPr - 100
            endif
            kShGate = tab:k(347, giNeptrState)
            if (kShGate > 50) then
                chnset kShGate - 100, "voc_gate"
            endif
        endif
#endif
        ; ---- PRESET BANK load (once) ---- restore the per-context bank file,
        ; validating a layout stamp; fall back to the .b copy; a missing/invalid
        ; stamp means a fresh bank -> clear all 17 occupancy flags so every slot
        ; reads empty until the user captures into it.
        ; load each file that exists, note (stamp, generation), then keep the
        ; valid one with the higher generation - see PB_GEN_I.
        iGenA = -1
        iGenB = -1
        iBankOk filevalid SBankA
        if (iBankOk == 1) then
            ftload SBankA, 0, giPresetBank
            iStA tab_i $PB_STAMP_I, giPresetBank
            if (iStA == $PB_STAMP) then
                iGenA tab_i $PB_GEN_I, giPresetBank
            endif
        endif
        iBankOkB filevalid SBankB
        if (iBankOkB == 1) then
            ftload SBankB, 0, giPresetBank
            iStB tab_i $PB_STAMP_I, giPresetBank
            if (iStB == $PB_STAMP) then
                iGenB tab_i $PB_GEN_I, giPresetBank
            endif
        endif
        ; table currently holds B (or A if B was missing). Reload A when it is
        ; valid and at least as fresh; equal generations mean identical banks.
        if (iGenA >= 0 && iGenA >= iGenB) then
            ftload SBankA, 0, giPresetBank
        endif
        iBStamp tab_i $PB_STAMP_I, giPresetBank
        if (iBStamp != $PB_STAMP) then
            iZ = 0
            while (iZ < 17) do
                tableiw 0, $PB_OCC + iZ, giPresetBank
                iZ += 1
            od
        endif
        tableiw $PB_STAMP, $PB_STAMP_I, giPresetBank
        ; ---- ACTIVATION SEQUENCE sidecar (same A/B + generation rule) ----
        iSGenA = -1
        iSGenB = -1
        iSqOkA filevalid SSeqA
        if (iSqOkA == 1) then
            ftload SSeqA, 0, giSeqBank
            if (tab_i($PB_STAMP_I, giSeqBank) == $SEQ_STAMP) then
                iSGenA tab_i $PB_GEN_I, giSeqBank
            endif
        endif
        iSqOkB filevalid SSeqB
        if (iSqOkB == 1) then
            ftload SSeqB, 0, giSeqBank
            if (tab_i($PB_STAMP_I, giSeqBank) == $SEQ_STAMP) then
                iSGenB tab_i $PB_GEN_I, giSeqBank
            endif
        endif
        if (iSGenA >= 0 && iSGenA >= iSGenB) then
            ftload SSeqA, 0, giSeqBank
        endif
        if (tab_i($PB_STAMP_I, giSeqBank) != $SEQ_STAMP) then
            iZ = 0
            while (iZ < 8736) do
                tableiw 0, iZ, giSeqBank      ; no/invalid file: every slot legacy
                iZ += 1
            od
        endif
        tableiw $SEQ_STAMP, $PB_STAMP_I, giSeqBank
        gkSeqMode = 2     ; first scan rebuilds the activation ring from restored state
        gkPresetBankLoaded = 1
    endif
    ; ---- change-scan + save (2 s cadence) ----
    ; PRESET RECALL (op 2): copy bank slot -> giNeptrState, unpack to globals,
    ; resync UI. Runs BEFORE the write-side so the freshly-unpacked globals match
    ; the table and the change-scan below won't clobber them. An EMPTY slot
    ; (occupancy 0) is a no-op — a recall never overwrites live state with zeros.
    ; DUODECIMUS 1/3/5 are NAM-only: a plain recall of them (web app, ch16 CC106)
    ; becomes the NAM-only recall (op 5 below), never the full-state/sequence path.
    if (gkPresetOp == 2 && (gkPresetSlot == 1 || gkPresetSlot == 3 || gkPresetSlot == 5)) then
        gkPresetOp = 5
    endif
    if (gkPresetOp == 1 && (gkPresetSlot == 1 || gkPresetSlot == 3 || gkPresetSlot == 5)) then
        gkPresetOp = 4
    endif
    if (gkPresetOp == 2 && gkPresetBankLoaded == 1) then
        kRSlot = gkPresetSlot
        if (kRSlot >= 1 && kRSlot <= 17) then
            kROcc = tab:k($PB_OCC + kRSlot - 1, giPresetBank)
            if (kROcc >= 0.5) then
                ; snapshot BLOOPER + GLITCH-menu live globals so the recall
                ; leaves those generative effects EXACTLY as they are now.
                #include "state_preset_exclude_save.inc.orc"
                kRBase = (kRSlot - 1) * $PB_SLOTW
                kRi = 0
                while (kRi < $PB_SLOTW) do
                    tablew tab:k(kRBase + kRi, giPresetBank), kRi, giNeptrState
                    kRi += 1
                od
                #include "state_unpack.inc.orc"
                ; put the excluded generative effects back to their live values
                ; (unpack just overwrote them from the preset row — undo that).
                #include "state_preset_exclude_restore.inc.orc"
                ; PUSH THE RECALLED hyperGuitar LANES TO THE BELA (2026-10-03).
                ; Unpack set the gkHgLane* mirror from the preset; the Bela is
                ; the owner, so tell it. It applies and echoes /hg/state, which
                ; the mirror then follows (hyperguitar.orc "WHO OWNS THE
                ; STATE"). Unrolled: HgSendLane keeps a per-call-site OSC
                ; trigger counter, so each lane needs its own call site. A
                ; preset captured before lanes were persisted leaves the mirror
                ; at its live value, so this resends current state (no-op).
                ; Boot does NOT come through here (it keeps HgSendSync).
                if (gkHgPresent == 1) then
                    HgSendLane 0, gkHgLaneOn[0], gkHgLaneIdx[0]
                    HgSendLane 1, gkHgLaneOn[1], gkHgLaneIdx[1]
                    HgSendLane 2, gkHgLaneOn[2], gkHgLaneIdx[2]
                    HgSendLane 3, gkHgLaneOn[3], gkHgLaneIdx[3]
                    HgSendLane 4, gkHgLaneOn[4], gkHgLaneIdx[4]
                endif
                ; TELL THE UI A RECALL IS HAPPENING (2026-09-25).
                ; Until now a recall broadcast NOTHING - the UI only ever saw
                ; the burst of per-effect bypass echoes that gkOscRefresh
                ; triggers, and since "an effect switched on" means "open that
                ; effect's page", the screen got dragged through a dozen pages
                ; and parked on whichever landed last. Mid-song that is how you
                ; lose the LOOPER page you were using.
                ; This marker arrives BEFORE the echo burst (gkOscRefresh is
                ; consumed after this line), so the UI can suppress
                ; toggle-driven navigation for the duration of the burst and
                ; simply stay where it is.
                ; SOUNDMAGIC rack (spectral/sm_persist.orc): restore the slot's
                ; params + on flags. A slot captured before the rack existed (or
                ; empty) has no stamp in giSMonBank -> leave the rack as it is.
                if (tab:k((kRSlot - 1) * 24 + 23, giSMonBank) == $SM_STAMP) then
                    kSMi = 0
                    while (kSMi < 768) do
                        tablew tab:k((kRSlot - 1) * 768 + kSMi, giSMPBank), kSMi, giSMP
                        kSMi += 1
                    od
                    kSMi = 0
                    while (kSMi < 24) do
                        tablew tab:k((kRSlot - 1) * 24 + kSMi, giSMonBank), kSMi, giSMon
                        kSMi += 1
                    od
                endif
                ; ACTIVATION SEQUENCE (2026-11-02). A slot with a sequence flag is
                ; replayed as if pressed by hand: GUITAR effects off, then each step
                ; in order (effect ON / vocal / NAM amp), so the last pedal claimer
                ; owns the pedal - normally VOLUME, as the last step. A legacy slot
                ; (no sequence) keeps the 2026-11-01 behaviour: row as captured,
                ; then VOLUME takes the pedal as if toggled last.
                if (tab:k($SEQ_FLAG + kRSlot - 1, giSeqBank) >= 0.5) then
                    PresetRecallSeq kRSlot
                else
                    gkLastToggle   = 1
                    gkLastToggle29 = 1
                    gkSeqMode = 2
                endif
                if (gkLastToggle29 == 1 && gkExprPos >= 0) then
                    gkVOLpedal = gkExprPos
                endif
                gkExprHandoff = 1
                OSC3x_i "/preset_recall", kRSlot
                gkPrLoaded = kRSlot
                gkOscRefresh = 1
            endif
        endif
    endif
    ; A CAPTURE (op 1) forces a full globals->table sync THIS cycle (the OR
    ; below) so the snapshot taken after the write-side is exact-current.
    kMet metro 0.5
    kDirty = 0
    if ((kMet == 1 || gkPresetOp == 1) && gkNeptrStLoaded != 0) then
        if (gkVOLpedalBypass != tab:k(0, giNeptrState)) then
            tablew gkVOLpedalBypass, 0, giNeptrState
            kDirty = 1
        endif
        if (gkVOLpedal != tab:k(1, giNeptrState)) then
            tablew gkVOLpedal, 1, giNeptrState
            kDirty = 1
        endif
        if (gkATTK_bypass != tab:k(2, giNeptrState)) then
            tablew gkATTK_bypass, 2, giNeptrState
            kDirty = 1
        endif
        if (gkATTK_mix != tab:k(3, giNeptrState)) then
            tablew gkATTK_mix, 3, giNeptrState
            kDirty = 1
        endif
        if (gkATTK_atkMs != tab:k(4, giNeptrState)) then
            tablew gkATTK_atkMs, 4, giNeptrState
            kDirty = 1
        endif
        if (gkATTK_sens != tab:k(5, giNeptrState)) then
            tablew gkATTK_sens, 5, giNeptrState
            kDirty = 1
        endif
        if (gkATTK_shape != tab:k(6, giNeptrState)) then
            tablew gkATTK_shape, 6, giNeptrState
            kDirty = 1
        endif
        if (gkATTK_range != tab:k(7, giNeptrState)) then
            tablew gkATTK_range, 7, giNeptrState
            kDirty = 1
        endif
        if (gkATTK_retrigMs != tab:k(8, giNeptrState)) then
            tablew gkATTK_retrigMs, 8, giNeptrState
            kDirty = 1
        endif
        if (gkLeslieBypass != tab:k(9, giNeptrState)) then
            tablew gkLeslieBypass, 9, giNeptrState
            kDirty = 1
        endif
        if (gkLeslieSpeed != tab:k(10, giNeptrState)) then
            tablew gkLeslieSpeed, 10, giNeptrState
            kDirty = 1
        endif
        if (gkLeslieDepth != tab:k(11, giNeptrState)) then
            tablew gkLeslieDepth, 11, giNeptrState
            kDirty = 1
        endif
        if (gkBypassRM != tab:k(12, giNeptrState)) then
            tablew gkBypassRM, 12, giNeptrState
            kDirty = 1
        endif
        if (gkARModDepth != tab:k(13, giNeptrState)) then
            tablew gkARModDepth, 13, giNeptrState
            kDirty = 1
        endif
        if (gkARModFreq != tab:k(14, giNeptrState)) then
            tablew gkARModFreq, 14, giNeptrState
            kDirty = 1
        endif
        if (gkTapeBypass != tab:k(15, giNeptrState)) then
            tablew gkTapeBypass, 15, giNeptrState
            kDirty = 1
        endif
        if (gkTapeAmount != tab:k(16, giNeptrState)) then
            tablew gkTapeAmount, 16, giNeptrState
            kDirty = 1
        endif
        if (gkTapeFail != tab:k(17, giNeptrState)) then
            tablew gkTapeFail, 17, giNeptrState
            kDirty = 1
        endif
        if (gkTapeModel != tab:k(18, giNeptrState)) then
            tablew gkTapeModel, 18, giNeptrState
            kDirty = 1
        endif
        if (gkFormsBypass != tab:k(19, giNeptrState)) then
            tablew gkFormsBypass, 19, giNeptrState
            kDirty = 1
        endif
        if (gkFormsMix != tab:k(20, giNeptrState)) then
            tablew gkFormsMix, 20, giNeptrState
            kDirty = 1
        endif
        if (gkFormsShift != tab:k(21, giNeptrState)) then
            tablew gkFormsShift, 21, giNeptrState
            kDirty = 1
        endif
        if (gkFormantBypass != tab:k(22, giNeptrState)) then
            tablew gkFormantBypass, 22, giNeptrState
            kDirty = 1
        endif
        if (gkFormantMix != tab:k(23, giNeptrState)) then
            tablew gkFormantMix, 23, giNeptrState
            kDirty = 1
        endif
        if (gkFormantShift != tab:k(24, giNeptrState)) then
            tablew gkFormantShift, 24, giNeptrState
            kDirty = 1
        endif
        if (gkSpecDelBypass != tab:k(25, giNeptrState)) then
            tablew gkSpecDelBypass, 25, giNeptrState
            kDirty = 1
        endif
        if (gkSpecDelMix != tab:k(26, giNeptrState)) then
            tablew gkSpecDelMix, 26, giNeptrState
            kDirty = 1
        endif
        if (gkSpecDelMaxDel != tab:k(27, giNeptrState)) then
            tablew gkSpecDelMaxDel, 27, giNeptrState
            kDirty = 1
        endif
        if (gkSpecDelFB != tab:k(28, giNeptrState)) then
            tablew gkSpecDelFB, 28, giNeptrState
            kDirty = 1
        endif
        if (gkBinBypass != tab:k(29, giNeptrState)) then
            tablew gkBinBypass, 29, giNeptrState
            kDirty = 1
        endif
        if (gkBinPedal != tab:k(30, giNeptrState)) then
            tablew gkBinPedal, 30, giNeptrState
            kDirty = 1
        endif
        if (gkBcBypass != tab:k(31, giNeptrState)) then
            tablew gkBcBypass, 31, giNeptrState
            kDirty = 1
        endif
        if (gkBcPedal != tab:k(32, giNeptrState)) then
            tablew gkBcPedal, 32, giNeptrState
            kDirty = 1
        endif
        if (gkBscBypass != tab:k(33, giNeptrState)) then
            tablew gkBscBypass, 33, giNeptrState
            kDirty = 1
        endif
        if (gkBscMix != tab:k(34, giNeptrState)) then
            tablew gkBscMix, 34, giNeptrState
            kDirty = 1
        endif
        if (gkBscScatter != tab:k(35, giNeptrState)) then
            tablew gkBscScatter, 35, giNeptrState
            kDirty = 1
        endif
        if (gkBscFB != tab:k(36, giNeptrState)) then
            tablew gkBscFB, 36, giNeptrState
            kDirty = 1
        endif
        if (gkBscMaxDel != tab:k(37, giNeptrState)) then
            tablew gkBscMaxDel, 37, giNeptrState
            kDirty = 1
        endif
        if (gkBypassH90 != tab:k(38, giNeptrState)) then
            tablew gkBypassH90, 38, giNeptrState
            kDirty = 1
        endif
        if (gkH90mix != tab:k(39, giNeptrState)) then
            tablew gkH90mix, 39, giNeptrState
            kDirty = 1
        endif
        if (gkSemiH90 != tab:k(40, giNeptrState)) then
            tablew gkSemiH90, 40, giNeptrState
            kDirty = 1
        endif
        if (gkBypassPS != tab:k(41, giNeptrState)) then
            tablew gkBypassPS, 41, giNeptrState
            kDirty = 1
        endif
        if (gkDryPctPS != tab:k(42, giNeptrState)) then
            tablew gkDryPctPS, 42, giNeptrState
            kDirty = 1
        endif
        if (gkSemi != tab:k(43, giNeptrState)) then
            tablew gkSemi, 43, giNeptrState
            kDirty = 1
        endif
        if (gkBypassGPS != tab:k(44, giNeptrState)) then
            tablew gkBypassGPS, 44, giNeptrState
            kDirty = 1
        endif
        if (gkGPSmix != tab:k(45, giNeptrState)) then
            tablew gkGPSmix, 45, giNeptrState
            kDirty = 1
        endif
        if (gkGPSsemis != tab:k(46, giNeptrState)) then
            tablew gkGPSsemis, 46, giNeptrState
            kDirty = 1
        endif
        if (gkGPSfback != tab:k(47, giNeptrState)) then
            tablew gkGPSfback, 47, giNeptrState
            kDirty = 1
        endif
        if (gkGPSdly != tab:k(48, giNeptrState)) then
            tablew gkGPSdly, 48, giNeptrState
            kDirty = 1
        endif
        if (gkBypassPF != tab:k(49, giNeptrState)) then
            tablew gkBypassPF, 49, giNeptrState
            kDirty = 1
        endif
        if (gkPFmix != tab:k(50, giNeptrState)) then
            tablew gkPFmix, 50, giNeptrState
            kDirty = 1
        endif
        if (gkSemiPF != tab:k(51, giNeptrState)) then
            tablew gkSemiPF, 51, giNeptrState
            kDirty = 1
        endif
        if (gkLsyBypass != tab:k(52, giNeptrState)) then
            tablew gkLsyBypass, 52, giNeptrState
            kDirty = 1
        endif
        if (gkLSY_mix != tab:k(53, giNeptrState)) then
            tablew gkLSY_mix, 53, giNeptrState
            kDirty = 1
        endif
        if (gkLSY_loss != tab:k(54, giNeptrState)) then
            tablew gkLSY_loss, 54, giNeptrState
            kDirty = 1
        endif
        if (gkLSY_bw != tab:k(55, giNeptrState)) then
            tablew gkLSY_bw, 55, giNeptrState
            kDirty = 1
        endif
        if (gkLSY_pkt != tab:k(56, giNeptrState)) then
            tablew gkLSY_pkt, 56, giNeptrState
            kDirty = 1
        endif
        if (gkRES_bypass != tab:k(57, giNeptrState)) then
            tablew gkRES_bypass, 57, giNeptrState
            kDirty = 1
        endif
        if (gkRES_mix != tab:k(58, giNeptrState)) then
            tablew gkRES_mix, 58, giNeptrState
            kDirty = 1
        endif
        if (gkRES_onset != tab:k(59, giNeptrState)) then
            tablew gkRES_onset, 59, giNeptrState
            kDirty = 1
        endif
        if (gkRES_fb != tab:k(60, giNeptrState)) then
            tablew gkRES_fb, 60, giNeptrState
            kDirty = 1
        endif
        if (gkRES_oct != tab:k(61, giNeptrState)) then
            tablew gkRES_oct, 61, giNeptrState
            kDirty = 1
        endif
        if (gkRES_frz != tab:k(62, giNeptrState)) then
            tablew gkRES_frz, 62, giNeptrState
            kDirty = 1
        endif
        if (gkWAH_bypass != tab:k(63, giNeptrState)) then
            tablew gkWAH_bypass, 63, giNeptrState
            kDirty = 1
        endif
        if (gkWAH_freq != tab:k(64, giNeptrState)) then
            tablew gkWAH_freq, 64, giNeptrState
            kDirty = 1
        endif
        if (gkWAH_res != tab:k(65, giNeptrState)) then
            tablew gkWAH_res, 65, giNeptrState
            kDirty = 1
        endif
        if (gkAUTOMOOG_bypass != tab:k(66, giNeptrState)) then
            tablew gkAUTOMOOG_bypass, 66, giNeptrState
            kDirty = 1
        endif
        if (gkAUTOMOOG_sens != tab:k(67, giNeptrState)) then
            tablew gkAUTOMOOG_sens, 67, giNeptrState
            kDirty = 1
        endif
        if (gkAUTOMOOG_res != tab:k(68, giNeptrState)) then
            tablew gkAUTOMOOG_res, 68, giNeptrState
            kDirty = 1
        endif
        if (gkAUTOMOOG_type != tab:k(69, giNeptrState)) then
            tablew gkAUTOMOOG_type, 69, giNeptrState
            kDirty = 1
        endif
        if (gkAUTOMOOG_level != tab:k(70, giNeptrState)) then
            tablew gkAUTOMOOG_level, 70, giNeptrState
            kDirty = 1
        endif
        if (gkVIB_bypass != tab:k(71, giNeptrState)) then
            tablew gkVIB_bypass, 71, giNeptrState
            kDirty = 1
        endif
        if (gkVIB_depth != tab:k(72, giNeptrState)) then
            tablew gkVIB_depth, 72, giNeptrState
            kDirty = 1
        endif
        if (gkVIB_freq != tab:k(73, giNeptrState)) then
            tablew gkVIB_freq, 73, giNeptrState
            kDirty = 1
        endif
        if (gkVIB_fback != tab:k(74, giNeptrState)) then
            tablew gkVIB_fback, 74, giNeptrState
            kDirty = 1
        endif
        if (gkBypassChorus != tab:k(75, giNeptrState)) then
            tablew gkBypassChorus, 75, giNeptrState
            kDirty = 1
        endif
        if (gkChorusMix != tab:k(76, giNeptrState)) then
            tablew gkChorusMix, 76, giNeptrState
            kDirty = 1
        endif
        if (gkChorusRate != tab:k(77, giNeptrState)) then
            tablew gkChorusRate, 77, giNeptrState
            kDirty = 1
        endif
        if (gkChorusDepth != tab:k(78, giNeptrState)) then
            tablew gkChorusDepth, 78, giNeptrState
            kDirty = 1
        endif
        if (gkPHA_bypass != tab:k(79, giNeptrState)) then
            tablew gkPHA_bypass, 79, giNeptrState
            kDirty = 1
        endif
        if (gkPHA_depth != tab:k(80, giNeptrState)) then
            tablew gkPHA_depth, 80, giNeptrState
            kDirty = 1
        endif
        if (gkPHA_rate != tab:k(81, giNeptrState)) then
            tablew gkPHA_rate, 81, giNeptrState
            kDirty = 1
        endif
        if (gkPHA_fback != tab:k(82, giNeptrState)) then
            tablew gkPHA_fback, 82, giNeptrState
            kDirty = 1
        endif
        if (gkPHA_stages != tab:k(83, giNeptrState)) then
            tablew gkPHA_stages, 83, giNeptrState
            kDirty = 1
        endif
        if (gkTremBypass != tab:k(84, giNeptrState)) then
            tablew gkTremBypass, 84, giNeptrState
            kDirty = 1
        endif
        if (gkTremDepth != tab:k(85, giNeptrState)) then
            tablew gkTremDepth, 85, giNeptrState
            kDirty = 1
        endif
        if (gkTremMode != tab:k(86, giNeptrState)) then
            tablew gkTremMode, 86, giNeptrState
            kDirty = 1
        endif
        if (gkTremRate != tab:k(87, giNeptrState)) then
            tablew gkTremRate, 87, giNeptrState
            kDirty = 1
        endif
        if (gkFLA_bypass != tab:k(88, giNeptrState)) then
            tablew gkFLA_bypass, 88, giNeptrState
            kDirty = 1
        endif
        if (gkFLA_depth != tab:k(89, giNeptrState)) then
            tablew gkFLA_depth, 89, giNeptrState
            kDirty = 1
        endif
        if (gkFLA_rate != tab:k(90, giNeptrState)) then
            tablew gkFLA_rate, 90, giNeptrState
            kDirty = 1
        endif
        if (gkFLA_fback != tab:k(91, giNeptrState)) then
            tablew gkFLA_fback, 91, giNeptrState
            kDirty = 1
        endif
        if (gkOD_bypass != tab:k(92, giNeptrState)) then
            tablew gkOD_bypass, 92, giNeptrState
            kDirty = 1
        endif
        if (gkOD_drive != tab:k(93, giNeptrState)) then
            tablew gkOD_drive, 93, giNeptrState
            kDirty = 1
        endif
        if (gkOD_tone != tab:k(94, giNeptrState)) then
            tablew gkOD_tone, 94, giNeptrState
            kDirty = 1
        endif
        if (gkOD_wc1 != tab:k(95, giNeptrState)) then
            tablew gkOD_wc1, 95, giNeptrState
            kDirty = 1
        endif
        if (gkOD_wc2 != tab:k(96, giNeptrState)) then
            tablew gkOD_wc2, 96, giNeptrState
            kDirty = 1
        endif
        if (gkMAR_bypass != tab:k(97, giNeptrState)) then
            tablew gkMAR_bypass, 97, giNeptrState
            kDirty = 1
        endif
        if (gkMAR_drive != tab:k(98, giNeptrState)) then
            tablew gkMAR_drive, 98, giNeptrState
            kDirty = 1
        endif
        if (gkMAR_tone != tab:k(99, giNeptrState)) then
            tablew gkMAR_tone, 99, giNeptrState
            kDirty = 1
        endif
        if (gkMAR_wc1 != tab:k(100, giNeptrState)) then
            tablew gkMAR_wc1, 100, giNeptrState
            kDirty = 1
        endif
        if (gkMAR_wc2 != tab:k(101, giNeptrState)) then
            tablew gkMAR_wc2, 101, giNeptrState
            kDirty = 1
        endif
        if (gkRMZ_bypass != tab:k(102, giNeptrState)) then
            tablew gkRMZ_bypass, 102, giNeptrState
            kDirty = 1
        endif
        if (gkRMZ_drive != tab:k(103, giNeptrState)) then
            tablew gkRMZ_drive, 103, giNeptrState
            kDirty = 1
        endif
        if (gkRMZ_tone != tab:k(104, giNeptrState)) then
            tablew gkRMZ_tone, 104, giNeptrState
            kDirty = 1
        endif
        if (gkRMZ_wc1 != tab:k(105, giNeptrState)) then
            tablew gkRMZ_wc1, 105, giNeptrState
            kDirty = 1
        endif
        if (gkRMZ_wc2 != tab:k(106, giNeptrState)) then
            tablew gkRMZ_wc2, 106, giNeptrState
            kDirty = 1
        endif
        if (gkDHG_bypass != tab:k(107, giNeptrState)) then
            tablew gkDHG_bypass, 107, giNeptrState
            kDirty = 1
        endif
        if (gkDHG_drive != tab:k(108, giNeptrState)) then
            tablew gkDHG_drive, 108, giNeptrState
            kDirty = 1
        endif
        if (gkDHG_tone != tab:k(109, giNeptrState)) then
            tablew gkDHG_tone, 109, giNeptrState
            kDirty = 1
        endif
        if (gkDHG_wc1 != tab:k(110, giNeptrState)) then
            tablew gkDHG_wc1, 110, giNeptrState
            kDirty = 1
        endif
        if (gkDHG_wc2 != tab:k(111, giNeptrState)) then
            tablew gkDHG_wc2, 111, giNeptrState
            kDirty = 1
        endif
        if (gkMUF_bypass != tab:k(112, giNeptrState)) then
            tablew gkMUF_bypass, 112, giNeptrState
            kDirty = 1
        endif
        if (gkMUF_drive != tab:k(113, giNeptrState)) then
            tablew gkMUF_drive, 113, giNeptrState
            kDirty = 1
        endif
        if (gkMUF_tone != tab:k(114, giNeptrState)) then
            tablew gkMUF_tone, 114, giNeptrState
            kDirty = 1
        endif
        if (gkMUF_wc1 != tab:k(115, giNeptrState)) then
            tablew gkMUF_wc1, 115, giNeptrState
            kDirty = 1
        endif
        if (gkMUF_wc2 != tab:k(116, giNeptrState)) then
            tablew gkMUF_wc2, 116, giNeptrState
            kDirty = 1
        endif
        if (gkPreamp_bypass != tab:k(117, giNeptrState)) then
            tablew gkPreamp_bypass, 117, giNeptrState
            kDirty = 1
        endif
        if (gkBPL_bypass != tab:k(118, giNeptrState)) then
            tablew gkBPL_bypass, 118, giNeptrState
            kDirty = 1
        endif
        if (gkBPL_drive != tab:k(119, giNeptrState)) then
            tablew gkBPL_drive, 119, giNeptrState
            kDirty = 1
        endif
        if (gkBPH_bypass != tab:k(120, giNeptrState)) then
            tablew gkBPH_bypass, 120, giNeptrState
            kDirty = 1
        endif
        if (gkBPH_drive != tab:k(121, giNeptrState)) then
            tablew gkBPH_drive, 121, giNeptrState
            kDirty = 1
        endif
        if (gkBDS_bypass != tab:k(122, giNeptrState)) then
            tablew gkBDS_bypass, 122, giNeptrState
            kDirty = 1
        endif
        if (gkBDS_drive != tab:k(123, giNeptrState)) then
            tablew gkBDS_drive, 123, giNeptrState
            kDirty = 1
        endif
        if (gkBDS_blend != tab:k(124, giNeptrState)) then
            tablew gkBDS_blend, 124, giNeptrState
            kDirty = 1
        endif
        if (gkBDS_tone != tab:k(125, giNeptrState)) then
            tablew gkBDS_tone, 125, giNeptrState
            kDirty = 1
        endif
        if (gkCMP_bypass != tab:k(126, giNeptrState)) then
            tablew gkCMP_bypass, 126, giNeptrState
            kDirty = 1
        endif
        if (gkCMP_amt != tab:k(127, giNeptrState)) then
            tablew gkCMP_amt, 127, giNeptrState
            kDirty = 1
        endif
        if (gkCMP_speed != tab:k(128, giNeptrState)) then
            tablew gkCMP_speed, 128, giNeptrState
            kDirty = 1
        endif
        if (gkCMP_makeup != tab:k(129, giNeptrState)) then
            tablew gkCMP_makeup, 129, giNeptrState
            kDirty = 1
        endif
        if (gkCMP_blend != tab:k(130, giNeptrState)) then
            tablew gkCMP_blend, 130, giNeptrState
            kDirty = 1
        endif
        if (gkCMP_gate != tab:k(131, giNeptrState)) then
            tablew gkCMP_gate, 131, giNeptrState
            kDirty = 1
        endif
        if (gkBypassDelay != tab:k(132, giNeptrState)) then
            tablew gkBypassDelay, 132, giNeptrState
            kDirty = 1
        endif
        if (gkDelMix != tab:k(133, giNeptrState)) then
            tablew gkDelMix, 133, giNeptrState
            kDirty = 1
        endif
        if (gkDelayMS != tab:k(134, giNeptrState)) then
            tablew gkDelayMS, 134, giNeptrState
            kDirty = 1
        endif
        if (gkFeedback != tab:k(135, giNeptrState)) then
            tablew gkFeedback, 135, giNeptrState
            kDirty = 1
        endif
        if (gkSemiDel != tab:k(136, giNeptrState)) then
            tablew gkSemiDel, 136, giNeptrState
            kDirty = 1
        endif
        if (gkDlyMode != tab:k(137, giNeptrState)) then
            tablew gkDlyMode, 137, giNeptrState
            kDirty = 1
        endif
        if (gkTdlBypass != tab:k(138, giNeptrState)) then
            tablew gkTdlBypass, 138, giNeptrState
            kDirty = 1
        endif
        if (gkTdlMix != tab:k(139, giNeptrState)) then
            tablew gkTdlMix, 139, giNeptrState
            kDirty = 1
        endif
        if (gkTdlTimeMs != tab:k(140, giNeptrState)) then
            tablew gkTdlTimeMs, 140, giNeptrState
            kDirty = 1
        endif
        if (gkTdlFeedback != tab:k(141, giNeptrState)) then
            tablew gkTdlFeedback, 141, giNeptrState
            kDirty = 1
        endif
        if (gkTdlFiltCtr != tab:k(142, giNeptrState)) then
            tablew gkTdlFiltCtr, 142, giNeptrState
            kDirty = 1
        endif
        if (gkTdlFiltQ != tab:k(143, giNeptrState)) then
            tablew gkTdlFiltQ, 143, giNeptrState
            kDirty = 1
        endif
        if (gkTdlHeadMode != tab:k(144, giNeptrState)) then
            tablew gkTdlHeadMode, 144, giNeptrState
            kDirty = 1
        endif
        if (gkMvdBypass != tab:k(145, giNeptrState)) then
            tablew gkMvdBypass, 145, giNeptrState
            kDirty = 1
        endif
        ; POLY DLY v5 (2026-10-22): slots 146..151 + 307/308 reused from the v4 mvd
        ; params (146 NumL 147 DenL 148 NumR 149 DenR 150 Intv 151 Att 307 Dec 308 Reps)
        if (gkMvdNumL != tab:k(146, giNeptrState)) then
            tablew gkMvdNumL, 146, giNeptrState
            kDirty = 1
        endif
        if (gkMvdDenL != tab:k(147, giNeptrState)) then
            tablew gkMvdDenL, 147, giNeptrState
            kDirty = 1
        endif
        if (gkMvdNumR != tab:k(148, giNeptrState)) then
            tablew gkMvdNumR, 148, giNeptrState
            kDirty = 1
        endif
        if (gkMvdDenR != tab:k(149, giNeptrState)) then
            tablew gkMvdDenR, 149, giNeptrState
            kDirty = 1
        endif
        if (gkMvdIntv != tab:k(150, giNeptrState)) then
            tablew gkMvdIntv, 150, giNeptrState
            kDirty = 1
        endif
        if (gkMvdAtt != tab:k(151, giNeptrState)) then
            tablew gkMvdAtt, 151, giNeptrState
            kDirty = 1
        endif
        if (gkNebBypass != tab:k(152, giNeptrState)) then
            tablew gkNebBypass, 152, giNeptrState
            kDirty = 1
        endif
        if (gkNebMix != tab:k(153, giNeptrState)) then
            tablew gkNebMix, 153, giNeptrState
            kDirty = 1
        endif
        if (gkNebDensity != tab:k(154, giNeptrState)) then
            tablew gkNebDensity, 154, giNeptrState
            kDirty = 1
        endif
        if (gkNebSize != tab:k(155, giNeptrState)) then
            tablew gkNebSize, 155, giNeptrState
            kDirty = 1
        endif
        if (gkShmBypass != tab:k(156, giNeptrState)) then
            tablew gkShmBypass, 156, giNeptrState
            kDirty = 1
        endif
        if (gkShmMix != tab:k(157, giNeptrState)) then
            tablew gkShmMix, 157, giNeptrState
            kDirty = 1
        endif
        if (gkShmInterval != tab:k(158, giNeptrState)) then
            tablew gkShmInterval, 158, giNeptrState
            kDirty = 1
        endif
        if (gkShmFbAmt != tab:k(159, giNeptrState)) then
            tablew gkShmFbAmt, 159, giNeptrState
            kDirty = 1
        endif
        if (gkFrzBypass != tab:k(160, giNeptrState)) then
            tablew gkFrzBypass, 160, giNeptrState
            kDirty = 1
        endif
        if (gkFrzBedLevel != tab:k(161, giNeptrState)) then
            tablew gkFrzBedLevel, 161, giNeptrState
            kDirty = 1
        endif
        if (gkFrzDamp != tab:k(162, giNeptrState)) then
            tablew gkFrzDamp, 162, giNeptrState
            kDirty = 1
        endif
        if (gkFrzListenGain != tab:k(163, giNeptrState)) then
            tablew gkFrzListenGain, 163, giNeptrState
            kDirty = 1
        endif
        if (gkGvbBypass != tab:k(164, giNeptrState)) then
            tablew gkGvbBypass, 164, giNeptrState
            kDirty = 1
        endif
        if (gkGvbMix != tab:k(165, giNeptrState)) then
            tablew gkGvbMix, 165, giNeptrState
            kDirty = 1
        endif
        if (gkGvbSpacing != tab:k(166, giNeptrState)) then
            tablew gkGvbSpacing, 166, giNeptrState
            kDirty = 1
        endif
        if (gkGvbDecay != tab:k(167, giNeptrState)) then
            tablew gkGvbDecay, 167, giNeptrState
            kDirty = 1
        endif
        if (gkverbBypass != tab:k(168, giNeptrState)) then
            tablew gkverbBypass, 168, giNeptrState
            kDirty = 1
        endif
        if (gkverbMix != tab:k(169, giNeptrState)) then
            tablew gkverbMix, 169, giNeptrState
            kDirty = 1
        endif
        if (gkReverbFB != tab:k(170, giNeptrState)) then
            tablew gkReverbFB, 170, giNeptrState
            kDirty = 1
        endif
        if (gkReverbDamp != tab:k(171, giNeptrState)) then
            tablew gkReverbDamp, 171, giNeptrState
            kDirty = 1
        endif
        if (gkGranBypass != tab:k(172, giNeptrState)) then
            tablew gkGranBypass, 172, giNeptrState
            kDirty = 1
        endif
        if (gkGranMix != tab:k(173, giNeptrState)) then
            tablew gkGranMix, 173, giNeptrState
            kDirty = 1
        endif
        if (gkGranDens != tab:k(174, giNeptrState)) then
            tablew gkGranDens, 174, giNeptrState
            kDirty = 1
        endif
        if (gkGranSize != tab:k(175, giNeptrState)) then
            tablew gkGranSize, 175, giNeptrState
            kDirty = 1
        endif
        if (gkGranPitch != tab:k(176, giNeptrState)) then
            tablew gkGranPitch, 176, giNeptrState
            kDirty = 1
        endif
        if (gkMOOD_Bypass != tab:k(177, giNeptrState)) then
            tablew gkMOOD_Bypass, 177, giNeptrState
            kDirty = 1
        endif
        if (gkMOOD_Mix != tab:k(178, giNeptrState)) then
            tablew gkMOOD_Mix, 178, giNeptrState
            kDirty = 1
        endif
        if (gkMOOD_Clock != tab:k(179, giNeptrState)) then
            tablew gkMOOD_Clock, 179, giNeptrState
            kDirty = 1
        endif
        if (gkMOOD_Dens != tab:k(180, giNeptrState)) then
            tablew gkMOOD_Dens, 180, giNeptrState
            kDirty = 1
        endif
        if (gkMOOD_Dly != tab:k(181, giNeptrState)) then
            tablew gkMOOD_Dly, 181, giNeptrState
            kDirty = 1
        endif
        if (gkOCT_bypass != tab:k(182, giNeptrState)) then
            tablew gkOCT_bypass, 182, giNeptrState
            kDirty = 1
        endif
        if (gkOCT_sub != tab:k(183, giNeptrState)) then
            tablew gkOCT_sub, 183, giNeptrState
            kDirty = 1
        endif
        if (gkOCT_tone != tab:k(184, giNeptrState)) then
            tablew gkOCT_tone, 184, giNeptrState
            kDirty = 1
        endif
        if (gkOCT_char != tab:k(185, giNeptrState)) then
            tablew gkOCT_char, 185, giNeptrState
            kDirty = 1
        endif
        if (gkENV_bypass != tab:k(186, giNeptrState)) then
            tablew gkENV_bypass, 186, giNeptrState
            kDirty = 1
        endif
        if (gkENV_wet != tab:k(187, giNeptrState)) then
            tablew gkENV_wet, 187, giNeptrState
            kDirty = 1
        endif
        if (gkENV_sens != tab:k(188, giNeptrState)) then
            tablew gkENV_sens, 188, giNeptrState
            kDirty = 1
        endif
        if (gkENV_mode != tab:k(189, giNeptrState)) then
            tablew gkENV_mode, 189, giNeptrState
            kDirty = 1
        endif
        if (gkENV_att != tab:k(190, giNeptrState)) then
            tablew gkENV_att, 190, giNeptrState
            kDirty = 1
        endif
        if (gkENV_dec != tab:k(191, giNeptrState)) then
            tablew gkENV_dec, 191, giNeptrState
            kDirty = 1
        endif
        if (gkTSY_bypass != tab:k(192, giNeptrState)) then
            tablew gkTSY_bypass, 192, giNeptrState
            kDirty = 1
        endif
        if (gkTSY_mix != tab:k(193, giNeptrState)) then
            tablew gkTSY_mix, 193, giNeptrState
            kDirty = 1
        endif
        if (gkTSY_cut != tab:k(194, giNeptrState)) then
            tablew gkTSY_cut, 194, giNeptrState
            kDirty = 1
        endif
        if (gkTSY_res != tab:k(195, giNeptrState)) then
            tablew gkTSY_res, 195, giNeptrState
            kDirty = 1
        endif
        if (gkTSY_glide != tab:k(196, giNeptrState)) then
            tablew gkTSY_glide, 196, giNeptrState
            kDirty = 1
        endif
        if (gkTSY_sub != tab:k(197, giNeptrState)) then
            tablew gkTSY_sub, 197, giNeptrState
            kDirty = 1
        endif
        if (gkTSY_det != tab:k(198, giNeptrState)) then
            tablew gkTSY_det, 198, giNeptrState
            kDirty = 1
        endif
        if (gkPLL_bypass != tab:k(199, giNeptrState)) then
            tablew gkPLL_bypass, 199, giNeptrState
            kDirty = 1
        endif
        if (gkPLL_mix != tab:k(200, giNeptrState)) then
            tablew gkPLL_mix, 200, giNeptrState
            kDirty = 1
        endif
        if (gkPLL_intv != tab:k(201, giNeptrState)) then
            tablew gkPLL_intv, 201, giNeptrState
            kDirty = 1
        endif
        if (gkPLL_lock != tab:k(202, giNeptrState)) then
            tablew gkPLL_lock, 202, giNeptrState
            kDirty = 1
        endif
        if (gkPLL_vmix != tab:k(203, giNeptrState)) then
            tablew gkPLL_vmix, 203, giNeptrState
            kDirty = 1
        endif
        if (gkTEQ_bypass != tab:k(204, giNeptrState)) then
            tablew gkTEQ_bypass, 204, giNeptrState
            kDirty = 1
        endif
        if (gkTEQ_mid != tab:k(205, giNeptrState)) then
            tablew gkTEQ_mid, 205, giNeptrState
            kDirty = 1
        endif
        if (gkTEQ_freq != tab:k(206, giNeptrState)) then
            tablew gkTEQ_freq, 206, giNeptrState
            kDirty = 1
        endif
        if (gkTEQ_tilt != tab:k(207, giNeptrState)) then
            tablew gkTEQ_tilt, 207, giNeptrState
            kDirty = 1
        endif
        if (gkTEQ_low != tab:k(208, giNeptrState)) then
            tablew gkTEQ_low, 208, giNeptrState
            kDirty = 1
        endif
        if (gkTEQ_high != tab:k(209, giNeptrState)) then
            tablew gkTEQ_high, 209, giNeptrState
            kDirty = 1
        endif
        if (gkFBK_bypass != tab:k(210, giNeptrState)) then
            tablew gkFBK_bypass, 210, giNeptrState
            kDirty = 1
        endif
        if (gkFBK_level != tab:k(211, giNeptrState)) then
            tablew gkFBK_level, 211, giNeptrState
            kDirty = 1
        endif
        if (gkFBK_harm != tab:k(212, giNeptrState)) then
            tablew gkFBK_harm, 212, giNeptrState
            kDirty = 1
        endif
        if (gkFBK_bloom != tab:k(213, giNeptrState)) then
            tablew gkFBK_bloom, 213, giNeptrState
            kDirty = 1
        endif
        if (gkFBK_vib != tab:k(214, giNeptrState)) then
            tablew gkFBK_vib, 214, giNeptrState
            kDirty = 1
        endif
        if (gkDIF_bypass != tab:k(215, giNeptrState)) then
            tablew gkDIF_bypass, 215, giNeptrState
            kDirty = 1
        endif
        if (gkDIF_wet != tab:k(216, giNeptrState)) then
            tablew gkDIF_wet, 216, giNeptrState
            kDirty = 1
        endif
        if (gkDIF_depth != tab:k(217, giNeptrState)) then
            tablew gkDIF_depth, 217, giNeptrState
            kDirty = 1
        endif
        if (gkDIF_rate != tab:k(218, giNeptrState)) then
            tablew gkDIF_rate, 218, giNeptrState
            kDirty = 1
        endif
        if (gkDIF_duck != tab:k(219, giNeptrState)) then
            tablew gkDIF_duck, 219, giNeptrState
            kDirty = 1
        endif
        if (gkCFB_bypass != tab:k(220, giNeptrState)) then
            tablew gkCFB_bypass, 220, giNeptrState
            kDirty = 1
        endif
        if (gkCFB_ret != tab:k(221, giNeptrState)) then
            tablew gkCFB_ret, 221, giNeptrState
            kDirty = 1
        endif
        if (gkCFB_gain != tab:k(222, giNeptrState)) then
            tablew gkCFB_gain, 222, giNeptrState
            kDirty = 1
        endif
        if (gkCFB_tone != tab:k(223, giNeptrState)) then
            tablew gkCFB_tone, 223, giNeptrState
            kDirty = 1
        endif
        if (gkCFB_ceil != tab:k(224, giNeptrState)) then
            tablew gkCFB_ceil, 224, giNeptrState
            kDirty = 1
        endif
        if (gkBLP_bypass != tab:k(225, giNeptrState)) then
            tablew gkBLP_bypass, 225, giNeptrState
            kDirty = 1
        endif
        if (gkBLP_Motion != tab:k(226, giNeptrState)) then
            tablew gkBLP_Motion, 226, giNeptrState
            kDirty = 1
        endif
        if (gkBLP_Scat != tab:k(227, giNeptrState)) then
            tablew gkBLP_Scat, 227, giNeptrState
            kDirty = 1
        endif
        if (gkBLP_Size != tab:k(228, giNeptrState)) then
            tablew gkBLP_Size, 228, giNeptrState
            kDirty = 1
        endif
        if (gkBLP_Char != tab:k(229, giNeptrState)) then
            tablew gkBLP_Char, 229, giNeptrState
            kDirty = 1
        endif
        if (gkONW_bypass != tab:k(230, giNeptrState)) then
            tablew gkONW_bypass, 230, giNeptrState
            kDirty = 1
        endif
        if (gkONW_mix != tab:k(231, giNeptrState)) then
            tablew gkONW_mix, 231, giNeptrState
            kDirty = 1
        endif
        if (gkONW_blend != tab:k(232, giNeptrState)) then
            tablew gkONW_blend, 232, giNeptrState
            kDirty = 1
        endif
        if (gkONW_size != tab:k(233, giNeptrState)) then
            tablew gkONW_size, 233, giNeptrState
            kDirty = 1
        endif
        if (gkONW_follow != tab:k(234, giNeptrState)) then
            tablew gkONW_follow, 234, giNeptrState
            kDirty = 1
        endif
        if (gkONW_density != tab:k(235, giNeptrState)) then
            tablew gkONW_density, 235, giNeptrState
            kDirty = 1
        endif
        if (gkONW_err != tab:k(236, giNeptrState)) then
            tablew gkONW_err, 236, giNeptrState
            kDirty = 1
        endif
        if (gkONW_oct != tab:k(237, giNeptrState)) then
            tablew gkONW_oct, 237, giNeptrState
            kDirty = 1
        endif
        if (gkHabitBypass != tab:k(238, giNeptrState)) then
            tablew gkHabitBypass, 238, giNeptrState
            kDirty = 1
        endif
        if (gkHabitMix != tab:k(239, giNeptrState)) then
            tablew gkHabitMix, 239, giNeptrState
            kDirty = 1
        endif
        if (gkHabitSize != tab:k(240, giNeptrState)) then
            tablew gkHabitSize, 240, giNeptrState
            kDirty = 1
        endif
        if (gkHabitRepeats != tab:k(241, giNeptrState)) then
            tablew gkHabitRepeats, 241, giNeptrState
            kDirty = 1
        endif
        if (gkHabitBlend != tab:k(242, giNeptrState)) then
            tablew gkHabitBlend, 242, giNeptrState
            kDirty = 1
        endif
        if (gkHab_rec != tab:k(243, giNeptrState)) then
            tablew gkHab_rec, 243, giNeptrState
            kDirty = 1
        endif
        if (gkMD2_bypass != tab:k(244, giNeptrState)) then
            tablew gkMD2_bypass, 244, giNeptrState
            kDirty = 1
        endif
        if (gkMD2_Mix != tab:k(245, giNeptrState)) then
            tablew gkMD2_Mix, 245, giNeptrState
            kDirty = 1
        endif
        if (gkMD2_Clock != tab:k(246, giNeptrState)) then
            tablew gkMD2_Clock, 246, giNeptrState
            kDirty = 1
        endif
        if (gkMD2_Length != tab:k(247, giNeptrState)) then
            tablew gkMD2_Length, 247, giNeptrState
            kDirty = 1
        endif
        if (gkMD2_Gen != tab:k(248, giNeptrState)) then
            tablew gkMD2_Gen, 248, giNeptrState
            kDirty = 1
        endif
        if (gkIMP_bypass != tab:k(249, giNeptrState)) then
            tablew gkIMP_bypass, 249, giNeptrState
            kDirty = 1
        endif
        if (gkIMP_mix != tab:k(250, giNeptrState)) then
            tablew gkIMP_mix, 250, giNeptrState
            kDirty = 1
        endif
        if (gkIMP_prob != tab:k(251, giNeptrState)) then
            tablew gkIMP_prob, 251, giNeptrState
            kDirty = 1
        endif
        if (gkIMP_dec != tab:k(252, giNeptrState)) then
            tablew gkIMP_dec, 252, giNeptrState
            kDirty = 1
        endif
        if (gkIMP_tone != tab:k(253, giNeptrState)) then
            tablew gkIMP_tone, 253, giNeptrState
            kDirty = 1
        endif
        if (gkBBcut_bypass != tab:k(254, giNeptrState)) then
            tablew gkBBcut_bypass, 254, giNeptrState
            kDirty = 1
        endif
        if (gkBBcut_drywet != tab:k(255, giNeptrState)) then
            tablew gkBBcut_drywet, 255, giNeptrState
            kDirty = 1
        endif
        if (gkBBcut_prob != tab:k(256, giNeptrState)) then
            tablew gkBBcut_prob, 256, giNeptrState
            kDirty = 1
        endif
        if (gkBBcut_fltMix != tab:k(257, giNeptrState)) then
            tablew gkBBcut_fltMix, 257, giNeptrState
            kDirty = 1
        endif
        if (gkWGuide_bypass != tab:k(258, giNeptrState)) then
            tablew gkWGuide_bypass, 258, giNeptrState
            kDirty = 1
        endif
        if (gkWGuide_drywet != tab:k(259, giNeptrState)) then
            tablew gkWGuide_drywet, 259, giNeptrState
            kDirty = 1
        endif
        if (gkRPT_prob != tab:k(260, giNeptrState)) then
            tablew gkRPT_prob, 260, giNeptrState
            kDirty = 1
        endif
        if (gkRPT_char != tab:k(261, giNeptrState)) then
            tablew gkRPT_char, 261, giNeptrState
            kDirty = 1
        endif
        if (gkRPT_len != tab:k(262, giNeptrState)) then
            tablew gkRPT_len, 262, giNeptrState
            kDirty = 1
        endif
        if (gkSqMod_bypass != tab:k(263, giNeptrState)) then
            tablew gkSqMod_bypass, 263, giNeptrState
            kDirty = 1
        endif
        if (gkSqMod_drywet != tab:k(264, giNeptrState)) then
            tablew gkSqMod_drywet, 264, giNeptrState
            kDirty = 1
        endif
        if (gkCHP_prob != tab:k(265, giNeptrState)) then
            tablew gkCHP_prob, 265, giNeptrState
            kDirty = 1
        endif
        if (gkCHP_rate != tab:k(266, giNeptrState)) then
            tablew gkCHP_rate, 266, giNeptrState
            kDirty = 1
        endif
        if (gkCHP_shape != tab:k(267, giNeptrState)) then
            tablew gkCHP_shape, 267, giNeptrState
            kDirty = 1
        endif
        if (gkFShift_bypass != tab:k(268, giNeptrState)) then
            tablew gkFShift_bypass, 268, giNeptrState
            kDirty = 1
        endif
        if (gkFShift_drywet != tab:k(269, giNeptrState)) then
            tablew gkFShift_drywet, 269, giNeptrState
            kDirty = 1
        endif
        if (gkRVS_prob != tab:k(270, giNeptrState)) then
            tablew gkRVS_prob, 270, giNeptrState
            kDirty = 1
        endif
        if (gkRVS_len != tab:k(271, giNeptrState)) then
            tablew gkRVS_len, 271, giNeptrState
            kDirty = 1
        endif
        if (gkNAMActiveRow != tab:k(272, giNeptrState)) then
            tablew gkNAMActiveRow, 272, giNeptrState
            kDirty = 1
        endif
        if (gkNAMBypass != tab:k(273, giNeptrState)) then
            tablew gkNAMBypass, 273, giNeptrState
            kDirty = 1
        endif
        if (gkNAMInGain != tab:k(274, giNeptrState)) then
            tablew gkNAMInGain, 274, giNeptrState
            kDirty = 1
        endif
        if (gkNAMOutGain != tab:k(275, giNeptrState)) then
            tablew gkNAMOutGain, 275, giNeptrState
            kDirty = 1
        endif
        if (gkNAMRowSel[0] != tab:k(276, giNeptrState)) then
            tablew gkNAMRowSel[0], 276, giNeptrState
            kDirty = 1
        endif
        if (gkNAMRowSel[1] != tab:k(277, giNeptrState)) then
            tablew gkNAMRowSel[1], 277, giNeptrState
            kDirty = 1
        endif
        if (gkNAMRowSel[2] != tab:k(278, giNeptrState)) then
            tablew gkNAMRowSel[2], 278, giNeptrState
            kDirty = 1
        endif
        if (gkNAMRowSel[3] != tab:k(279, giNeptrState)) then
            tablew gkNAMRowSel[3], 279, giNeptrState
            kDirty = 1
        endif
        if (gkNAMRowSel[4] != tab:k(280, giNeptrState)) then
            tablew gkNAMRowSel[4], 280, giNeptrState
            kDirty = 1
        endif
        if (gkIRActiveRow != tab:k(281, giNeptrState)) then
            tablew gkIRActiveRow, 281, giNeptrState
            kDirty = 1
        endif
        if (gkIRBypass != tab:k(282, giNeptrState)) then
            tablew gkIRBypass, 282, giNeptrState
            kDirty = 1
        endif
        if (gkIRDamp != tab:k(283, giNeptrState)) then
            tablew gkIRDamp, 283, giNeptrState
            kDirty = 1
        endif
        if (gkIRLevel != tab:k(284, giNeptrState)) then
            tablew gkIRLevel, 284, giNeptrState
            kDirty = 1
        endif
        if (gkIRSel[0] != tab:k(285, giNeptrState)) then
            tablew gkIRSel[0], 285, giNeptrState
            kDirty = 1
        endif
        if (gkIRRowMix[0] != tab:k(286, giNeptrState)) then
            tablew gkIRRowMix[0], 286, giNeptrState
            kDirty = 1
        endif
        if (gkIRSel[1] != tab:k(287, giNeptrState)) then
            tablew gkIRSel[1], 287, giNeptrState
            kDirty = 1
        endif
        if (gkIRRowMix[1] != tab:k(288, giNeptrState)) then
            tablew gkIRRowMix[1], 288, giNeptrState
            kDirty = 1
        endif
        if (gkIRSel[2] != tab:k(289, giNeptrState)) then
            tablew gkIRSel[2], 289, giNeptrState
            kDirty = 1
        endif
        if (gkIRRowMix[2] != tab:k(290, giNeptrState)) then
            tablew gkIRRowMix[2], 290, giNeptrState
            kDirty = 1
        endif
        if (gkIRSel[3] != tab:k(291, giNeptrState)) then
            tablew gkIRSel[3], 291, giNeptrState
            kDirty = 1
        endif
        if (gkIRRowMix[3] != tab:k(292, giNeptrState)) then
            tablew gkIRRowMix[3], 292, giNeptrState
            kDirty = 1
        endif
        if (gkIRSel[4] != tab:k(293, giNeptrState)) then
            tablew gkIRSel[4], 293, giNeptrState
            kDirty = 1
        endif
        if (gkIRRowMix[4] != tab:k(294, giNeptrState)) then
            tablew gkIRRowMix[4], 294, giNeptrState
            kDirty = 1
        endif
        if (gkStereoIn != tab:k(295, giNeptrState)) then
            tablew gkStereoIn, 295, giNeptrState
            kDirty = 1
        endif
        if (gkMasterVol != tab:k(296, giNeptrState)) then
            tablew gkMasterVol, 296, giNeptrState
            kDirty = 1
        endif
        if (gkLpSpIdx1 != tab:k(297, giNeptrState)) then
            tablew gkLpSpIdx1, 297, giNeptrState
            kDirty = 1
        endif
        if (gkLpSpIdx2 != tab:k(298, giNeptrState)) then
            tablew gkLpSpIdx2, 298, giNeptrState
            kDirty = 1
        endif
        if (gkLpSpIdx3 != tab:k(299, giNeptrState)) then
            tablew gkLpSpIdx3, 299, giNeptrState
            kDirty = 1
        endif
        if (gkLpSpIdx4 != tab:k(300, giNeptrState)) then
            tablew gkLpSpIdx4, 300, giNeptrState
            kDirty = 1
        endif
        if (gkLpSpIdx5 != tab:k(301, giNeptrState)) then
            tablew gkLpSpIdx5, 301, giNeptrState
            kDirty = 1
        endif
        ; UI routing (stored +1; see the restore block)
        if (gkCurrentMenu + 1 != tab:k(302, giNeptrState)) then
            tablew gkCurrentMenu + 1, 302, giNeptrState
            kDirty = 1
        endif
        if (gkLastToggle + 1 != tab:k(303, giNeptrState)) then
            tablew gkLastToggle + 1, 303, giNeptrState
            kDirty = 1
        endif
        if (gkLastToggle29 + 1 != tab:k(304, giNeptrState)) then
            tablew gkLastToggle29 + 1, 304, giNeptrState
            kDirty = 1
        endif
        ; utility page master-out + manual BPM
        if (gkMasterOut != tab:k(305, giNeptrState)) then
            tablew gkMasterOut, 305, giNeptrState
            kDirty = 1
        endif
        if (gkManualBpm != tab:k(306, giNeptrState)) then
            tablew gkManualBpm, 306, giNeptrState
            kDirty = 1
        endif
        if (gkMvdDec != tab:k(307, giNeptrState)) then
            tablew gkMvdDec, 307, giNeptrState
            kDirty = 1
        endif
        if (gkMvdReps != tab:k(308, giNeptrState)) then
            tablew gkMvdReps, 308, giNeptrState
            kDirty = 1
        endif
        ; hyperGuitar lane mirror (2026-10-03): 309..313 = gkHgLaneOn[0..4],
        ; 314..318 = gkHgLaneIdx[0..4], both stored +1 so 0 means "absent"
        ; (presets/files from before this change read 0 and are skipped).
        if (gkHgLaneOn[0] + 1 != tab:k(309, giNeptrState)) then
            tablew gkHgLaneOn[0] + 1, 309, giNeptrState
            kDirty = 1
        endif
        if (gkHgLaneOn[1] + 1 != tab:k(310, giNeptrState)) then
            tablew gkHgLaneOn[1] + 1, 310, giNeptrState
            kDirty = 1
        endif
        if (gkHgLaneOn[2] + 1 != tab:k(311, giNeptrState)) then
            tablew gkHgLaneOn[2] + 1, 311, giNeptrState
            kDirty = 1
        endif
        if (gkHgLaneOn[3] + 1 != tab:k(312, giNeptrState)) then
            tablew gkHgLaneOn[3] + 1, 312, giNeptrState
            kDirty = 1
        endif
        if (gkHgLaneOn[4] + 1 != tab:k(313, giNeptrState)) then
            tablew gkHgLaneOn[4] + 1, 313, giNeptrState
            kDirty = 1
        endif
        if (gkHgLaneIdx[0] + 1 != tab:k(314, giNeptrState)) then
            tablew gkHgLaneIdx[0] + 1, 314, giNeptrState
            kDirty = 1
        endif
        if (gkHgLaneIdx[1] + 1 != tab:k(315, giNeptrState)) then
            tablew gkHgLaneIdx[1] + 1, 315, giNeptrState
            kDirty = 1
        endif
        if (gkHgLaneIdx[2] + 1 != tab:k(316, giNeptrState)) then
            tablew gkHgLaneIdx[2] + 1, 316, giNeptrState
            kDirty = 1
        endif
        if (gkHgLaneIdx[3] + 1 != tab:k(317, giNeptrState)) then
            tablew gkHgLaneIdx[3] + 1, 317, giNeptrState
            kDirty = 1
        endif
        if (gkHgLaneIdx[4] + 1 != tab:k(318, giNeptrState)) then
            tablew gkHgLaneIdx[4] + 1, 318, giNeptrState
            kDirty = 1
        endif
        ; VOCAL chain (2026-10-22): 319..330 (+342 voc_pre_db) = the voc_* chn_k channels of
        ; effects/vocal_chain.orc, in the order voc_on, hpf_hz, mid_db, mid_hz,
        ; mid_q, dly_ms, dly_fb, dly_wet, rev_fb, rev_cut, rev_wet, level.
        ; Stored +100 so 0 means "absent" (older files/presets read 0 and are
        ; skipped; the channel keeps its chn_k default).
        kVoc_on chnget "voc_on"
        if (kVoc_on + 100 != tab:k(319, giNeptrState)) then
            tablew kVoc_on + 100, 319, giNeptrState
            kDirty = 1
        endif
        kVoc_hpf_hz chnget "voc_hpf_hz"
        if (kVoc_hpf_hz + 100 != tab:k(320, giNeptrState)) then
            tablew kVoc_hpf_hz + 100, 320, giNeptrState
            kDirty = 1
        endif
        kVoc_mid_db chnget "voc_mid_db"
        if (kVoc_mid_db + 100 != tab:k(321, giNeptrState)) then
            tablew kVoc_mid_db + 100, 321, giNeptrState
            kDirty = 1
        endif
        kVoc_mid_hz chnget "voc_mid_hz"
        if (kVoc_mid_hz + 100 != tab:k(322, giNeptrState)) then
            tablew kVoc_mid_hz + 100, 322, giNeptrState
            kDirty = 1
        endif
        kVoc_mid_q chnget "voc_mid_q"
        if (kVoc_mid_q + 100 != tab:k(323, giNeptrState)) then
            tablew kVoc_mid_q + 100, 323, giNeptrState
            kDirty = 1
        endif
        kVoc_dly_ms chnget "voc_dly_ms"
        if (kVoc_dly_ms + 100 != tab:k(324, giNeptrState)) then
            tablew kVoc_dly_ms + 100, 324, giNeptrState
            kDirty = 1
        endif
        kVoc_dly_fb chnget "voc_dly_fb"
        if (kVoc_dly_fb + 100 != tab:k(325, giNeptrState)) then
            tablew kVoc_dly_fb + 100, 325, giNeptrState
            kDirty = 1
        endif
        kVoc_dly_wet chnget "voc_dly_wet"
        if (kVoc_dly_wet + 100 != tab:k(326, giNeptrState)) then
            tablew kVoc_dly_wet + 100, 326, giNeptrState
            kDirty = 1
        endif
        kVoc_rev_fb chnget "voc_rev_fb"
        if (kVoc_rev_fb + 100 != tab:k(327, giNeptrState)) then
            tablew kVoc_rev_fb + 100, 327, giNeptrState
            kDirty = 1
        endif
        kVoc_rev_cut chnget "voc_rev_cut"
        if (kVoc_rev_cut + 100 != tab:k(328, giNeptrState)) then
            tablew kVoc_rev_cut + 100, 328, giNeptrState
            kDirty = 1
        endif
        kVoc_rev_wet chnget "voc_rev_wet"
        if (kVoc_rev_wet + 100 != tab:k(329, giNeptrState)) then
            tablew kVoc_rev_wet + 100, 329, giNeptrState
            kDirty = 1
        endif
        kVoc_level chnget "voc_level"
        if (kVoc_level + 100 != tab:k(330, giNeptrState)) then
            tablew kVoc_level + 100, 330, giNeptrState
            kDirty = 1
        endif
        ; MIXER page (2026-10-22): 331..339 (+341 mx_lp_vol) = mx_gtr_vol, mx_gtr_pan, mx_gtr_wid,
        ; mx_tpt_*, mx_pmr_* (mixer_page.orc), stored +100 (0 = absent -> chn_k default).
        ; VOCAL's mixer params are voc_level/voc_dly_wet/voc_rev_wet, slots 330/326/329.
        kMx_gtr_vol chnget "mx_gtr_vol"
        if (kMx_gtr_vol + 100 != tab:k(331, giNeptrState)) then
            tablew kMx_gtr_vol + 100, 331, giNeptrState
            kDirty = 1
        endif
        kMx_gtr_pan chnget "mx_gtr_pan"
        if (kMx_gtr_pan + 100 != tab:k(332, giNeptrState)) then
            tablew kMx_gtr_pan + 100, 332, giNeptrState
            kDirty = 1
        endif
        kMx_gtr_wid chnget "mx_gtr_wid"
        if (kMx_gtr_wid + 100 != tab:k(333, giNeptrState)) then
            tablew kMx_gtr_wid + 100, 333, giNeptrState
            kDirty = 1
        endif
        kMx_tpt_vol chnget "mx_tpt_vol"
        if (kMx_tpt_vol + 100 != tab:k(334, giNeptrState)) then
            tablew kMx_tpt_vol + 100, 334, giNeptrState
            kDirty = 1
        endif
        kMx_tpt_pan chnget "mx_tpt_pan"
        if (kMx_tpt_pan + 100 != tab:k(335, giNeptrState)) then
            tablew kMx_tpt_pan + 100, 335, giNeptrState
            kDirty = 1
        endif
        kMx_tpt_wid chnget "mx_tpt_wid"
        if (kMx_tpt_wid + 100 != tab:k(336, giNeptrState)) then
            tablew kMx_tpt_wid + 100, 336, giNeptrState
            kDirty = 1
        endif
        kMx_pmr_vol chnget "mx_pmr_vol"
        if (kMx_pmr_vol + 100 != tab:k(337, giNeptrState)) then
            tablew kMx_pmr_vol + 100, 337, giNeptrState
            kDirty = 1
        endif
        kMx_pmr_pan chnget "mx_pmr_pan"
        if (kMx_pmr_pan + 100 != tab:k(338, giNeptrState)) then
            tablew kMx_pmr_pan + 100, 338, giNeptrState
            kDirty = 1
        endif
        kMx_pmr_wid chnget "mx_pmr_wid"
        if (kMx_pmr_wid + 100 != tab:k(339, giNeptrState)) then
            tablew kMx_pmr_wid + 100, 339, giNeptrState
            kDirty = 1
        endif
        ; VOCAL mic preamp (2026-10-24): 342 = voc_pre_db, stored +100.
        kVoc_pre_db chnget "voc_pre_db"
        if (kVoc_pre_db + 100 != tab:k(342, giNeptrState)) then
            tablew kVoc_pre_db + 100, 342, giNeptrState
            kDirty = 1
        endif
        ; MIXER looper master (2026-10-24): 341 = mx_lp_vol, stored +100.
        kMx_lp_vol chnget "mx_lp_vol"
        if (kMx_lp_vol + 100 != tab:k(341, giNeptrState)) then
            tablew kMx_lp_vol + 100, 341, giNeptrState
            kDirty = 1
        endif
        ; NAM quality (UTILITY, 2026-10-23): 340 = chn "nam_size", stored +100
        ; (0 = absent = keep the FULL default).
        kNam_size chnget "nam_size"
        if (kNam_size + 100 != tab:k(340, giNeptrState)) then
            tablew kNam_size + 100, 340, giNeptrState
            kDirty = 1
        endif
        ; SHOW-READY STATE (2026-11-02), stored +100 so 0 = absent. BOOT-ONLY
        ; restore (state_persist.orc, after the unpack include) - deliberately NOT
        ; in state_unpack.inc.orc, so a preset recall never rewrites them:
        ;   344 gkDuoFx  345 gkDuoNam  (DUODECIMUS radio state -> re-press toggles
        ;   off correctly after reboot)  346 gkPrLoaded  347 chn voc_gate (x0/x1)
        ;   348 save generation (see boot loader)
        if (gkDuoFx + 100 != tab:k(344, giNeptrState)) then
            tablew gkDuoFx + 100, 344, giNeptrState
            kDirty = 1
        endif
        if (gkDuoNam + 100 != tab:k(345, giNeptrState)) then
            tablew gkDuoNam + 100, 345, giNeptrState
            kDirty = 1
        endif
        if (gkPrLoaded + 100 != tab:k(346, giNeptrState)) then
            tablew gkPrLoaded + 100, 346, giNeptrState
            kDirty = 1
        endif
        kVoc_gate chnget "voc_gate"
        if (kVoc_gate + 100 != tab:k(347, giNeptrState)) then
            tablew kVoc_gate + 100, 347, giNeptrState
            kDirty = 1
        endif
    endif
    ; alternate targets so one truncated write can never take out the only
    ; good copy (see the dual-file note at the top of this instr)
    ; first save after boot goes to the OLDER copy (iAwins: A newer -> B next)
    kSaveAB init (iAwins == 1 ? 0 : 1)
    kStGen init tab_i(348, giNeptrState)
    kSaveA = 0
    kSaveB = 0
    if (kDirty == 1) then
        kStGen += 1
        tablew kStGen, 348, giNeptrState
        kSaveAB = 1 - kSaveAB
        kSaveA = (kSaveAB == 0 ? 1 : 0)
        kSaveB = kSaveAB
    endif
    ftsavek SFile,  kSaveA, 0, giNeptrState
    ftsavek SFileB, kSaveB, 0, giNeptrState

    ; ---- ACTIVATION-ORDER SCAN (every 8th k-cycle, and whenever a recall /
    ; boot asked for a resync, and on capture so the ring is exact) ----
    kSeqTick init 0
    kSeqTick = (kSeqTick + 1) % 8
    if (gkNeptrStLoaded != 0 && (kSeqTick == 0 || gkSeqMode > 0 || gkPresetOp == 1)) then
        PresetScan gkSeqMode
        gkSeqMode = 0
    endif

    ; ---- WEB / OSC VERBS (port 7770, see docs/neptr_web.md) ----
    ;  /fx id on         effect ON (hand toggle-ON semantics) or OFF
    ;  /nam prof in out  NAM amp step (flat profile 0..24, in dB, out dB)
    ;  /nam_byp on       NAM amp bypass (0 = amp on)
    ;  /bank_reload 1    re-read bank + sequences from disk (web saved them)
    kFxId  init 0
    kFxOn  init 0
    kFxGot init 0
    kFxMore init 1
    kFxMore = 1
    while (kFxMore == 1) do
        kFxGot OSClisten giCtlOSC, "/fx", "ii", kFxId, kFxOn
        if (kFxGot == 1) then
            if (kFxOn > 0) then
                PresetApplyFx kFxId
            else
                PresetOffFx kFxId
            endif
            gkExprHandoff = 1
            if (gkLastToggle29 == 1 && gkExprPos >= 0) then
                gkVOLpedal = gkExprPos
            endif
            gkOscRefresh = 1
        else
            kFxMore = 0
        endif
    od
    kNmP  init 0
    kNmI  init 0
    kNmO  init 0
    kNmGot init 0
    kNmMore init 1
    kNmMore = 1
    while (kNmMore == 1) do
        kNmGot OSClisten giCtlOSC, "/nam", "iff", kNmP, kNmI, kNmO
        if (kNmGot == 1) then
            PresetApplyNam kNmP, kNmI, kNmO
        else
            kNmMore = 0
        endif
    od
    kNbV  init 0
    kNbGot init 0
    kNbMore init 1
    kNbMore = 1
    while (kNbMore == 1) do
        kNbGot OSClisten giCtlOSC, "/nam_byp", "i", kNbV
        if (kNbGot == 1) then
            gkNAMBypass = (kNbV > 0 ? 1 : 0)
        else
            kNbMore = 0
        endif
    od
    ; /bank_reload: staged over four k-cycles so the audio thread never does the
    ; file reads and the table copies in one go. The web app writes seq + bank
    ; A/B identical with a higher generation; we read A only (a bad stamp = the
    ; reload is ignored and the in-memory bank stays authoritative).
    kRlV  init 0
    kRlGot init 0
    kRlStage init 0
    kRlTrig = 0
    kRlGot OSClisten giCtlOSC, "/bank_reload", "i", kRlV
    if (kRlGot == 1 && kRlStage == 0 && gkPresetBankLoaded == 1) then
        kRlStage = 1
    endif
    if (kRlStage == 1) then
        tablew 0, $PB_STAMP_I, giSeqTmpBk
        kRlTrig = 1
    endif
    ftloadk SBankA, kRlTrig, 0, giSeqTmpBk
    if (kRlStage == 2) then
        tablew 0, $PB_STAMP_I, giSeqTmpSq
        kRlTrig = 1
    else
        kRlTrig = 0
    endif
    ftloadk SSeqA, kRlTrig, 0, giSeqTmpSq
    if (kRlStage == 3) then
        if (tab:k($PB_STAMP_I, giSeqTmpBk) == $PB_STAMP) then
            kRi = 0
            while (kRi < 8736) do
                tablew tab:k(kRi, giSeqTmpBk), kRi, giPresetBank
                kRi += 1
            od
        endif
    endif
    if (kRlStage == 4) then
        if (tab:k($PB_STAMP_I, giSeqTmpSq) == $SEQ_STAMP) then
            kRi = 0
            while (kRi < 8736) do
                tablew tab:k(kRi, giSeqTmpSq), kRi, giSeqBank
                kRi += 1
            od
        endif
        gkOscRefresh = 1
    endif
    kRlStage = (kRlStage == 0 ? 0 : (kRlStage >= 4 ? 0 : kRlStage + 1))

    ; ---- PRESET BANK capture / delete (op 1 / op 3) ----
    ; CAPTURE: the write-side above forced giNeptrState current this cycle, so
    ; copy the whole 512-pt snapshot into bank slot N and mark it occupied.
    ; DELETE: zero the slot's region and clear its occupancy. Both mark the bank
    ; dirty for the dual-file save below.
    kBankDirty = 0
    ; NAM-ONLY SLOTS (2026-11-02, DUODECIMUS 1/3/5). Op 4 = capture the current
    ; amp (profile, in gain, out gain) as the slot's single NAM AMP step; op 5 =
    ; recall: apply that step only. No row unpack, no guitar sweep, no vocal:
    ; nothing but the amp changes. Bank occupancy is set so the UIs show it.
    if (gkPresetOp == 4 && gkPresetBankLoaded == 1 && gkPresetSlot >= 1 && gkPresetSlot <= 17) then
        kNBase = (gkPresetSlot - 1) * 64
        kNi = 0
        while (kNi < 64) do
            tablew 0, kNBase + kNi, giSeqBank
            kNi += 1
        od
        tablew 3, kNBase, giSeqBank
        tablew gkNAMProfileIdx, kNBase + 1, giSeqBank
        tablew gkNAMInGain, kNBase + 2, giSeqBank
        tablew gkNAMOutGain, kNBase + 3, giSeqBank
        tablew 1, $SEQ_FLAG + gkPresetSlot - 1, giSeqBank
        tablew 1, $PB_OCC + gkPresetSlot - 1, giPresetBank
        gkPrLoaded = gkPresetSlot
        kBankDirty = 1
        gkPresetOp = 0
    elseif (gkPresetOp == 6) then
        PresetSweepOff                    ; guitar radio off (DUODECIMUS re-press)
        gkPresetOp = 0
    elseif (gkPresetOp == 5 && gkPresetBankLoaded == 1 && gkPresetSlot >= 1 && gkPresetSlot <= 17) then
        kNBase = (gkPresetSlot - 1) * 64
        if (tab:k(kNBase, giSeqBank) == 3) then
            PresetApplyNam tab:k(kNBase + 1, giSeqBank), tab:k(kNBase + 2, giSeqBank), tab:k(kNBase + 3, giSeqBank)
            gkPrLoaded = gkPresetSlot
        endif
        gkPresetOp = 0
    endif
    if ((gkPresetOp == 1 || gkPresetOp == 3) && gkPresetBankLoaded == 1) then
        kSlot = gkPresetSlot
        if (kSlot >= 1 && kSlot <= 17) then
            kBase = (kSlot - 1) * $PB_SLOTW
            kCi = 0
            if (gkPresetOp == 1) then
                while (kCi < $PB_SLOTW) do
                    tablew tab:k(kCi, giNeptrState), kBase + kCi, giPresetBank
                    kCi += 1
                od
                tablew 1, $PB_OCC + kSlot - 1, giPresetBank
                PresetSeqCapture kSlot
                gkPrLoaded = kSlot
                ; SOUNDMAGIC rack snapshot (giSMP/giSMon -> parallel banks)
                kSMi = 0
                while (kSMi < 768) do
                    tablew tab:k(kSMi, giSMP), (kSlot - 1) * 768 + kSMi, giSMPBank
                    kSMi += 1
                od
                kSMi = 0
                while (kSMi < 24) do
                    tablew tab:k(kSMi, giSMon), (kSlot - 1) * 24 + kSMi, giSMonBank
                    kSMi += 1
                od
                gkSMBankDirty = 1
            else
                while (kCi < $PB_SLOTW) do
                    tablew 0, kBase + kCi, giPresetBank
                    kCi += 1
                od
                tablew 0, $PB_OCC + kSlot - 1, giPresetBank
                PresetSeqClearSlot kSlot
                if (gkPrLoaded == kSlot) then
                    gkPrLoaded = 0
                endif
                kSMi = 0
                while (kSMi < 24) do
                    tablew 0, (kSlot - 1) * 24 + kSMi, giSMonBank   ; clears the stamp too
                    kSMi += 1
                od
                gkSMBankDirty = 1
            endif
            kBankDirty = 1
        endif
    endif
    ; consume the op each cycle (recall handled above; capture/delete here)
    gkPresetOp = 0
    ; USER PRESETS occupancy bitmask for the UI (onboard slots 13..17 -> bits
    ; 0..4; 2026-11-01 onboard split from DUODECIMUS 1..12), echoed as /pocc.
    ; /ploaded gets the onboard index 1..5 (0 when a DUODECIMUS slot was last).
    if (gkPresetBankLoaded == 1) then
        gkPrOccMask = (tab:k($PB_OCC + 12, giPresetBank) >= 0.5 ? 1 : 0) \
                    + (tab:k($PB_OCC + 13, giPresetBank) >= 0.5 ? 2 : 0) \
                    + (tab:k($PB_OCC + 14, giPresetBank) >= 0.5 ? 4 : 0) \
                    + (tab:k($PB_OCC + 15, giPresetBank) >= 0.5 ? 8 : 0) \
                    + (tab:k($PB_OCC + 16, giPresetBank) >= 0.5 ? 16 : 0)
    endif
    gkPrLoadedUi = (gkPrLoaded >= 13 ? gkPrLoaded - 12 : 0)

    ; bank persistence — dual-file, same anti-truncation dance as the settings
    ; file. Save immediately on a capture/delete so a new/removed preset survives
    ; a power cut without waiting for a metro tick.
    kBSaveAB init 0
    kBSaveA = 0
    kBSaveB = 0
    if (kBankDirty == 1) then
        kBSaveAB = 1 - kBSaveAB
        kBSaveA = (kBSaveAB == 0 ? 1 : 0)
        kBSaveB = kBSaveAB
        ; bump the generation so the loader can tell which file is newer
        kBGen = tab:k($PB_GEN_I, giPresetBank) + 1
        tablew kBGen, $PB_GEN_I, giPresetBank
        tablew tab:k($PB_GEN_I, giSeqBank) + 1, $PB_GEN_I, giSeqBank
    endif
    ftsavek SBankA, kBSaveA, 0, giPresetBank
    ftsavek SBankB, kBSaveB, 0, giPresetBank
    ftsavek SSeqA, kBSaveA, 0, giSeqBank
    ftsavek SSeqB, kBSaveB, 0, giSeqBank
endin

schedule 97, 0, -1

#endif  ; STATE_PERSIST
